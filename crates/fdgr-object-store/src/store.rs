#![forbid(unsafe_code)]
//! Immutable file import, readback, verification, and staging inspection.

use crate::layout::{ensure_directory, manifest_path, object_path, sync_directory};
use crate::{
    IMPORT_RECEIPT_SCHEMA, MANIFEST_SUFFIX, MAX_STAGING_ATTEMPTS, MAX_STAGING_ENTRIES,
    OBJECT_SUFFIX,
};
use crate::{ObjectStoreError, error::io_error};
use fdgr_evidence::{MAX_MANIFEST_BYTES, ObjectManifest, build_file_manifest, verify_file};
use fdgr_types::{EvidenceDigest, PublicationStage};
use std::fs::{self, File, OpenOptions};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};

/// Successful root-last local import receipt.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ImportReceipt {
    /// Stable receipt schema identity.
    pub schema: &'static str,
    /// Complete logical-object identity.
    pub object_digest: EvidenceDigest,
    /// Exact chunk-representation manifest identity.
    pub manifest_digest: EvidenceDigest,
    /// Exact logical byte length.
    pub object_length: u64,
    /// Nominal chunk size.
    pub chunk_size: u32,
    /// Number of chunks in the representation.
    pub chunk_count: u64,
    /// Whether this import created the logical object path.
    pub object_created: bool,
    /// Whether this import published the representation-manifest root.
    pub manifest_created: bool,
    /// Whether all staging artifacts were removed after publication.
    pub staging_cleanup_complete: bool,
    /// Remaining generated staging entry when cleanup is incomplete.
    pub staging_entry: Option<String>,
    /// Terminal publication stage.
    pub stage: PublicationStage,
}

/// One bounded staging-directory observation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct StoreEntryStatus {
    /// Generated staging entry name, not a caller-controlled path.
    pub name: String,
    /// Whether an object temporary file exists.
    pub has_object: bool,
    /// Whether a manifest temporary file exists.
    pub has_manifest: bool,
}

/// Reference immutable filesystem object store.
#[derive(Clone, Debug)]
pub struct LocalObjectStore {
    root: PathBuf,
    next_stage: u64,
}

impl LocalObjectStore {
    /// Creates or opens a generated object-store layout.
    ///
    /// # Errors
    ///
    /// Returns a typed filesystem, symlink, directory, or durability error. This reference
    /// implementation intentionally rejects symlinked store roots and generated components.
    pub fn open(root: impl AsRef<Path>) -> Result<Self, ObjectStoreError> {
        let root = root.as_ref();
        ensure_directory(root)?;
        let root = fs::canonicalize(root)
            .map_err(|error| io_error("canonicalize_store_root", root, error))?;
        for component in ["objects", "manifests", "staging"] {
            let path = root.join(component);
            ensure_directory(&path)?;
        }
        sync_directory(&root)?;
        Ok(Self {
            root,
            next_stage: 0,
        })
    }

    /// Returns the canonicalized store root.
    #[must_use]
    pub fn root(&self) -> &Path {
        &self.root
    }

    /// Imports a regular file with exact-byte verification and root-last publication.
    ///
    /// # Errors
    ///
    /// Returns a typed source, manifest, copy, verification, filesystem, collision, or durability
    /// error. On failure the manifest root is never newly published; staging may remain for
    /// diagnosis. Retry is idempotent by logical-object and manifest identities.
    pub fn import_file(
        &mut self,
        source: impl AsRef<Path>,
        chunk_size: u32,
    ) -> Result<ImportReceipt, ObjectStoreError> {
        let source = source.as_ref();
        validate_source(source)?;
        let manifest = build_file_manifest(source, chunk_size)?;
        let stage = self.allocate_stage(&manifest.manifest_digest)?;
        let staged_object = stage.join(format!("object{OBJECT_SUFFIX}"));
        let staged_manifest = stage.join(format!("manifest{MANIFEST_SUFFIX}"));

        let copied = copy_new_file(source, &staged_object)?;
        if copied != manifest.object_length {
            return Err(ObjectStoreError::CopiedLengthMismatch {
                expected: manifest.object_length,
                observed: copied,
            });
        }
        verify_file(&staged_object, &manifest)?;
        let manifest_bytes = manifest.to_canonical_bytes()?;
        write_new_synced_file(&staged_manifest, &manifest_bytes)?;
        sync_directory(&stage)?;

        let object_destination = object_path(&self.root, &manifest.object_digest)?;
        let manifest_destination = manifest_path(&self.root, &manifest.manifest_digest)?;
        ensure_parent(&object_destination)?;
        ensure_parent(&manifest_destination)?;

        let (object_created, object_stage_removed) =
            publish_object(&staged_object, &object_destination, &manifest)?;
        let (manifest_created, manifest_stage_removed) = publish_manifest(
            &staged_manifest,
            &manifest_destination,
            &manifest,
            &manifest_bytes,
        )?;

        let stage_removed = remove_empty_stage(&stage);
        let staging_synced = sync_directory(&self.root.join("staging")).is_ok();
        let staging_cleanup_complete = object_stage_removed
            && manifest_stage_removed
            && stage_removed
            && staging_synced;
        let staging_entry = if staging_cleanup_complete {
            None
        } else {
            stage
                .file_name()
                .map(|name| name.to_string_lossy().into_owned())
        };
        let chunk_count = u64::try_from(manifest.chunks.len())
            .map_err(|_| fdgr_evidence::ManifestError::LengthOverflow)?;
        Ok(ImportReceipt {
            schema: IMPORT_RECEIPT_SCHEMA,
            object_digest: manifest.object_digest,
            manifest_digest: manifest.manifest_digest,
            object_length: manifest.object_length,
            chunk_size: manifest.chunk_size,
            chunk_count,
            object_created,
            manifest_created,
            staging_cleanup_complete,
            staging_entry,
            stage: PublicationStage::Published,
        })
    }

    /// Loads and authenticates one published representation manifest.
    ///
    /// # Errors
    ///
    /// Returns a not-found, I/O, size, decode, or manifest-identity error.
    pub fn read_manifest(
        &self,
        digest: &EvidenceDigest,
    ) -> Result<ObjectManifest, ObjectStoreError> {
        let path = manifest_path(&self.root, digest)?;
        let bytes = read_bounded(&path, MAX_MANIFEST_BYTES).map_err(|error| match error {
            ReadBoundedError::NotFound => ObjectStoreError::ManifestNotFound(digest.clone()),
            ReadBoundedError::Io(source) => io_error("read_manifest", &path, source),
            ReadBoundedError::Symlink => ObjectStoreError::SymlinkNotAllowed(path.clone()),
            ReadBoundedError::TooLarge => ObjectStoreError::ArtifactTooLarge {
                path: path.clone(),
                maximum: MAX_MANIFEST_BYTES,
            },
        })?;
        let manifest = ObjectManifest::from_canonical_bytes(&bytes)?;
        if &manifest.manifest_digest != digest {
            return Err(ObjectStoreError::ManifestCollision(digest.clone()));
        }
        Ok(manifest)
    }

    /// Verifies a published manifest and its required logical object.
    ///
    /// # Errors
    ///
    /// Returns a manifest/object absence, I/O, structural, or digest error.
    pub fn verify_manifest(&self, digest: &EvidenceDigest) -> Result<(), ObjectStoreError> {
        let manifest = self.read_manifest(digest)?;
        let path = object_path(&self.root, &manifest.object_digest)?;
        verify_published_object(&path, &manifest)
    }

    /// Returns sorted, bounded facts about unfinished staging directories.
    ///
    /// # Errors
    ///
    /// Returns a typed directory-read or metadata error.
    pub fn staging_entries(&self) -> Result<Vec<StoreEntryStatus>, ObjectStoreError> {
        let staging = self.root.join("staging");
        let entries = fs::read_dir(&staging)
            .map_err(|error| io_error("read_staging", &staging, error))?;
        let mut statuses = Vec::new();
        for entry in entries {
            let entry = entry.map_err(|error| io_error("read_staging_entry", &staging, error))?;
            let path = entry.path();
            let metadata = fs::symlink_metadata(&path)
                .map_err(|error| io_error("staging_metadata", &path, error))?;
            if metadata.file_type().is_symlink() {
                return Err(ObjectStoreError::SymlinkNotAllowed(path));
            }
            if !metadata.is_dir() {
                continue;
            }
            let name = entry.file_name().to_string_lossy().into_owned();
            if statuses.len() >= MAX_STAGING_ENTRIES {
                return Err(ObjectStoreError::TooManyStagingEntries {
                    maximum: MAX_STAGING_ENTRIES,
                });
            }
            statuses.push(StoreEntryStatus {
                has_object: path.join(format!("object{OBJECT_SUFFIX}")).is_file(),
                has_manifest: path.join(format!("manifest{MANIFEST_SUFFIX}")).is_file(),
                name,
            });
        }
        statuses.sort_by(|left, right| left.name.cmp(&right.name));
        Ok(statuses)
    }

    fn allocate_stage(
        &mut self,
        manifest_digest: &EvidenceDigest,
    ) -> Result<PathBuf, ObjectStoreError> {
        let process = std::process::id();
        for _ in 0..MAX_STAGING_ATTEMPTS {
            let counter = self.next_stage;
            self.next_stage = self.next_stage.wrapping_add(1);
            let name = format!("{}-{process}-{counter}", manifest_digest.as_str());
            let path = self.root.join("staging").join(name);
            match fs::create_dir(&path) {
                Ok(()) => {
                    sync_directory(&self.root.join("staging"))?;
                    return Ok(path);
                }
                Err(error) if error.kind() == io::ErrorKind::AlreadyExists => {}
                Err(error) => return Err(io_error("create_staging", path, error)),
            }
        }
        Err(ObjectStoreError::StagingExhausted)
    }
}

fn validate_source(source: &Path) -> Result<(), ObjectStoreError> {
    let metadata = fs::symlink_metadata(source)
        .map_err(|error| io_error("source_metadata", source, error))?;
    if metadata.file_type().is_symlink() {
        return Err(ObjectStoreError::SourceSymlink(source.to_path_buf()));
    }
    if !metadata.is_file() {
        return Err(ObjectStoreError::SourceNotRegular(source.to_path_buf()));
    }
    Ok(())
}

fn ensure_parent(path: &Path) -> Result<(), ObjectStoreError> {
    let parent = path
        .parent()
        .ok_or_else(|| ObjectStoreError::NotDirectory(path.to_path_buf()))?;
    ensure_directory(parent)?;
    if let Some(grandparent) = parent.parent() {
        sync_directory(grandparent)?;
    }
    Ok(())
}

fn copy_new_file(source: &Path, destination: &Path) -> Result<u64, ObjectStoreError> {
    let mut input =
        File::open(source).map_err(|error| io_error("open_import_source", source, error))?;
    let mut output = OpenOptions::new()
        .create_new(true)
        .write(true)
        .open(destination)
        .map_err(|error| io_error("create_staged_object", destination, error))?;
    let copied = io::copy(&mut input, &mut output)
        .map_err(|error| io_error("copy_staged_object", destination, error))?;
    output
        .flush()
        .and_then(|()| output.sync_all())
        .map_err(|error| io_error("sync_staged_object", destination, error))?;
    Ok(copied)
}

fn write_new_synced_file(path: &Path, bytes: &[u8]) -> Result<(), ObjectStoreError> {
    let mut file = OpenOptions::new()
        .create_new(true)
        .write(true)
        .open(path)
        .map_err(|error| io_error("create_staged_manifest", path, error))?;
    file.write_all(bytes)
        .and_then(|()| file.flush())
        .and_then(|()| file.sync_all())
        .map_err(|error| io_error("sync_staged_manifest", path, error))
}

fn verify_published_object(
    path: &Path,
    manifest: &ObjectManifest,
) -> Result<(), ObjectStoreError> {
    let metadata = match fs::symlink_metadata(path) {
        Ok(metadata) => metadata,
        Err(error) if error.kind() == io::ErrorKind::NotFound => {
            return Err(ObjectStoreError::ObjectNotFound(
                manifest.object_digest.clone(),
            ));
        }
        Err(error) => return Err(io_error("published_object_metadata", path, error)),
    };
    if metadata.file_type().is_symlink() {
        return Err(ObjectStoreError::SymlinkNotAllowed(path.to_path_buf()));
    }
    if !metadata.is_file() {
        return Err(ObjectStoreError::ObjectCollision(
            manifest.object_digest.clone(),
        ));
    }
    verify_file(path, manifest).map_err(ObjectStoreError::from)
}

fn publish_object(
    staged: &Path,
    destination: &Path,
    manifest: &ObjectManifest,
) -> Result<(bool, bool), ObjectStoreError> {
    match fs::hard_link(staged, destination) {
        Ok(()) => {
            sync_parent(destination)?;
            let removed = fs::remove_file(staged).is_ok();
            Ok((true, removed))
        }
        Err(error) if error.kind() == io::ErrorKind::AlreadyExists => {
            verify_published_object(destination, manifest)?;
            let removed = fs::remove_file(staged).is_ok();
            Ok((false, removed))
        }
        Err(error) => Err(io_error("publish_object", destination, error)),
    }
}

fn publish_manifest(
    staged: &Path,
    destination: &Path,
    manifest: &ObjectManifest,
    canonical_bytes: &[u8],
) -> Result<(bool, bool), ObjectStoreError> {
    match fs::hard_link(staged, destination) {
        Ok(()) => {
            if let Err(error) = sync_parent(destination) {
                let source = match error {
                    ObjectStoreError::Io { source, .. } => source,
                    other => return Err(other),
                };
                return Err(ObjectStoreError::PublicationIndeterminate {
                    manifest_digest: manifest.manifest_digest.clone(),
                    operation: "sync_manifest_parent",
                    path: destination.to_path_buf(),
                    source,
                });
            }
            let removed = fs::remove_file(staged).is_ok();
            Ok((true, removed))
        }
        Err(error) if error.kind() == io::ErrorKind::AlreadyExists => {
            let existing = read_bounded(destination, MAX_MANIFEST_BYTES).map_err(|failure| {
                match failure {
                    ReadBoundedError::NotFound => io_error(
                        "read_existing_manifest",
                        destination,
                        io::Error::from(io::ErrorKind::NotFound),
                    ),
                    ReadBoundedError::Io(source) => {
                        io_error("read_existing_manifest", destination, source)
                    }
                    ReadBoundedError::Symlink => {
                        ObjectStoreError::SymlinkNotAllowed(destination.to_path_buf())
                    }
                    ReadBoundedError::TooLarge => {
                        ObjectStoreError::ManifestCollision(manifest.manifest_digest.clone())
                    }
                }
            })?;
            if existing.as_slice() != canonical_bytes {
                return Err(ObjectStoreError::ManifestCollision(
                    manifest.manifest_digest.clone(),
                ));
            }
            let removed = fs::remove_file(staged).is_ok();
            Ok((false, removed))
        }
        Err(error) => Err(io_error("publish_manifest", destination, error)),
    }
}

fn sync_parent(path: &Path) -> Result<(), ObjectStoreError> {
    let parent = path
        .parent()
        .ok_or_else(|| ObjectStoreError::NotDirectory(path.to_path_buf()))?;
    sync_directory(parent)
}

fn remove_empty_stage(stage: &Path) -> bool {
    fs::remove_dir(stage).is_ok()
}

enum ReadBoundedError {
    NotFound,
    Io(io::Error),
    Symlink,
    TooLarge,
}

fn read_bounded(path: &Path, maximum: usize) -> Result<Vec<u8>, ReadBoundedError> {
    let metadata = match fs::symlink_metadata(path) {
        Ok(metadata) => metadata,
        Err(error) if error.kind() == io::ErrorKind::NotFound => {
            return Err(ReadBoundedError::NotFound);
        }
        Err(error) => return Err(ReadBoundedError::Io(error)),
    };
    if metadata.file_type().is_symlink() {
        return Err(ReadBoundedError::Symlink);
    }
    let length = usize::try_from(metadata.len()).map_err(|_| ReadBoundedError::TooLarge)?;
    if length > maximum {
        return Err(ReadBoundedError::TooLarge);
    }
    let mut file = File::open(path).map_err(ReadBoundedError::Io)?;
    let mut bytes = Vec::with_capacity(length);
    file.read_to_end(&mut bytes).map_err(ReadBoundedError::Io)?;
    if bytes.len() > maximum {
        return Err(ReadBoundedError::TooLarge);
    }
    Ok(bytes)
}

#[cfg(all(test, unix))]
mod tests {
    use super::LocalObjectStore;
    use crate::ObjectStoreError;
    use std::fs;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

    fn test_root(label: &str) -> PathBuf {
        let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
        std::env::temp_dir().join(format!(
            "fdgr-store-{label}-{}-{id}",
            std::process::id()
        ))
    }

    fn prepare(label: &str, bytes: &[u8]) -> Option<(PathBuf, PathBuf)> {
        let root = test_root(label);
        if fs::create_dir_all(&root).is_err() {
            return None;
        }
        let source = root.join("source.bin");
        if fs::write(&source, bytes).is_err() {
            return None;
        }
        Some((root, source))
    }

    #[test]
    fn import_is_root_last_and_idempotent() {
        let prepared = prepare("idempotent", b"immutable original media bytes");
        assert!(prepared.is_some());
        if let Some((root, source)) = prepared {
            let store_root = root.join("store");
            let store = LocalObjectStore::open(&store_root);
            assert!(store.is_ok());
            if let Ok(mut store) = store {
                let first = store.import_file(&source, 4);
                assert!(matches!(
                    first,
                    Ok(ref receipt) if receipt.object_created && receipt.manifest_created
                ));
                if let Ok(first) = first {
                    assert!(store.verify_manifest(&first.manifest_digest).is_ok());
                    let second = store.import_file(&source, 4);
                    assert!(matches!(
                        second,
                        Ok(ref receipt)
                            if !receipt.object_created && !receipt.manifest_created
                    ));
                    assert!(matches!(
                        store.staging_entries(),
                        Ok(ref entries) if entries.is_empty()
                    ));
                }
            }
            assert!(fs::remove_dir_all(root).is_ok());
        }
    }

    #[test]
    fn one_logical_object_accepts_multiple_chunk_representations() {
        let prepared = prepare("representations", b"same logical object different chunking");
        assert!(prepared.is_some());
        if let Some((root, source)) = prepared {
            let store = LocalObjectStore::open(root.join("store"));
            assert!(store.is_ok());
            if let Ok(mut store) = store {
                let first = store.import_file(&source, 4);
                let second = store.import_file(&source, 7);
                assert!(first.is_ok());
                assert!(second.is_ok());
                if let (Ok(first), Ok(second)) = (first, second) {
                    assert_eq!(first.object_digest, second.object_digest);
                    assert_ne!(first.manifest_digest, second.manifest_digest);
                    assert!(!second.object_created && second.manifest_created);
                    assert!(store.verify_manifest(&first.manifest_digest).is_ok());
                    assert!(store.verify_manifest(&second.manifest_digest).is_ok());
                }
            }
            assert!(fs::remove_dir_all(root).is_ok());
        }
    }

    #[test]
    fn published_corruption_is_detected() {
        let prepared = prepare("corruption", b"original");
        assert!(prepared.is_some());
        if let Some((root, source)) = prepared {
            let store = LocalObjectStore::open(root.join("store"));
            assert!(store.is_ok());
            if let Ok(mut store) = store {
                let receipt = store.import_file(&source, 4);
                assert!(receipt.is_ok());
                if let Ok(receipt) = receipt {
                    let object = crate::layout::object_path(store.root(), &receipt.object_digest);
                    assert!(object.is_ok());
                    if let Ok(object) = object {
                        assert!(fs::write(&object, b"mutated!").is_ok());
                        assert!(matches!(
                            store.verify_manifest(&receipt.manifest_digest),
                            Err(ObjectStoreError::Manifest(_))
                        ));
                    }
                }
            }
            assert!(fs::remove_dir_all(root).is_ok());
        }
    }
}
