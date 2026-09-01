#![forbid(unsafe_code)]
//! Reference content-addressed evidence manifests and root-last publication.

mod manifest;
mod store;

pub use manifest::{
    ChunkDescriptor, ManifestError, ObjectManifest, build_file_manifest, verify_file,
};
pub use store::{
    PublicationReceipt, PublishedObject, ReferenceStore, ReservationId, StoreError,
};

/// Public schema identity for object manifests.
pub const OBJECT_MANIFEST_SCHEMA: &str = "fdgr.object_manifest/1";
/// Default immutable chunk size: four mebibytes.
pub const DEFAULT_CHUNK_SIZE: u32 = 4 * 1024 * 1024;
/// Largest admitted chunk size: sixty-four mebibytes.
pub const MAX_CHUNK_SIZE: u32 = 64 * 1024 * 1024;
/// Maximum number of chunks in one reference manifest.
pub const MAX_CHUNKS: usize = 1_000_000;
/// Maximum canonical manifest bytes accepted by the decoder.
pub const MAX_MANIFEST_BYTES: usize = 64 * 1024 * 1024;

pub(crate) const MANIFEST_VERSION: u16 = 1;
pub(crate) const CHUNK_DOMAIN: &str = "fdgr.evidence_chunk/1";
pub(crate) const OBJECT_DOMAIN: &str = "fdgr.evidence_object/1";
pub(crate) const MANIFEST_DOMAIN: &str = "fdgr.object_manifest/1";
