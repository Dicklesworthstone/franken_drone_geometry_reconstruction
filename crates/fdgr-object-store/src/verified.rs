#![forbid(unsafe_code)]
//! Verified read leases over immutable published objects.

use crate::error::io_error;
use crate::layout::object_path;
use crate::{LocalObjectStore, ObjectStoreError};
use fdgr_evidence::ObjectManifest;
use fdgr_types::EvidenceDigest;
use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom};

/// An authenticated manifest paired with the exact open object handle it verified.
///
/// The file is rewound before construction. Replacing the directory entry after this object is
/// returned does not retarget the open handle. The reference store still does not claim hostile
/// race-free path confinement; that remains a FrankenFS admission requirement.
#[derive(Debug)]
pub struct VerifiedObject {
    manifest: ObjectManifest,
    file: File,
}

impl VerifiedObject {
    /// Returns the authenticated representation manifest.
    #[must_use]
    pub const fn manifest(&self) -> &ObjectManifest {
        &self.manifest
    }

    /// Returns the logical immutable object identity.
    #[must_use]
    pub fn object_digest(&self) -> &EvidenceDigest {
        &self.manifest.object_digest
    }

    /// Returns the exact authenticated byte length.
    #[must_use]
    pub const fn object_length(&self) -> u64 {
        self.manifest.object_length
    }

    /// Consumes the lease and returns its manifest and open file.
    #[must_use]
    pub fn into_parts(self) -> (ObjectManifest, File) {
        (self.manifest, self.file)
    }
}

impl Read for VerifiedObject {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        self.file.read(buffer)
    }
}

impl Seek for VerifiedObject {
    fn seek(&mut self, position: SeekFrom) -> std::io::Result<u64> {
        self.file.seek(position)
    }
}

impl LocalObjectStore {
    /// Opens one published object only after authenticating its manifest, complete logical digest,
    /// ordered chunk digests, and exact length through the same file handle returned to the caller.
    ///
    /// # Errors
    ///
    /// Returns a typed missing-artifact, symlink, non-file, I/O, manifest, length, chunk, or object
    /// identity error. The handle is never returned after partial verification.
    pub fn open_verified_object(
        &self,
        manifest_digest: &EvidenceDigest,
    ) -> Result<VerifiedObject, ObjectStoreError> {
        let manifest = self.read_manifest(manifest_digest)?;
        let path = object_path(self.root(), &manifest.object_digest)?;
        let metadata = match fs::symlink_metadata(&path) {
            Ok(metadata) => metadata,
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => {
                return Err(ObjectStoreError::ObjectNotFound(
                    manifest.object_digest.clone(),
                ));
            }
            Err(error) => return Err(io_error("verified_object_metadata", &path, error)),
        };
        if metadata.file_type().is_symlink() {
            return Err(ObjectStoreError::SymlinkNotAllowed(path));
        }
        if !metadata.is_file() {
            return Err(ObjectStoreError::ObjectCollision(
                manifest.object_digest.clone(),
            ));
        }
        if metadata.len() != manifest.object_length {
            return Err(fdgr_evidence::ManifestError::ObjectLengthMismatch {
                expected: manifest.object_length,
                observed: metadata.len(),
            }
            .into());
        }
        let mut file = File::open(&path)
            .map_err(|error| io_error("open_verified_object", &path, error))?;
        let opened_metadata = file
            .metadata()
            .map_err(|error| io_error("verified_object_open_metadata", &path, error))?;
        if !opened_metadata.is_file() {
            return Err(ObjectStoreError::ObjectCollision(
                manifest.object_digest.clone(),
            ));
        }
        if opened_metadata.len() != manifest.object_length {
            return Err(fdgr_evidence::ManifestError::ObjectLengthMismatch {
                expected: manifest.object_length,
                observed: opened_metadata.len(),
            }
            .into());
        }
        manifest.verify_reader(&mut file)?;
        file.seek(SeekFrom::Start(0))
            .map_err(|error| io_error("rewind_verified_object", &path, error))?;
        Ok(VerifiedObject { manifest, file })
    }
}

#[cfg(all(test, unix))]
mod tests {
    use super::VerifiedObject;
    use crate::layout::object_path;
    use crate::{LocalObjectStore, ObjectStoreError};
    use fdgr_evidence::ManifestError;
    use std::fs;
    use std::io::Read;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

    fn test_root(label: &str) -> PathBuf {
        let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
        std::env::temp_dir().join(format!(
            "fdgr-verified-object-{label}-{}-{id}",
            std::process::id()
        ))
    }

    fn import_fixture(label: &str, bytes: &[u8]) -> Option<(PathBuf, LocalObjectStore, crate::ImportReceipt)> {
        let root = test_root(label);
        if fs::create_dir_all(&root).is_err() {
            return None;
        }
        let source = root.join("source.bin");
        if fs::write(&source, bytes).is_err() {
            let _ = fs::remove_dir_all(&root);
            return None;
        }
        let mut store = match LocalObjectStore::open(root.join("store")) {
            Ok(value) => value,
            Err(_) => {
                let _ = fs::remove_dir_all(&root);
                return None;
            }
        };
        let receipt = match store.import_file(&source, 4) {
            Ok(value) => value,
            Err(_) => {
                let _ = fs::remove_dir_all(&root);
                return None;
            }
        };
        Some((root, store, receipt))
    }

    fn read_all(mut object: VerifiedObject) -> std::io::Result<Vec<u8>> {
        let mut bytes = Vec::new();
        object.read_to_end(&mut bytes)?;
        Ok(bytes)
    }

    #[test]
    fn verified_reader_is_rewound_after_authentication() {
        let bytes = b"immutable object bytes";
        let prepared = import_fixture("read", bytes);
        assert!(prepared.is_some());
        if let Some((root, store, receipt)) = prepared {
            let object = store.open_verified_object(&receipt.manifest_digest);
            assert!(matches!(
                object,
                Ok(ref value)
                    if value.object_digest() == &receipt.object_digest
                        && value.object_length() == receipt.object_length
            ));
            if let Ok(object) = object {
                assert!(matches!(read_all(object), Ok(ref observed) if observed == bytes));
            }
            assert!(fs::remove_dir_all(root).is_ok());
        }
    }

    #[test]
    fn tampered_published_bytes_are_never_returned() {
        let prepared = import_fixture("tamper", b"original evidence");
        assert!(prepared.is_some());
        if let Some((root, store, receipt)) = prepared {
            let path = object_path(store.root(), &receipt.object_digest);
            assert!(path.is_ok());
            if let Ok(path) = path {
                assert!(fs::write(path, b"tampered evidence").is_ok());
            }
            assert!(matches!(
                store.open_verified_object(&receipt.manifest_digest),
                Err(ObjectStoreError::Manifest(
                    ManifestError::ObjectLengthMismatch { .. }
                        | ManifestError::ChunkDigestMismatch { .. }
                        | ManifestError::ObjectDigestMismatch { .. }
                ))
            ));
            assert!(fs::remove_dir_all(root).is_ok());
        }
    }
}
