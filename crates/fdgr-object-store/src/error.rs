#![forbid(unsafe_code)]
//! Stable local object-store errors.

use fdgr_evidence::ManifestError;
use fdgr_types::EvidenceDigest;
use std::error::Error;
use std::fmt::{self, Display, Formatter};
use std::io;
use std::path::PathBuf;

/// Stable local object-store failures.
#[derive(Debug)]
pub enum ObjectStoreError {
    /// Object-manifest construction or verification failed.
    Manifest(ManifestError),
    /// A filesystem operation failed.
    Io {
        /// Stable operation name.
        operation: &'static str,
        /// Path at which the operation failed.
        path: PathBuf,
        /// Underlying operating-system error.
        source: io::Error,
    },
    /// Store root or generated directory is a symlink.
    SymlinkNotAllowed(PathBuf),
    /// Store root or generated component is not a directory.
    NotDirectory(PathBuf),
    /// Import source is a symlink.
    SourceSymlink(PathBuf),
    /// Import source is not a regular file.
    SourceNotRegular(PathBuf),
    /// The staged copy length differs from the source manifest.
    CopiedLengthMismatch {
        /// Manifest byte length.
        expected: u64,
        /// Copied byte length.
        observed: u64,
    },
    /// A bounded store artifact exceeded its hard byte limit.
    ArtifactTooLarge {
        /// Artifact path.
        path: PathBuf,
        /// Maximum accepted bytes.
        maximum: usize,
    },
    /// Diagnostic staging cardinality exceeded its hard limit.
    TooManyStagingEntries {
        /// Maximum accepted entries.
        maximum: usize,
    },
    /// A manifest root may be visible but its durability outcome is unresolved.
    PublicationIndeterminate {
        /// Manifest identity whose publication may have happened.
        manifest_digest: EvidenceDigest,
        /// Filesystem operation that failed after publication became possible.
        operation: &'static str,
        /// Path involved in the unresolved operation.
        path: PathBuf,
        /// Underlying operating-system error.
        source: io::Error,
    },
    /// A content-addressed object path existed with incompatible bytes.
    ObjectCollision(EvidenceDigest),
    /// A content-addressed manifest path existed with incompatible canonical bytes.
    ManifestCollision(EvidenceDigest),
    /// A requested manifest is not published.
    ManifestNotFound(EvidenceDigest),
    /// A manifest root exists but its required object is missing.
    ObjectNotFound(EvidenceDigest),
    /// Staging namespace could not allocate a bounded unique directory.
    StagingExhausted,
    /// Directory durability is unavailable on this target.
    DirectorySyncUnsupported,
    /// A digest-derived path could not be formed despite canonical input.
    InvalidDigestPath(EvidenceDigest),
}

impl Display for ObjectStoreError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Manifest(error) => write!(formatter, "manifest error: {error}"),
            Self::Io {
                operation,
                path,
                source,
            } => write!(
                formatter,
                "filesystem operation {operation} failed at {}: {source}",
                path.display()
            ),
            Self::SymlinkNotAllowed(path) => {
                write!(formatter, "symlink is not allowed at {}", path.display())
            }
            Self::NotDirectory(path) => {
                write!(formatter, "store component is not a directory: {}", path.display())
            }
            Self::SourceSymlink(path) => {
                write!(formatter, "import source must not be a symlink: {}", path.display())
            }
            Self::SourceNotRegular(path) => {
                write!(formatter, "import source is not a regular file: {}", path.display())
            }
            Self::CopiedLengthMismatch { expected, observed } => write!(
                formatter,
                "staged copy length mismatch: expected {expected} bytes, observed {observed}"
            ),
            Self::ArtifactTooLarge { path, maximum } => write!(
                formatter,
                "store artifact {} exceeds maximum {maximum} bytes",
                path.display()
            ),
            Self::TooManyStagingEntries { maximum } => write!(
                formatter,
                "staging scan exceeds maximum {maximum} entries"
            ),
            Self::PublicationIndeterminate {
                manifest_digest,
                operation,
                path,
                source,
            } => write!(
                formatter,
                "manifest {manifest_digest} publication is indeterminate after {operation} failed at {}: {source}",
                path.display()
            ),
            Self::ObjectCollision(digest) => {
                write!(formatter, "logical object collision at digest {digest}")
            }
            Self::ManifestCollision(digest) => {
                write!(formatter, "representation manifest collision at digest {digest}")
            }
            Self::ManifestNotFound(digest) => {
                write!(formatter, "published manifest {digest} was not found")
            }
            Self::ObjectNotFound(digest) => {
                write!(formatter, "published object {digest} was not found")
            }
            Self::StagingExhausted => {
                formatter.write_str("could not allocate a bounded unique staging directory")
            }
            Self::DirectorySyncUnsupported => formatter.write_str(
                "directory durability is unsupported on this target; publication was not claimed",
            ),
            Self::InvalidDigestPath(digest) => {
                write!(formatter, "cannot derive a store path from digest {digest}")
            }
        }
    }
}

impl Error for ObjectStoreError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Manifest(error) => Some(error),
            Self::Io { source, .. } | Self::PublicationIndeterminate { source, .. } => {
                Some(source)
            }
            _ => None,
        }
    }
}

impl From<ManifestError> for ObjectStoreError {
    fn from(error: ManifestError) -> Self {
        Self::Manifest(error)
    }
}

pub(crate) fn io_error(
    operation: &'static str,
    path: impl Into<PathBuf>,
    source: io::Error,
) -> ObjectStoreError {
    ObjectStoreError::Io {
        operation,
        path: path.into(),
        source,
    }
}
