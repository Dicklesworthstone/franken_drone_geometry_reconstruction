#![forbid(unsafe_code)]
//! Custody-preserving media inspection over authenticated published objects.
//!
//! This crate is the narrow integration seam between immutable local evidence custody and the
//! native media parser. It never reopens the original source path and it carries both logical and
//! representation identities into every derived result.

mod artifact;

pub use artifact::{
    MediaArtifactError, PublishedMediaInspection, PublishedSampleWindow,
    encode_stored_media_inspection, encode_stored_sample_window,
    publish_stored_media_inspection, publish_stored_sample_window,
};
use fdgr_evidence::ObjectManifest;
use fdgr_media::{
    IsoBmffSummary, MediaError, ParseLimits, SampleIndexError, SampleWindowLimits,
    SampleWindowRequest, TrackSampleWindow, inspect_iso_bmff, read_classic_sample_window,
};
use fdgr_object_store::{LocalObjectStore, ObjectStoreError};
use fdgr_types::EvidenceDigest;
use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// Public schema identity for inspection results bound to immutable custody.
pub const STORED_MEDIA_INSPECTION_SCHEMA: &str = "fdgr.stored_media_inspection/1";
/// Public schema identity for exact sample windows bound to immutable custody.
pub const STORED_SAMPLE_WINDOW_SCHEMA: &str = "fdgr.stored_sample_window/1";

/// Authenticated immutable-media inspection result.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct StoredMediaInspection {
    /// Published representation manifest that authenticated the bytes.
    pub manifest: ObjectManifest,
    /// Native bounded ISO BMFF summary over those exact bytes.
    pub summary: IsoBmffSummary,
}

/// Authenticated immutable-media sample window.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct StoredSampleWindow {
    /// Published representation manifest that authenticated the bytes.
    pub manifest: ObjectManifest,
    /// Native bounded summary over the same exact bytes.
    pub summary: IsoBmffSummary,
    /// Exact bounded classic-table sample window.
    pub window: TrackSampleWindow,
}

/// Stable failures at the custody/media integration boundary.
#[derive(Debug)]
pub enum MediaCustodyError {
    /// Manifest/object lookup or authentication failed before media parsing.
    Store(ObjectStoreError),
    /// Bounded container inspection failed after custody authentication.
    Media(MediaError),
    /// Exact classic-sample expansion failed after custody authentication.
    Sample(SampleIndexError),
}

impl Display for MediaCustodyError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Store(error) => write!(formatter, "stored media custody error: {error}"),
            Self::Media(error) => write!(formatter, "stored media inspection error: {error}"),
            Self::Sample(error) => write!(formatter, "stored media sample-index error: {error}"),
        }
    }
}

impl Error for MediaCustodyError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Store(error) => Some(error),
            Self::Media(error) => Some(error),
            Self::Sample(error) => Some(error),
        }
    }
}

impl From<ObjectStoreError> for MediaCustodyError {
    fn from(error: ObjectStoreError) -> Self {
        Self::Store(error)
    }
}

impl From<MediaError> for MediaCustodyError {
    fn from(error: MediaError) -> Self {
        Self::Media(error)
    }
}

impl From<SampleIndexError> for MediaCustodyError {
    fn from(error: SampleIndexError) -> Self {
        Self::Sample(error)
    }
}

/// Authenticates a published object and inspects the exact open bytes without reopening any source
/// path.
///
/// # Errors
///
/// Returns a typed store-authentication or bounded media-inspection failure.
pub fn inspect_published_media(
    store: &LocalObjectStore,
    manifest_digest: &EvidenceDigest,
    limits: ParseLimits,
) -> Result<StoredMediaInspection, MediaCustodyError> {
    let mut object = store.open_verified_object(manifest_digest)?;
    let manifest = object.manifest().clone();
    let length = object.object_length();
    let summary = inspect_iso_bmff(&mut object, length, limits)?;
    Ok(StoredMediaInspection { manifest, summary })
}

/// Authenticates a published object and expands one exact classic-table sample window over the same
/// open bytes.
///
/// # Errors
///
/// Returns a typed store-authentication, bounded media-inspection, request, table, or budget failure.
pub fn read_published_sample_window(
    store: &LocalObjectStore,
    manifest_digest: &EvidenceDigest,
    request: SampleWindowRequest,
    parse_limits: ParseLimits,
    window_limits: SampleWindowLimits,
) -> Result<StoredSampleWindow, MediaCustodyError> {
    let mut object = store.open_verified_object(manifest_digest)?;
    let manifest = object.manifest().clone();
    let length = object.object_length();
    let (summary, window) = read_classic_sample_window(
        &mut object,
        length,
        request,
        parse_limits,
        window_limits,
    )?;
    Ok(StoredSampleWindow {
        manifest,
        summary,
        window,
    })
}

#[cfg(all(test, unix))]
mod tests;
