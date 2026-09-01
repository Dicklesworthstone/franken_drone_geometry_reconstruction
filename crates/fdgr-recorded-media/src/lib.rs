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
    MediaArtifactError, MediaCustodyError, PublishedMediaInspection, StoredMediaInspection,
    inspect_published_media, publish_stored_media_inspection,
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
        /// Artifact-publication failure.
        source: MediaArtifactError,
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
mod tests {
    #![allow(
        clippy::cast_lossless,
        clippy::cast_possible_truncation,
        clippy::indexing_slicing
    )]

    use super::{
        RecordedMediaIngestError, RecordedMediaIngestOptions, RecordedMediaRoot,
        ingest_recorded_media,
    };
    use fdgr_object_store::LocalObjectStore;
    use std::fs;
    use std::io::Read;
    use std::path::{Path, PathBuf};
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

    fn test_root(label: &str) -> PathBuf {
        let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
        std::env::temp_dir().join(format!(
            "fdgr-recorded-media-{label}-{}-{id}",
            std::process::id()
        ))
    }

    fn write_u32(buffer: &mut [u8], offset: usize, value: u32) {
        buffer[offset..offset + 4].copy_from_slice(&value.to_be_bytes());
    }

    fn make_box(box_type: [u8; 4], payload: &[u8]) -> Vec<u8> {
        let size = 8_u32 + payload.len() as u32;
        let mut output = Vec::with_capacity(size as usize);
        output.extend_from_slice(&size.to_be_bytes());
        output.extend_from_slice(&box_type);
        output.extend_from_slice(payload);
        output
    }

    fn make_container(box_type: [u8; 4], children: &[Vec<u8>]) -> Vec<u8> {
        let payload_length = children.iter().map(Vec::len).sum();
        let mut payload = Vec::with_capacity(payload_length);
        for child in children {
            payload.extend_from_slice(child);
        }
        make_box(box_type, &payload)
    }

    fn classic_file() -> Vec<u8> {
        let mut ftyp_payload = Vec::new();
        ftyp_payload.extend_from_slice(b"isom");
        ftyp_payload.extend_from_slice(&0_u32.to_be_bytes());
        ftyp_payload.extend_from_slice(b"isom");
        let ftyp = make_box(*b"ftyp", &ftyp_payload);
        let mdat = make_box(*b"mdat", &[0_u8; 18]);

        let mut mvhd_payload = vec![0_u8; 20];
        write_u32(&mut mvhd_payload, 12, 1_000);
        write_u32(&mut mvhd_payload, 16, 4_000);
        let mvhd = make_box(*b"mvhd", &mvhd_payload);

        let mut tkhd_payload = vec![0_u8; 84];
        write_u32(&mut tkhd_payload, 12, 1);
        write_u32(&mut tkhd_payload, 76, 1_920_u32 << 16);
        write_u32(&mut tkhd_payload, 80, 1_080_u32 << 16);
        let tkhd = make_box(*b"tkhd", &tkhd_payload);

        let mut mdhd_payload = vec![0_u8; 20];
        write_u32(&mut mdhd_payload, 12, 1_000);
        write_u32(&mut mdhd_payload, 16, 4_000);
        let mdhd = make_box(*b"mdhd", &mdhd_payload);

        let mut hdlr_payload = vec![0_u8; 12];
        hdlr_payload[8..12].copy_from_slice(b"vide");
        let hdlr = make_box(*b"hdlr", &hdlr_payload);

        let mut stsd_payload = vec![0_u8; 8];
        write_u32(&mut stsd_payload, 4, 1);
        stsd_payload.extend_from_slice(&8_u32.to_be_bytes());
        stsd_payload.extend_from_slice(b"avc1");
        let stsd = make_box(*b"stsd", &stsd_payload);

        let mut stts_payload = vec![0_u8; 16];
        write_u32(&mut stts_payload, 4, 1);
        write_u32(&mut stts_payload, 8, 4);
        write_u32(&mut stts_payload, 12, 1_000);
        let stts = make_box(*b"stts", &stts_payload);

        let mut stsz_payload = vec![0_u8; 28];
        write_u32(&mut stsz_payload, 8, 4);
        for (index, size) in [3_u32, 4, 5, 6].into_iter().enumerate() {
            write_u32(&mut stsz_payload, 12 + index * 4, size);
        }
        let stsz = make_box(*b"stsz", &stsz_payload);

        let mut stco_payload = vec![0_u8; 16];
        write_u32(&mut stco_payload, 4, 2);
        write_u32(&mut stco_payload, 8, 28);
        write_u32(&mut stco_payload, 12, 35);
        let stco = make_box(*b"stco", &stco_payload);

        let mut stsc_payload = vec![0_u8; 20];
        write_u32(&mut stsc_payload, 4, 1);
        write_u32(&mut stsc_payload, 8, 1);
        write_u32(&mut stsc_payload, 12, 2);
        write_u32(&mut stsc_payload, 16, 1);
        let stsc = make_box(*b"stsc", &stsc_payload);

        let stbl = make_container(*b"stbl", &[stsd, stts, stsz, stco, stsc]);
        let minf = make_container(*b"minf", &[stbl]);
        let mdia = make_container(*b"mdia", &[mdhd, hdlr, minf]);
        let trak = make_container(*b"trak", &[tkhd, mdia]);
        let moov = make_container(*b"moov", &[mvhd, trak]);

        let mut file = Vec::with_capacity(ftyp.len() + mdat.len() + moov.len());
        file.extend_from_slice(&ftyp);
        file.extend_from_slice(&mdat);
        file.extend_from_slice(&moov);
        file
    }

    fn write_source(path: &Path, bytes: &[u8]) -> bool {
        fs::create_dir_all(path.parent().unwrap_or_else(|| Path::new("."))).is_ok()
            && fs::write(path, bytes).is_ok()
    }

    #[test]
    fn root_codec_round_trips_exact_child_identities() {
        let source = fdgr_object_store::ImportReceipt {
            schema: fdgr_object_store::IMPORT_RECEIPT_SCHEMA,
            object_digest: fdgr_types::EvidenceDigest::from_bytes([1_u8; 32]),
            manifest_digest: fdgr_types::EvidenceDigest::from_bytes([2_u8; 32]),
            object_length: 100,
            chunk_size: 64,
            chunk_count: 2,
            object_created: true,
            manifest_created: true,
            staging_cleanup_complete: true,
            staging_entry: None,
            stage: fdgr_types::PublicationStage::Published,
        };
        let inspection = fdgr_object_store::ImportReceipt {
            schema: fdgr_object_store::IMPORT_RECEIPT_SCHEMA,
            object_digest: fdgr_types::EvidenceDigest::from_bytes([3_u8; 32]),
            manifest_digest: fdgr_types::EvidenceDigest::from_bytes([4_u8; 32]),
            object_length: 200,
            chunk_size: 128,
            chunk_count: 2,
            object_created: true,
            manifest_created: true,
            staging_cleanup_complete: true,
            staging_entry: None,
            stage: fdgr_types::PublicationStage::Published,
        };
        let root = RecordedMediaRoot::from_receipts(&source, &inspection);
        let bytes = root.to_canonical_bytes();
        assert!(matches!(
            bytes,
            Ok(ref encoded)
                if matches!(RecordedMediaRoot::from_canonical_bytes(encoded), Ok(ref decoded) if decoded == &root)
        ));
    }

    #[test]
    fn ingest_publishes_a_verifiable_root_last_graph() {
        let root_path = test_root("success");
        let source_path = root_path.join("source.mp4");
        assert!(write_source(&source_path, &classic_file()));
        let mut store = LocalObjectStore::open(root_path.join("store"));
        assert!(store.is_ok());
        if let Ok(ref mut store) = store {
            let receipt = ingest_recorded_media(
                store,
                &source_path,
                RecordedMediaIngestOptions {
                    source_chunk_size: 16,
                    derived_chunk_size: 64,
                    ..RecordedMediaIngestOptions::default()
                },
            );
            assert!(receipt.is_ok());
            if let Ok(receipt) = receipt {
                assert!(store.verify_manifest(&receipt.source.manifest_digest).is_ok());
                assert!(store
                    .verify_manifest(&receipt.inspection.artifact.manifest_digest)
                    .is_ok());
                assert!(store.verify_manifest(&receipt.root.manifest_digest).is_ok());
                let root_object = store.open_verified_object(&receipt.root.manifest_digest);
                assert!(root_object.is_ok());
                if let Ok(mut root_object) = root_object {
                    let mut bytes = Vec::new();
                    assert!(root_object.read_to_end(&mut bytes).is_ok());
                    assert!(matches!(
                        RecordedMediaRoot::from_canonical_bytes(&bytes),
                        Ok(ref decoded) if decoded == &receipt.root_value()
                    ));
                }
            }
        }
        assert!(fs::remove_dir_all(root_path).is_ok());
    }

    #[test]
    fn inspection_failure_retains_the_durable_source_receipt() {
        let root_path = test_root("invalid-media");
        let source_path = root_path.join("source.bin");
        assert!(write_source(&source_path, b"this is not an ISO BMFF object"));
        let mut store = LocalObjectStore::open(root_path.join("store"));
        assert!(store.is_ok());
        if let Ok(ref mut store) = store {
            let result = ingest_recorded_media(
                store,
                &source_path,
                RecordedMediaIngestOptions {
                    source_chunk_size: 8,
                    derived_chunk_size: 64,
                    ..RecordedMediaIngestOptions::default()
                },
            );
            assert!(matches!(result, Err(RecordedMediaIngestError::Inspection { .. })));
            if let Err(error) = result {
                assert!(error.source_receipt().is_some());
                if let Some(receipt) = error.source_receipt() {
                    assert!(store.verify_manifest(&receipt.manifest_digest).is_ok());
                }
                assert!(error.published_inspection().is_none());
            }
        }
        assert!(fs::remove_dir_all(root_path).is_ok());
    }
}
