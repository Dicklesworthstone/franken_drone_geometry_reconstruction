#![forbid(unsafe_code)]
//! Generated store layout and platform durability helpers.

use crate::{MANIFEST_SUFFIX, OBJECT_SUFFIX, ObjectStoreError};
use fdgr_types::EvidenceDigest;
use std::fs::{self, File};
use std::path::{Path, PathBuf};

pub(crate) fn ensure_directory(path: &Path) -> Result<(), ObjectStoreError> {
    fs::create_dir_all(path)
        .map_err(|error| crate::error::io_error("create_dir_all", path, error))?;
    let metadata = fs::symlink_metadata(path)
        .map_err(|error| crate::error::io_error("symlink_metadata", path, error))?;
    if metadata.file_type().is_symlink() {
        return Err(ObjectStoreError::SymlinkNotAllowed(path.to_path_buf()));
    }
    if !metadata.is_dir() {
        return Err(ObjectStoreError::NotDirectory(path.to_path_buf()));
    }
    Ok(())
}

pub(crate) fn digest_path(
    root: &Path,
    category: &str,
    digest: &EvidenceDigest,
    suffix: &str,
) -> Result<PathBuf, ObjectStoreError> {
    let text = digest.as_str();
    let prefix = text
        .get(..2)
        .ok_or_else(|| ObjectStoreError::InvalidDigestPath(digest.clone()))?;
    let remainder = text
        .get(2..)
        .ok_or_else(|| ObjectStoreError::InvalidDigestPath(digest.clone()))?;
    Ok(root
        .join(category)
        .join(prefix)
        .join(format!("{remainder}{suffix}")))
}

pub(crate) fn object_path(
    root: &Path,
    digest: &EvidenceDigest,
) -> Result<PathBuf, ObjectStoreError> {
    digest_path(root, "objects", digest, OBJECT_SUFFIX)
}

pub(crate) fn manifest_path(
    root: &Path,
    digest: &EvidenceDigest,
) -> Result<PathBuf, ObjectStoreError> {
    digest_path(root, "manifests", digest, MANIFEST_SUFFIX)
}

pub(crate) fn sync_directory(path: &Path) -> Result<(), ObjectStoreError> {
    sync_directory_platform(path)
}

#[cfg(unix)]
fn sync_directory_platform(path: &Path) -> Result<(), ObjectStoreError> {
    File::open(path)
        .and_then(|directory| directory.sync_all())
        .map_err(|error| crate::error::io_error("sync_directory", path, error))
}

#[cfg(not(unix))]
fn sync_directory_platform(_path: &Path) -> Result<(), ObjectStoreError> {
    Err(ObjectStoreError::DirectorySyncUnsupported)
}
