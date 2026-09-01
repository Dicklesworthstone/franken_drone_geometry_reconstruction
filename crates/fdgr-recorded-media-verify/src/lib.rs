#![forbid(unsafe_code)]
#![allow(
    clippy::large_enum_variant,
    clippy::module_name_repetitions,
    clippy::struct_field_names,
    clippy::too_many_lines
)]
//! Independent readback verification for recorded-media publication graphs.
//!
//! The verifier begins from the root manifest, authenticates and decodes the root object, verifies
//! both child manifests, authenticates the inspection artifact, and proves that every basis and
//! representation field agrees. It never trusts a producer receipt by itself.

use fdgr_codec::DecodeLimits;
use fdgr_media_custody::{
    MediaArtifactDecodeError, StoredMediaInspection, decode_media_inspection_artifact,
};
use fdgr_object_store::{LocalObjectStore, ObjectStoreError};
use fdgr_recorded_media::{RecordedMediaRoot, RecordedMediaRootError};
use fdgr_types::EvidenceDigest;
use std::error::Error;
use std::fmt::{self, Display, Formatter};
use std::io::Read;

/// Public schema identity for an independently verified recorded-media graph.
pub const VERIFIED_RECORDED_MEDIA_SCHEMA: &str = "fdgr.verified_recorded_media/1";
/// Maximum accepted root-artifact length.
pub const MAX_ROOT_ARTIFACT_BYTES: u64 = 512;
/// Maximum accepted canonical inspection-artifact length.
pub const MAX_INSPECTION_ARTIFACT_BYTES: u64 = 16 * 1024 * 1024;

/// Independently reconstructed and verified recorded-media graph.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct VerifiedRecordedMedia {
    /// Stable result schema identity.
    pub schema: &'static str,
    /// Manifest identity used to enter the object graph.
    pub root_manifest_digest: EvidenceDigest,
    /// Authenticated root logical-object identity.
    pub root_object_digest: EvidenceDigest,
    /// Decoded canonical root value.
    pub root: RecordedMediaRoot,
    /// Decoded source-bound native inspection artifact.
    pub inspection: StoredMediaInspection,
}

/// Typed readback and closure-verification failures.
#[derive(Debug)]
pub enum RecordedMediaVerificationError {
    /// Immutable object-store lookup, readback, or verification failed.
    Store(ObjectStoreError),
    /// An artifact exceeds the verifier's hard read bound.
    ArtifactTooLarge {
        /// Logical artifact class.
        artifact: &'static str,
        /// Declared object length.
        observed: u64,
        /// Hard maximum.
        maximum: u64,
    },
    /// A verified object could not be read completely.
    ReadArtifact {
        /// Logical artifact class.
        artifact: &'static str,
        /// Underlying I/O error.
        source: std::io::Error,
    },
    /// Bytes read from a verified object do not match the authenticated manifest length.
    ReadLengthMismatch {
        /// Logical artifact class.
        artifact: &'static str,
        /// Length named by the authenticated manifest.
        expected: u64,
        /// Length actually read.
        observed: u64,
    },
    /// Recorded-media root decoding failed.
    RootDecode(RecordedMediaRootError),
    /// Native inspection-artifact decoding failed.
    InspectionDecode(MediaArtifactDecodeError),
    /// A root field does not match the authenticated child manifest or decoded child artifact.
    BindingMismatch {
        /// Stable field/path identifier.
        field: &'static str,
        /// Canonical expected value.
        expected: String,
        /// Canonical observed value.
        observed: String,
    },
    /// A collection length cannot be represented in the public `u64` domain.
    LengthOverflow {
        /// Collection being converted.
        field: &'static str,
    },
}

impl Display for RecordedMediaVerificationError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Store(error) => write!(formatter, "recorded-media object-store verification failed: {error}"),
            Self::ArtifactTooLarge {
                artifact,
                observed,
                maximum,
            } => write!(
                formatter,
                "{artifact} length {observed} exceeds verifier maximum {maximum}"
            ),
            Self::ReadArtifact { artifact, source } => {
                write!(formatter, "failed to read verified {artifact}: {source}")
            }
            Self::ReadLengthMismatch {
                artifact,
                expected,
                observed,
            } => write!(
                formatter,
                "verified {artifact} length mismatch: expected {expected}, observed {observed}"
            ),
            Self::RootDecode(error) => write!(formatter, "recorded-media root decode failed: {error}"),
            Self::InspectionDecode(error) => {
                write!(formatter, "media-inspection artifact decode failed: {error}")
            }
            Self::BindingMismatch {
                field,
                expected,
                observed,
            } => write!(
                formatter,
                "recorded-media binding mismatch at {field}: expected {expected}, observed {observed}"
            ),
            Self::LengthOverflow { field } => {
                write!(formatter, "recorded-media collection length overflows u64 at {field}")
            }
        }
    }
}

impl Error for RecordedMediaVerificationError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Store(error) => Some(error),
            Self::ReadArtifact { source, .. } => Some(source),
            Self::RootDecode(error) => Some(error),
            Self::InspectionDecode(error) => Some(error),
            Self::ArtifactTooLarge { .. }
            | Self::ReadLengthMismatch { .. }
            | Self::BindingMismatch { .. }
            | Self::LengthOverflow { .. } => None,
        }
    }
}

impl From<ObjectStoreError> for RecordedMediaVerificationError {
    fn from(value: ObjectStoreError) -> Self {
        Self::Store(value)
    }
}

impl From<RecordedMediaRootError> for RecordedMediaVerificationError {
    fn from(value: RecordedMediaRootError) -> Self {
        Self::RootDecode(value)
    }
}

impl From<MediaArtifactDecodeError> for RecordedMediaVerificationError {
    fn from(value: MediaArtifactDecodeError) -> Self {
        Self::InspectionDecode(value)
    }
}

/// Independently verifies and reconstructs a complete recorded-media object graph from its root
/// manifest identity.
///
/// # Errors
///
/// Returns a typed object-store, bound, readback, decode, length, or child-binding error. A
/// successful result proves byte-level closure of the root, source, and inspection publications;
/// it does not claim media decode, geometric correctness, or semantic correctness.
pub fn verify_recorded_media_root(
    store: &LocalObjectStore,
    root_manifest_digest: &EvidenceDigest,
) -> Result<VerifiedRecordedMedia, RecordedMediaVerificationError> {
    let mut root_object = store.open_verified_object(root_manifest_digest)?;
    let root_manifest = root_object.manifest().clone();
    let root_bytes = read_bounded_verified_object(
        &mut root_object,
        "recorded_media_root",
        MAX_ROOT_ARTIFACT_BYTES,
    )?;
    let root = RecordedMediaRoot::from_canonical_bytes(&root_bytes)?;

    store.verify_manifest(&root.source_manifest_digest)?;
    let source_manifest = store.read_manifest(&root.source_manifest_digest)?;
    verify_manifest_binding(
        "root.source",
        &root.source_object_digest,
        &root.source_manifest_digest,
        root.source_object_length,
        root.source_chunk_size,
        root.source_chunk_count,
        &source_manifest,
    )?;

    store.verify_manifest(&root.inspection_manifest_digest)?;
    let inspection_manifest = store.read_manifest(&root.inspection_manifest_digest)?;
    verify_manifest_binding(
        "root.inspection",
        &root.inspection_object_digest,
        &root.inspection_manifest_digest,
        root.inspection_object_length,
        root.inspection_chunk_size,
        root.inspection_chunk_count,
        &inspection_manifest,
    )?;

    let mut inspection_object = store.open_verified_object(&root.inspection_manifest_digest)?;
    let inspection_bytes = read_bounded_verified_object(
        &mut inspection_object,
        "media_inspection",
        MAX_INSPECTION_ARTIFACT_BYTES,
    )?;
    let inspection = decode_media_inspection_artifact(
        &inspection_bytes,
        DecodeLimits {
            max_total_bytes: usize::try_from(MAX_INSPECTION_ARTIFACT_BYTES).map_err(|_| {
                RecordedMediaVerificationError::LengthOverflow {
                    field: "inspection.max_total_bytes",
                }
            })?,
            max_blob_bytes: 128,
            max_string_bytes: 128,
        },
    )?;
    verify_inspection_basis(&root, &inspection)?;

    Ok(VerifiedRecordedMedia {
        schema: VERIFIED_RECORDED_MEDIA_SCHEMA,
        root_manifest_digest: root_manifest_digest.clone(),
        root_object_digest: root_manifest.object_digest,
        root,
        inspection,
    })
}

fn read_bounded_verified_object<R: Read>(
    reader: &mut R,
    artifact: &'static str,
    maximum: u64,
) -> Result<Vec<u8>, RecordedMediaVerificationError>
where
    R: VerifiedManifestReader,
{
    let expected = reader.verified_object_length();
    if expected > maximum {
        return Err(RecordedMediaVerificationError::ArtifactTooLarge {
            artifact,
            observed: expected,
            maximum,
        });
    }
    let capacity = usize::try_from(expected).map_err(|_| {
        RecordedMediaVerificationError::LengthOverflow {
            field: "verified_object.length",
        }
    })?;
    let mut bytes = Vec::with_capacity(capacity);
    reader
        .read_to_end(&mut bytes)
        .map_err(|source| RecordedMediaVerificationError::ReadArtifact { artifact, source })?;
    let observed = u64::try_from(bytes.len()).map_err(|_| {
        RecordedMediaVerificationError::LengthOverflow {
            field: "verified_object.bytes",
        }
    })?;
    if observed != expected {
        return Err(RecordedMediaVerificationError::ReadLengthMismatch {
            artifact,
            expected,
            observed,
        });
    }
    Ok(bytes)
}

trait VerifiedManifestReader {
    fn verified_object_length(&self) -> u64;
}

impl VerifiedManifestReader for fdgr_object_store::VerifiedObject {
    fn verified_object_length(&self) -> u64 {
        self.manifest().object_length
    }
}

fn verify_manifest_binding(
    prefix: &'static str,
    object_digest: &EvidenceDigest,
    manifest_digest: &EvidenceDigest,
    object_length: u64,
    chunk_size: u32,
    chunk_count: u64,
    manifest: &fdgr_evidence::ObjectManifest,
) -> Result<(), RecordedMediaVerificationError> {
    require_equal(
        binding_field(prefix, "object_digest"),
        object_digest.to_string(),
        manifest.object_digest.to_string(),
    )?;
    require_equal(
        binding_field(prefix, "manifest_digest"),
        manifest_digest.to_string(),
        manifest.manifest_digest.to_string(),
    )?;
    require_equal(
        binding_field(prefix, "object_length"),
        object_length.to_string(),
        manifest.object_length.to_string(),
    )?;
    require_equal(
        binding_field(prefix, "chunk_size"),
        chunk_size.to_string(),
        manifest.chunk_size.to_string(),
    )?;
    let observed_chunk_count = u64::try_from(manifest.chunks.len()).map_err(|_| {
        RecordedMediaVerificationError::LengthOverflow {
            field: "manifest.chunks",
        }
    })?;
    require_equal(
        binding_field(prefix, "chunk_count"),
        chunk_count.to_string(),
        observed_chunk_count.to_string(),
    )
}

fn verify_inspection_basis(
    root: &RecordedMediaRoot,
    inspection: &StoredMediaInspection,
) -> Result<(), RecordedMediaVerificationError> {
    require_equal(
        "inspection.source_object_digest",
        root.source_object_digest.to_string(),
        inspection.source_object_digest.to_string(),
    )?;
    require_equal(
        "inspection.source_manifest_digest",
        root.source_manifest_digest.to_string(),
        inspection.source_manifest_digest.to_string(),
    )?;
    require_equal(
        "inspection.source_object_length",
        root.source_object_length.to_string(),
        inspection.source_object_length.to_string(),
    )?;
    require_equal(
        "inspection.source_chunk_size",
        root.source_chunk_size.to_string(),
        inspection.source_chunk_size.to_string(),
    )?;
    require_equal(
        "inspection.source_chunk_count",
        root.source_chunk_count.to_string(),
        inspection.source_chunk_count.to_string(),
    )?;
    require_equal(
        "inspection.summary.file_length",
        root.source_object_length.to_string(),
        inspection.summary.file_length.to_string(),
    )
}

const fn binding_field(prefix: &'static str, suffix: &'static str) -> &'static str {
    match (prefix, suffix) {
        ("root.source", "object_digest") => "root.source.object_digest",
        ("root.source", "manifest_digest") => "root.source.manifest_digest",
        ("root.source", "object_length") => "root.source.object_length",
        ("root.source", "chunk_size") => "root.source.chunk_size",
        ("root.source", "chunk_count") => "root.source.chunk_count",
        ("root.inspection", "object_digest") => "root.inspection.object_digest",
        ("root.inspection", "manifest_digest") => "root.inspection.manifest_digest",
        ("root.inspection", "object_length") => "root.inspection.object_length",
        ("root.inspection", "chunk_size") => "root.inspection.chunk_size",
        ("root.inspection", "chunk_count") => "root.inspection.chunk_count",
        _ => "root.unknown_binding",
    }
}

fn require_equal(
    field: &'static str,
    expected: String,
    observed: String,
) -> Result<(), RecordedMediaVerificationError> {
    if expected == observed {
        Ok(())
    } else {
        Err(RecordedMediaVerificationError::BindingMismatch {
            field,
            expected,
            observed,
        })
    }
}

#[cfg(all(test, unix))]
mod tests;
