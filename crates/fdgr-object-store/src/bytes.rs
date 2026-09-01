#![forbid(unsafe_code)]
//! Root-last publication of canonical in-memory artifacts.

use crate::error::io_error;
use crate::layout::{
    manifest_path, manifests_root, object_path, objects_root, shard_directory, staged_manifest_path,
    staged_object_path, staging_root, sync_directory,
};
use crate::{ImportReceipt, LocalObjectStore, MAX_STAGING_ATTEMPTS, ObjectStoreError};
use fdgr_evidence::{ObjectManifest, encode_manifest};
use fdgr_types::EvidenceDigest;
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

static NEXT_BYTE_STAGE: AtomicU64 = AtomicU64::new(1);

impl LocalObjectStore {
    /// Publishes canonical bytes as one immutable logical object and one representation manifest.
    /// Object visibility precedes manifest-root visibility, and existing matching artifacts are
    /// reused without overwrite.
    ///
    /// # Errors
    ///
    /// Returns a typed manifest, staging, filesystem, collision, verification, or cleanup error.
    pub fn import_bytes(
        &mut self,
        bytes: &[u8],
        chunk_size: u32,
    ) -> Result<ImportReceipt, ObjectStoreError> {
        let manifest = ObjectManifest::build(bytes, chunk_size)?;
        let manifest_bytes = encode_manifest(&manifest)?;
        let staging_directory = allocate_staging_directory(self.root(), &manifest.manifest_digest)?;
        let staging_entry = staging_directory
            .file_name()
            .and_then(|value| value.to_str())
            .ok_or_else(|| ObjectStoreError::InvalidStoreRoot(staging_directory.clone()))?
            .to_owned();
        let staged_object = staged_object_path(&staging_directory);
        let staged_manifest = staged_manifest_path(&staging_directory);
        write_new_synced_file(&staged_object, bytes, "write_staged_byte_object")?;
        manifest.verify_file(&staged_object)?;
        write_new_synced_file(
            &staged_manifest,
            &manifest_bytes,
            "write_staged_byte_manifest",
        )?;
        let staged_manifest_bytes = fs::read(&staged_manifest)
            .map_err(|error| io_error("read_staged_byte_manifest", &staged_manifest, error))?;
        if staged_manifest_bytes != manifest_bytes {
            return Err(ObjectStoreError::ManifestCollision(
                manifest.manifest_digest.clone(),
            ));
        }
        sync_directory(&staging_directory)?;
        let (object_created, manifest_created) = publish_staged_bytes(
            self.root(),
            &manifest,
            &manifest_bytes,
            &staged_object,
            &staged_manifest,
        )?;
        fs::remove_file(&staged_manifest).map_err(|error| {
            io_error(
                "remove_published_staged_byte_manifest",
                &staged_manifest,
                error,
            )
        })?;
        fs::remove_file(&staged_object).map_err(|error| {
            io_error(
                "remove_published_staged_byte_object",
                &staged_object,
                error,
            )
        })?;
        fs::remove_dir(&staging_directory).map_err(|error| {
            io_error(
                "remove_published_byte_staging_directory",
                &staging_directory,
                error,
            )
        })?;
        sync_directory(&staging_root(self.root()))?;
        Ok(ImportReceipt {
            schema: crate::IMPORT_RECEIPT_SCHEMA,
            object_digest: manifest.object_digest,
            manifest_digest: manifest.manifest_digest,
            object_length: manifest.object_length,
            chunk_size: manifest.chunk_size,
            chunk_count: manifest.chunks.len(),
            object_created,
            manifest_created,
            staging_entry: Some(staging_entry),
            staging_cleanup_complete: true,
        })
    }
}

fn allocate_staging_directory(
    root: &Path,
    manifest_digest: &EvidenceDigest,
) -> Result<PathBuf, ObjectStoreError> {
    let staging = staging_root(root);
    for _ in 0..MAX_STAGING_ATTEMPTS {
        let generation = NEXT_BYTE_STAGE.fetch_add(1, Ordering::Relaxed);
        let directory = staging.join(format!(
            "bytes-{}-{}-{generation}",
            manifest_digest,
            std::process::id()
        ));
        match fs::create_dir(&directory) {
            Ok(()) => {
                sync_directory(&staging)?;
                return Ok(directory);
            }
            Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(error) => {
                return Err(io_error(
                    "create_byte_staging_directory",
                    &directory,
                    error,
                ));
            }
        }
    }
    Err(ObjectStoreError::StageIdExhausted)
}

fn write_new_synced_file(
    path: &Path,
    bytes: &[u8],
    operation: &'static str,
) -> Result<(), ObjectStoreError> {
    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(path)
        .map_err(|error| io_error(operation, path, error))?;
    file.write_all(bytes)
        .map_err(|error| io_error(operation, path, error))?;
    file.sync_all()
        .map_err(|error| io_error(operation, path, error))?;
    Ok(())
}

fn publish_staged_bytes(
    root: &Path,
    manifest: &ObjectManifest,
    manifest_bytes: &[u8],
    staged_object: &Path,
    staged_manifest: &Path,
) -> Result<(bool, bool), ObjectStoreError> {
    let object_parent = shard_directory(&objects_root(root), &manifest.object_digest)?;
    let manifest_parent = shard_directory(&manifests_root(root), &manifest.manifest_digest)?;
    create_and_sync_directory(&object_parent)?;
    create_and_sync_directory(&manifest_parent)?;
    let final_object = object_path(root, &manifest.object_digest)?;
    let object_created = publish_object(
        staged_object,
        &final_object,
        manifest,
        &manifest.object_digest,
    )?;
    sync_directory(&object_parent)?;
    manifest.verify_file(&final_object)?;
    let final_manifest = manifest_path(root, &manifest.manifest_digest)?;
    let manifest_created = publish_manifest(
        staged_manifest,
        &final_manifest,
        manifest_bytes,
        &manifest.manifest_digest,
    )?;
    sync_directory(&manifest_parent)?;
    verify_published_manifest(&final_manifest, manifest, manifest_bytes)?;
    manifest.verify_file(&final_object)?;
    Ok((object_created, manifest_created))
}

fn create_and_sync_directory(path: &Path) -> Result<(), ObjectStoreError> {
    match fs::symlink_metadata(path) {
        Ok(metadata) => {
            if metadata.file_type().is_symlink() {
                return Err(ObjectStoreError::SymlinkNotAllowed(path.to_path_buf()));
            }
            if !metadata.is_dir() {
                return Err(ObjectStoreError::InvalidStoreRoot(path.to_path_buf()));
            }
            Ok(())
        }
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => {
            let parent = path
                .parent()
                .ok_or_else(|| ObjectStoreError::InvalidStoreRoot(path.to_path_buf()))?;
            fs::create_dir(path).map_err(|create_error| {
                io_error("create_byte_publish_directory", path, create_error)
            })?;
            sync_directory(parent)
        }
        Err(error) => Err(io_error("inspect_byte_publish_directory", path, error)),
    }
}

fn publish_object(
    source: &Path,
    destination: &Path,
    manifest: &ObjectManifest,
    digest: &EvidenceDigest,
) -> Result<bool, ObjectStoreError> {
    match fs::hard_link(source, destination) {
        Ok(()) => Ok(true),
        Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => {
            verify_existing_object(destination, manifest, digest)?;
            Ok(false)
        }
        Err(error) => Err(io_error("publish_byte_object", destination, error)),
    }
}

fn publish_manifest(
    source: &Path,
    destination: &Path,
    expected_bytes: &[u8],
    digest: &EvidenceDigest,
) -> Result<bool, ObjectStoreError> {
    match fs::hard_link(source, destination) {
        Ok(()) => Ok(true),
        Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => {
            let existing = fs::read(destination).map_err(|read_error| {
                io_error("read_existing_byte_manifest", destination, read_error)
            })?;
            if existing != expected_bytes {
                return Err(ObjectStoreError::ManifestCollision(digest.clone()));
            }
            Ok(false)
        }
        Err(error) => Err(io_error("publish_byte_manifest", destination, error)),
    }
}

fn verify_existing_object(
    path: &Path,
    manifest: &ObjectManifest,
    digest: &EvidenceDigest,
) -> Result<(), ObjectStoreError> {
    let metadata = fs::symlink_metadata(path)
        .map_err(|error| io_error("inspect_existing_byte_object", path, error))?;
    if metadata.file_type().is_symlink() {
        return Err(ObjectStoreError::SymlinkNotAllowed(path.to_path_buf()));
    }
    if !metadata.is_file() || metadata.len() != manifest.object_length {
        return Err(ObjectStoreError::ObjectCollision(digest.clone()));
    }
    manifest
        .verify_file(path)
        .map_err(|_| ObjectStoreError::ObjectCollision(digest.clone()))
}

fn verify_published_manifest(
    path: &Path,
    manifest: &ObjectManifest,
    expected_bytes: &[u8],
) -> Result<(), ObjectStoreError> {
    let bytes = fs::read(path)
        .map_err(|error| io_error("read_published_byte_manifest", path, error))?;
    if bytes != expected_bytes {
        return Err(ObjectStoreError::ManifestCollision(
            manifest.manifest_digest.clone(),
        ));
    }
    let decoded = fdgr_evidence::decode_manifest(&bytes)?;
    if decoded != *manifest {
        return Err(ObjectStoreError::ManifestCollision(
            manifest.manifest_digest.clone(),
        ));
    }
    Ok(())
}

#[cfg(all(test, unix))]
mod tests {
    use crate::LocalObjectStore;
    use std::fs;
    use std::io::Read;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

    fn test_root(label: &str) -> PathBuf {
        let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
        std::env::temp_dir().join(format!(
            "fdgr-byte-import-{label}-{}-{id}",
            std::process::id()
        ))
    }

    #[test]
    fn byte_import_is_verified_and_idempotent() {
        let root = test_root("idempotent");
        let mut store = LocalObjectStore::open(&root);
        assert!(store.is_ok());
        if let Ok(ref mut store) = store {
            let first = store.import_bytes(b"canonical derived bytes", 8);
            assert!(first.is_ok());
            if let Ok(first) = first {
                let second = store.import_bytes(b"canonical derived bytes", 8);
                assert!(matches!(
                    second,
                    Ok(ref receipt)
                        if receipt.object_digest == first.object_digest
                            && receipt.manifest_digest == first.manifest_digest
                            && !receipt.object_created
                            && !receipt.manifest_created
                ));
                let object = store.open_verified_object(&first.manifest_digest);
                assert!(object.is_ok());
                if let Ok(mut object) = object {
                    let mut bytes = Vec::new();
                    assert!(object.read_to_end(&mut bytes).is_ok());
                    assert_eq!(bytes, b"canonical derived bytes");
                }
            }
        }
        assert!(fs::remove_dir_all(root).is_ok());
    }
}
