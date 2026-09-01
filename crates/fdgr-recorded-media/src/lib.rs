#![forbid(unsafe_code)]
#![allow(
    clippy::large_enum_variant,
    clippy::module_name_repetitions,
    clippy::struct_field_names,
    clippy::too_many_lines
)]
//! Root-last orchestration for exact recorded-media ingest and native inspection.
//!
//! A successful operation publishes three immutable layers in order:
//!
//! 1. the exact source object and its chunk manifest;
//! 2. a canonical native `ISO BMFF` inspection artifact bound to that exact source manifest;
//! 3. a compact recorded-media root that names both verified child publications.
//!
//! Failures after source publication retain the exact durable progress needed for safe retry.

use fdgr_codec::{CodecError, DecodeLimits, Decoder, Encoder};
use fdgr_media::ParseLimits;
use fdgr_media_custody::{
    MediaCustodyError, PublishedMediaInspection, StoredMediaInspection, inspect_published_media,
    publish_stored_media_inspection,
};
use fdgr_object_store::{ImportReceipt, LocalObjectStore, ObjectStoreError};
use fdgr_types::EvidenceDigest;
use std::error::Error;
use std::fmt::{self, Display, Formatter};
use std::path::Path;

/// Public schema identity for the terminal recorded-media ingest receipt.
pub const RECORDED_MEDIA_INGEST_SCHEMA: &str = "fdgr.recorded_media_ingest/1";
/// Public schema identity encoded by the root artifact.
pub const RECORDED_MEDIA_ROOT_SCHEMA: &str = "fdgr.recorded_media_root/1";
/// Default chunk size for large original recordings.
pub const DEFAULT_SOURCE_CHUNK_SIZE: u32 = 4 * 1024 * 1024;
/// Default chunk size for small canonical derived artifacts.
pub const DEFAULT_DERIVED_CHUNK_SIZE: u32 = 64 * 1024;

const ROOT_MAGIC: &[u8] = b"FDGR_RECORDED_MEDIA_ROOT";
const ROOT_VERSION: u16 = 1;
const MAX_ROOT_BYTES: usize = 512;

/// Bounded options for one recorded-media ingest operation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct RecordedMediaIngestOptions {
    /// Nominal chunk size used for the exact original recording.
    pub source_chunk_size: u32,
    /// Nominal chunk size used for inspection and root artifacts.
    pub derived_chunk_size: u32,
    /// Native parser bounds applied to the authenticated stored source.
    pub parse_limits: ParseLimits,
}

impl Default for RecordedMediaIngestOptions {
    fn default() -> Self {
        Self {
            source_chunk_size: DEFAULT_SOURCE_CHUNK_SIZE,
            derived_chunk_size: DEFAULT_DERIVED_CHUNK_SIZE,
            parse_limits: ParseLimits::default(),
        }
    }
}

/// Canonical root joining one original publication to one exact inspection publication.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RecordedMediaRoot {
    /// Original logical-object identity.
    pub source_object_digest: EvidenceDigest,
    /// Original chunk-manifest identity.
    pub source_manifest_digest: EvidenceDigest,
    /// Exact original length.
    pub source_object_length: u64,
    /// Original representation chunk size.
    pub source_chunk_size: u32,
    /// Original representation chunk count.
    pub source_chunk_count: u64,
    /// Canonical inspection-artifact logical-object identity.
    pub inspection_object_digest: EvidenceDigest,
    /// Canonical inspection-artifact chunk-manifest identity.
    pub inspection_manifest_digest: EvidenceDigest,
    /// Exact inspection-artifact length.
    pub inspection_object_length: u64,
    /// Inspection representation chunk size.
    pub inspection_chunk_size: u32,
    /// Inspection representation chunk count.
    pub inspection_chunk_count: u64,
}

impl RecordedMediaRoot {
    /// Constructs a root from two already-published child receipts.
    #[must_use]
    pub fn from_receipts(source: &ImportReceipt, inspection: &ImportReceipt) -> Self {
        Self {
            source_object_digest: source.object_digest.clone(),
            source_manifest_digest: source.manifest_digest.clone(),
            source_object_length: source.object_length,
            source_chunk_size: source.chunk_size,
            source_chunk_count: source.chunk_count,
            inspection_object_digest: inspection.object_digest.clone(),
            inspection_manifest_digest: inspection.manifest_digest.clone(),
            inspection_object_length: inspection.object_length,
            inspection_chunk_size: inspection.chunk_size,
            inspection_chunk_count: inspection.chunk_count,
        }
    }

    /// Encodes the root using the dependency-free canonical codec.
    ///
    /// # Errors
    ///
    /// Returns a canonical length error if the fixed magic cannot be represented.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, CodecError> {
        let mut encoder = Encoder::with_capacity(256);
        encoder.put_bytes(ROOT_MAGIC)?;
        encoder.put_u16(ROOT_VERSION);
        encoder.put_digest(&self.source_object_digest);
        encoder.put_digest(&self.source_manifest_digest);
        encoder.put_u64(self.source_object_length);
        encoder.put_u32(self.source_chunk_size);
        encoder.put_u64(self.source_chunk_count);
        encoder.put_digest(&self.inspection_object_digest);
        encoder.put_digest(&self.inspection_manifest_digest);
        encoder.put_u64(self.inspection_object_length);
        encoder.put_u32(self.inspection_chunk_size);
        encoder.put_u64(self.inspection_chunk_count);
        Ok(encoder.into_bytes())
    }

    /// Decodes and fully consumes one bounded canonical root artifact.
    ///
    /// # Errors
    ///
    /// Returns a typed codec, magic, version, bounds, or trailing-byte error.
    pub fn from_canonical_bytes(bytes: &[u8]) -> Result<Self, RecordedMediaRootError> {
        let mut decoder = Decoder::new(
            bytes,
            DecodeLimits {
                max_total_bytes: MAX_ROOT_BYTES,
                max_blob_bytes: ROOT_MAGIC.len(),
                max_string_bytes: 0,
            },
        )?;
        if decoder.read_bytes()? != ROOT_MAGIC {
            return Err(RecordedMediaRootError::InvalidMagic);
        }
        let version = decoder.read_u16()?;
        if version != ROOT_VERSION {
            return Err(RecordedMediaRootError::UnsupportedVersion { observed: version });
        }
        let root = Self {
            source_object_digest: decoder.read_digest()?,
            source_manifest_digest: decoder.read_digest()?,
            source_object_length: decoder.read_u64()?,
            source_chunk_size: decoder.read_u32()?,
            source_chunk_count: decoder.read_u64()?,
            inspection_object_digest: decoder.read_digest()?,
            inspection_manifest_digest: decoder.read_digest()?,
            inspection_object_length: decoder.read_u64()?,
            inspection_chunk_size: decoder.read_u32()?,
            inspection_chunk_count: decoder.read_u64()?,
        };
        decoder.finish()?;
        Ok(root)
    }
}

/// Terminal receipt for one complete recorded-media publication graph.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RecordedMediaIngestReceipt {
    /// Stable receipt schema identity.
    pub schema: &'static str,
    /// Exact original-object publication.
    pub source: ImportReceipt,
    /// Exact source-bound native inspection publication.
    pub inspection: PublishedMediaInspection,
    /// Root-last artifact joining the two child publications.
    pub root: ImportReceipt,
}

impl RecordedMediaIngestReceipt {
    /// Reconstructs the canonical root value named by this receipt.
    #[must_use]
    pub fn root_value(&self) -> RecordedMediaRoot {
        RecordedMediaRoot::from_receipts(&self.source, &self.inspection.artifact)
    }

    /// Returns the authoritative manifest identity for the complete ingest graph.
    #[must_use]
    pub fn root_manifest_digest(&self) -> &EvidenceDigest {
        &self.root.manifest_digest
    }
}

/// Root-artifact decode failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum RecordedMediaRootError {
    /// Canonical codec failure.
    Codec(CodecError),
    /// Payload magic does not identify an FDGR recorded-media root.
    InvalidMagic,
    /// Payload version is not supported.
    UnsupportedVersion {
        /// Version observed in the payload.
        observed: u16,
    },
}

impl Display for RecordedMediaRootError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Codec(error) => write!(formatter, "recorded-media root codec failure: {error}"),
            Self::InvalidMagic => formatter.write_str("recorded-media root magic is invalid"),
            Self::UnsupportedVersion { observed } => {
                write!(formatter, "recorded-media root version {observed} is unsupported")
            }
        }
    }
}

impl Error for RecordedMediaRootError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::InvalidMagic | Self::UnsupportedVersion { .. } => None,
        }
    }
}

impl From<CodecError> for RecordedMediaRootError {
    fn from(value: CodecError) -> Self {
        Self::Codec(value)
    }
}

/// Ingest failures carrying every child publication already made durable.
#[derive(Debug)]
pub enum RecordedMediaIngestError {
    /// Original publication failed before a source receipt was earned.
    SourceImport(ObjectStoreError),
    /// Original publication succeeded but authenticated native inspection failed.
    Inspection {
        /// Durable source publication that remains valid.
        source_receipt: Box<ImportReceipt>,
        /// Inspection failure.
        source: MediaCustodyError,
    },
    /// Original publication and inspection succeeded, but inspection-artifact publication failed.
    InspectionPublication {
        /// Durable source publication that remains valid.
        source_receipt: Box<ImportReceipt>,
        /// Deterministic inspection that can be retried without reopening the caller path.
        inspection: Box<StoredMediaInspection>,
        /// Underlying immutable-object publication failure.
        source: ObjectStoreError,
    },
    /// Source and inspection artifacts were published, but root encoding failed.
    RootEncoding {
        /// Durable source publication.
        source_receipt: Box<ImportReceipt>,
        /// Durable inspection publication.
        inspection: Box<PublishedMediaInspection>,
        /// Canonical encoding failure.
        source: CodecError,
    },
    /// Source and inspection artifacts were published, but root publication failed or is
    /// indeterminate.
    RootPublication {
        /// Durable source publication.
        source_receipt: Box<ImportReceipt>,
        /// Durable inspection publication.
        inspection: Box<PublishedMediaInspection>,
        /// Exact root value suitable for idempotent retry or lookup.
        root: Box<RecordedMediaRoot>,
        /// Object-store failure.
        source: ObjectStoreError,
    },
}

impl RecordedMediaIngestError {
    /// Returns a durable source receipt when original publication completed before failure.
    #[must_use]
    pub fn source_receipt(&self) -> Option<&ImportReceipt> {
        match self {
            Self::SourceImport(_) => None,
            Self::Inspection { source_receipt, .. }
            | Self::InspectionPublication { source_receipt, .. }
            | Self::RootEncoding { source_receipt, .. }
            | Self::RootPublication { source_receipt, .. } => Some(source_receipt),
        }
    }

    /// Returns a durable inspection publication when it completed before failure.
    #[must_use]
    pub fn published_inspection(&self) -> Option<&PublishedMediaInspection> {
        match self {
            Self::RootEncoding { inspection, .. }
            | Self::RootPublication { inspection, .. } => Some(inspection),
            Self::SourceImport(_)
            | Self::Inspection { .. }
            | Self::InspectionPublication { .. } => None,
        }
    }
}

impl Display for RecordedMediaIngestError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::SourceImport(error) => write!(formatter, "original-media import failed: {error}"),
            Self::Inspection {
                source_receipt,
                source,
            } => write!(
                formatter,
                "source manifest {} was published, but native inspection failed: {source}",
                source_receipt.manifest_digest
            ),
            Self::InspectionPublication {
                source_receipt,
                source,
                ..
            } => write!(
                formatter,
                "source manifest {} was published, but inspection-artifact publication failed: {source}",
                source_receipt.manifest_digest
            ),
            Self::RootEncoding {
                source_receipt,
                source,
                ..
            } => write!(
                formatter,
                "source manifest {} and its inspection were published, but root encoding failed: {source}",
                source_receipt.manifest_digest
            ),
            Self::RootPublication {
                source_receipt,
                source,
                ..
            } => write!(
                formatter,
                "source manifest {} and its inspection were published, but root publication failed or is indeterminate: {source}",
                source_receipt.manifest_digest
            ),
        }
    }
}

impl Error for RecordedMediaIngestError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::SourceImport(error) => Some(error),
            Self::Inspection { source, .. } => Some(source),
            Self::InspectionPublication { source, .. } => Some(source),
            Self::RootEncoding { source, .. } => Some(source),
            Self::RootPublication { source, .. } => Some(source),
        }
    }
}

/// Imports, authenticates, inspects, and root-publishes one exact recorded-media file.
///
/// # Errors
///
/// Returns a progress-carrying error. Any source or inspection receipt exposed by the error remains
/// durable and can be used for idempotent retry, reconciliation, or forensic inspection.
pub fn ingest_recorded_media(
    store: &mut LocalObjectStore,
    source_path: impl AsRef<Path>,
    options: RecordedMediaIngestOptions,
) -> Result<RecordedMediaIngestReceipt, RecordedMediaIngestError> {
    let source = store
        .import_file(source_path, options.source_chunk_size)
        .map_err(RecordedMediaIngestError::SourceImport)?;

    let stored_inspection = match inspect_published_media(
        store,
        &source.manifest_digest,
        options.parse_limits,
    ) {
        Ok(inspection) => inspection,
        Err(error) => {
            return Err(RecordedMediaIngestError::Inspection {
                source_receipt: Box::new(source),
                source: error,
            });
        }
    };

    let inspection = match publish_stored_media_inspection(
        store,
        stored_inspection,
        options.derived_chunk_size,
    ) {
        Ok(inspection) => inspection,
        Err(error) => {
            let (stored, source_error) = error.into_parts();
            return Err(RecordedMediaIngestError::InspectionPublication {
                source_receipt: Box::new(source),
                inspection: Box::new(stored),
                source: source_error,
            });
        }
    };

    let root_value = RecordedMediaRoot::from_receipts(&source, &inspection.artifact);
    let root_bytes = match root_value.to_canonical_bytes() {
        Ok(bytes) => bytes,
        Err(error) => {
            return Err(RecordedMediaIngestError::RootEncoding {
                source_receipt: Box::new(source),
                inspection: Box::new(inspection),
                source: error,
            });
        }
    };
    let root = match store.import_bytes(&root_bytes, options.derived_chunk_size) {
        Ok(receipt) => receipt,
        Err(error) => {
            return Err(RecordedMediaIngestError::RootPublication {
                source_receipt: Box::new(source),
                inspection: Box::new(inspection),
                root: Box::new(root_value),
                source: error,
            });
        }
    };

    Ok(RecordedMediaIngestReceipt {
        schema: RECORDED_MEDIA_INGEST_SCHEMA,
        source,
        inspection,
        root,
    })
}

#[cfg(all(test, unix))]
mod tests;
