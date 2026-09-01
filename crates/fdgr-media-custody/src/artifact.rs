#![forbid(unsafe_code)]
//! Canonical publication of custody-bound media derivations.

use crate::{StoredMediaInspection, StoredSampleWindow};
use fdgr_codec::{CanonicalEncoder, CodecError};
use fdgr_media::TrackSummary;
use fdgr_object_store::{ImportReceipt, LocalObjectStore, ObjectStoreError};
use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// Publication result for one canonical custody-bound inspection artifact.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PublishedMediaInspection {
    /// Source custody and parsed summary.
    pub inspection: StoredMediaInspection,
    /// Immutable derived-artifact publication receipt.
    pub artifact: ImportReceipt,
}

/// Publication result for one canonical custody-bound sample-window artifact.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PublishedSampleWindow {
    /// Source custody and exact bounded window.
    pub sample_window: StoredSampleWindow,
    /// Immutable derived-artifact publication receipt.
    pub artifact: ImportReceipt,
}

/// Canonical encoding or immutable publication failure for derived media evidence.
#[derive(Debug)]
pub enum MediaArtifactError {
    /// Canonical codec refused or failed to finish the artifact.
    Codec(CodecError),
    /// Local immutable object publication failed.
    Store(ObjectStoreError),
}

impl Display for MediaArtifactError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Codec(error) => write!(formatter, "media artifact codec error: {error}"),
            Self::Store(error) => write!(formatter, "media artifact store error: {error}"),
        }
    }
}

impl Error for MediaArtifactError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Store(error) => Some(error),
        }
    }
}

impl From<CodecError> for MediaArtifactError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<ObjectStoreError> for MediaArtifactError {
    fn from(error: ObjectStoreError) -> Self {
        Self::Store(error)
    }
}

/// Canonically encodes one inspection together with its exact source-custody basis.
///
/// # Errors
///
/// Returns a typed codec error if a field name, value, or total artifact exceeds codec bounds.
pub fn encode_stored_media_inspection(
    value: &StoredMediaInspection,
) -> Result<Vec<u8>, CodecError> {
    let mut encoder = CanonicalEncoder::new("fdgr.stored_media_inspection_artifact", 1)?;
    encode_source_basis(&mut encoder, &value.manifest)?;
    encoder.field_bytes("summary", &encode_summary(&value.summary)?)?;
    encoder.finish()
}

/// Canonically encodes one exact sample window together with its source-custody basis and summary.
///
/// # Errors
///
/// Returns a typed codec error if a field name, value, or total artifact exceeds codec bounds.
pub fn encode_stored_sample_window(value: &StoredSampleWindow) -> Result<Vec<u8>, CodecError> {
    let mut encoder = CanonicalEncoder::new("fdgr.stored_sample_window_artifact", 1)?;
    encode_source_basis(&mut encoder, &value.manifest)?;
    encoder.field_bytes("summary", &encode_summary(&value.summary)?)?;
    encoder.field_bytes("window", &encode_window(value)?)?;
    encoder.finish()
}

/// Encodes and immutably publishes one inspection artifact.
///
/// # Errors
///
/// Returns a typed canonical-encoding or immutable-store publication failure.
pub fn publish_stored_media_inspection(
    store: &mut LocalObjectStore,
    inspection: StoredMediaInspection,
    chunk_size: u32,
) -> Result<PublishedMediaInspection, MediaArtifactError> {
    let bytes = encode_stored_media_inspection(&inspection)?;
    let artifact = store.import_bytes(&bytes, chunk_size)?;
    Ok(PublishedMediaInspection {
        inspection,
        artifact,
    })
}

/// Encodes and immutably publishes one exact sample-window artifact.
///
/// # Errors
///
/// Returns a typed canonical-encoding or immutable-store publication failure.
pub fn publish_stored_sample_window(
    store: &mut LocalObjectStore,
    sample_window: StoredSampleWindow,
    chunk_size: u32,
) -> Result<PublishedSampleWindow, MediaArtifactError> {
    let bytes = encode_stored_sample_window(&sample_window)?;
    let artifact = store.import_bytes(&bytes, chunk_size)?;
    Ok(PublishedSampleWindow {
        sample_window,
        artifact,
    })
}

fn encode_source_basis(
    encoder: &mut CanonicalEncoder,
    manifest: &fdgr_evidence::ObjectManifest,
) -> Result<(), CodecError> {
    encoder.field_u64("source_chunk_count", usize_to_u64(manifest.chunks.len()))?;
    encoder.field_u64("source_chunk_size", u64::from(manifest.chunk_size))?;
    encoder.field_bytes(
        "source_manifest_digest",
        manifest.manifest_digest.as_str().as_bytes(),
    )?;
    encoder.field_bytes(
        "source_object_digest",
        manifest.object_digest.as_str().as_bytes(),
    )?;
    encoder.field_u64("source_object_length", manifest.object_length)
}

fn encode_summary(summary: &fdgr_media::IsoBmffSummary) -> Result<Vec<u8>, CodecError> {
    let mut encoder = CanonicalEncoder::new("fdgr.media_inspection_summary", 1)?;
    encoder.field_u64("boxes_visited", summary.boxes_visited)?;
    encoder.field_bytes(
        "compatible_brands",
        &encode_string_sequence(
            summary
                .compatible_brands
                .iter()
                .map(ToString::to_string),
        ),
    )?;
    encoder.field_u64("file_length", summary.file_length)?;
    encoder.field_u64("fragmented", bool_u64(summary.fragmented))?;
    let major_brand = summary.major_brand.map(|value| value.to_string());
    encoder.field_bytes(
        "major_brand",
        major_brand.as_deref().unwrap_or_default().as_bytes(),
    )?;
    encoder.field_u64("major_brand_present", bool_u64(major_brand.is_some()))?;
    encoder.field_u64(
        "minor_version",
        u64::from(summary.minor_version.unwrap_or_default()),
    )?;
    encoder.field_u64(
        "minor_version_present",
        bool_u64(summary.minor_version.is_some()),
    )?;
    encoder.field_u64("movie_duration", summary.movie_duration)?;
    encoder.field_u64("movie_timescale", u64::from(summary.movie_timescale))?;
    encoder.field_bytes("tracks", &encode_tracks(&summary.tracks)?)?;
    encoder.finish()
}

fn encode_tracks(tracks: &[TrackSummary]) -> Result<Vec<u8>, CodecError> {
    let mut bytes = Vec::new();
    push_count(&mut bytes, tracks.len());
    for track in tracks {
        let mut encoder = CanonicalEncoder::new("fdgr.media_track_summary", 1)?;
        field_optional_u64(
            &mut encoder,
            "chunk_count",
            "chunk_count_present",
            track.chunk_count,
        )?;
        let codec = track.codec.map(|value| value.to_string());
        encoder.field_bytes("codec", codec.as_deref().unwrap_or_default().as_bytes())?;
        encoder.field_u64("codec_present", bool_u64(codec.is_some()))?;
        field_optional_u64(
            &mut encoder,
            "composition_sample_count",
            "composition_sample_count_present",
            track.composition_sample_count,
        )?;
        field_optional_u32(
            &mut encoder,
            "constant_sample_size",
            "constant_sample_size_present",
            track.constant_sample_size,
        )?;
        field_optional_u64(
            &mut encoder,
            "decode_duration",
            "decode_duration_present",
            track.decode_duration,
        )?;
        encoder.field_u64("duration", track.duration)?;
        encoder.field_bytes("handler_type", track.handler_type.to_string().as_bytes())?;
        encoder.field_u64(
            "height_fixed_16_16",
            u64::from(track.height_fixed_16_16),
        )?;
        field_optional_u64(
            &mut encoder,
            "sample_count",
            "sample_count_present",
            track.sample_count,
        )?;
        field_optional_u32(
            &mut encoder,
            "sample_description_count",
            "sample_description_count_present",
            track.sample_description_count,
        )?;
        field_optional_u64(
            &mut encoder,
            "sample_to_chunk_entry_count",
            "sample_to_chunk_entry_count_present",
            track.sample_to_chunk_entry_count,
        )?;
        field_optional_u64(
            &mut encoder,
            "sync_sample_count",
            "sync_sample_count_present",
            track.sync_sample_count,
        )?;
        encoder.field_u64("timescale", u64::from(track.timescale))?;
        field_optional_u64(
            &mut encoder,
            "total_sample_bytes",
            "total_sample_bytes_present",
            track.total_sample_bytes,
        )?;
        encoder.field_u64("track_id", u64::from(track.track_id))?;
        encoder.field_u64("width_fixed_16_16", u64::from(track.width_fixed_16_16))?;
        push_record(&mut bytes, &encoder.finish()?);
    }
    Ok(bytes)
}

fn encode_window(value: &StoredSampleWindow) -> Result<Vec<u8>, CodecError> {
    let window = &value.window;
    let mut encoder = CanonicalEncoder::new("fdgr.classic_sample_window", 1)?;
    encoder.field_u64("complete", bool_u64(window.complete))?;
    encoder.field_u64("index_entries_scanned", window.index_entries_scanned)?;
    encoder.field_u64(
        "requested_max_samples",
        usize_to_u64(window.requested_max_samples),
    )?;
    let mut samples = Vec::new();
    push_count(&mut samples, window.samples.len());
    for sample in &window.samples {
        let mut sample_encoder = CanonicalEncoder::new("fdgr.classic_sample_record", 1)?;
        sample_encoder.field_u64("byte_length", u64::from(sample.byte_length))?;
        sample_encoder.field_u64("byte_offset", sample.byte_offset)?;
        sample_encoder.field_bytes(
            "composition_time_i128_be",
            &sample.composition_time.to_be_bytes(),
        )?;
        sample_encoder.field_u64("decode_time", sample.decode_time)?;
        sample_encoder.field_u64("duration", u64::from(sample.duration))?;
        sample_encoder.field_u64("is_sync", bool_u64(sample.is_sync))?;
        sample_encoder.field_u64(
            "sample_description_index",
            u64::from(sample.sample_description_index),
        )?;
        sample_encoder.field_u64("sample_index", sample.sample_index)?;
        push_record(&mut samples, &sample_encoder.finish()?);
    }
    encoder.field_bytes("samples", &samples)?;
    encoder.field_u64("start_sample", window.start_sample)?;
    encoder.field_u64("timescale", u64::from(window.timescale))?;
    encoder.field_u64("total_samples", window.total_samples)?;
    encoder.field_u64("track_id", u64::from(window.track_id))?;
    encoder.finish()
}

fn field_optional_u32(
    encoder: &mut CanonicalEncoder,
    value_field: &str,
    present_field: &str,
    value: Option<u32>,
) -> Result<(), CodecError> {
    encoder.field_u64(value_field, u64::from(value.unwrap_or_default()))?;
    encoder.field_u64(present_field, bool_u64(value.is_some()))
}

fn field_optional_u64(
    encoder: &mut CanonicalEncoder,
    value_field: &str,
    present_field: &str,
    value: Option<u64>,
) -> Result<(), CodecError> {
    encoder.field_u64(value_field, value.unwrap_or_default())?;
    encoder.field_u64(present_field, bool_u64(value.is_some()))
}

fn encode_string_sequence(values: impl IntoIterator<Item = String>) -> Vec<u8> {
    let values: Vec<String> = values.into_iter().collect();
    let mut bytes = Vec::new();
    push_count(&mut bytes, values.len());
    for value in values {
        push_record(&mut bytes, value.as_bytes());
    }
    bytes
}

fn push_count(bytes: &mut Vec<u8>, count: usize) {
    bytes.extend_from_slice(&usize_to_u64(count).to_be_bytes());
}

fn push_record(bytes: &mut Vec<u8>, record: &[u8]) {
    bytes.extend_from_slice(&usize_to_u64(record.len()).to_be_bytes());
    bytes.extend_from_slice(record);
}

const fn bool_u64(value: bool) -> u64 {
    if value { 1 } else { 0 }
}

fn usize_to_u64(value: usize) -> u64 {
    u64::try_from(value).unwrap_or(u64::MAX)
}

#[cfg(all(test, unix))]
mod tests {
    use super::{encode_stored_media_inspection, publish_stored_media_inspection};
    use crate::StoredMediaInspection;
    use fdgr_evidence::ObjectManifest;
    use fdgr_media::{FourCc, IsoBmffSummary};
    use fdgr_object_store::LocalObjectStore;
    use std::fs;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

    fn summary() -> Option<StoredMediaInspection> {
        let manifest = ObjectManifest::build(b"source media", 4).ok()?;
        Some(StoredMediaInspection {
            manifest,
            summary: IsoBmffSummary {
                file_length: 12,
                major_brand: Some(FourCc::new(*b"isom")),
                minor_version: Some(0),
                compatible_brands: vec![FourCc::new(*b"isom")],
                movie_timescale: 1_000,
                movie_duration: 4_000,
                fragmented: false,
                boxes_visited: 10,
                tracks: Vec::new(),
            },
        })
    }

    fn test_root() -> PathBuf {
        let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
        std::env::temp_dir().join(format!(
            "fdgr-media-artifact-{}-{id}",
            std::process::id()
        ))
    }

    #[test]
    fn encoding_is_deterministic_and_basis_sensitive() {
        let value = summary();
        assert!(value.is_some());
        if let Some(value) = value {
            let first = encode_stored_media_inspection(&value);
            let second = encode_stored_media_inspection(&value);
            assert!(matches!((&first, &second), (Ok(left), Ok(right)) if left == right));
            let mut changed = value;
            changed.summary.movie_duration = 4_001;
            let changed = encode_stored_media_inspection(&changed);
            assert!(matches!((&first, &changed), (Ok(left), Ok(right)) if left != right));
        }
    }

    #[test]
    fn published_artifact_is_retrievable_by_its_manifest() {
        let root = test_root();
        let store = LocalObjectStore::open(&root);
        assert!(store.is_ok());
        if let (Ok(mut store), Some(value)) = (store, summary()) {
            let published = publish_stored_media_inspection(&mut store, value, 64);
            assert!(published.is_ok());
            if let Ok(published) = published {
                assert!(
                    store
                        .open_verified_object(&published.artifact.manifest_digest)
                        .is_ok()
                );
            }
        }
        assert!(fs::remove_dir_all(root).is_ok());
    }
}
