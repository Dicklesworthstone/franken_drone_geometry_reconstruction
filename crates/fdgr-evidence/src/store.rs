#![forbid(unsafe_code)]
//! Root-last reference publication state machine.

use crate::{ManifestError, ObjectManifest};
use fdgr_types::{EvidenceDigest, PublicationStage};
use std::collections::BTreeMap;
use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// Opaque deterministic reservation identity for the reference publication store.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ReservationId(u64);

impl ReservationId {
    /// Returns the stable numeric identifier.
    #[must_use]
    pub const fn get(self) -> u64 {
        self.0
    }
}

/// Immutable logical object retained by the reference store.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PublishedObject {
    /// Exact logical bytes.
    pub bytes: Vec<u8>,
    manifests: BTreeMap<EvidenceDigest, ObjectManifest>,
}

impl PublishedObject {
    /// Returns one verified representation manifest by identity.
    #[must_use]
    pub fn manifest(&self, digest: &EvidenceDigest) -> Option<&ObjectManifest> {
        self.manifests.get(digest)
    }

    /// Returns the number of independently addressable chunk representations.
    #[must_use]
    pub fn manifest_count(&self) -> usize {
        self.manifests.len()
    }
}

/// Publication result distinguishing logical-object and representation reuse.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PublicationReceipt {
    /// Consumed reservation.
    pub reservation_id: ReservationId,
    /// Published logical object identity.
    pub object_digest: EvidenceDigest,
    /// Published representation-manifest identity.
    pub manifest_digest: EvidenceDigest,
    /// Whether the complete logical object was already visible.
    pub object_deduplicated: bool,
    /// Whether this exact chunk representation was already visible.
    pub manifest_deduplicated: bool,
    /// Terminal publication state.
    pub stage: PublicationStage,
}

#[derive(Clone, Debug)]
struct Reservation {
    manifest: ObjectManifest,
    bytes: Option<Vec<u8>>,
    stage: PublicationStage,
}

/// Single-threaded reference oracle for reserve → materialize → verify → publish.
#[derive(Clone, Debug)]
pub struct ReferenceStore {
    next_reservation: u64,
    reservations: BTreeMap<ReservationId, Reservation>,
    published: BTreeMap<EvidenceDigest, PublishedObject>,
}

impl Default for ReferenceStore {
    fn default() -> Self {
        Self::new()
    }
}

impl ReferenceStore {
    /// Creates an empty reference store.
    #[must_use]
    pub fn new() -> Self {
        Self {
            next_reservation: 1,
            reservations: BTreeMap::new(),
            published: BTreeMap::new(),
        }
    }

    /// Reserves one exact manifest without making an object visible.
    ///
    /// # Errors
    ///
    /// Returns a manifest-validation error or reservation-ID exhaustion.
    pub fn reserve(&mut self, manifest: ObjectManifest) -> Result<ReservationId, StoreError> {
        manifest.validate_structure()?;
        let id = ReservationId(self.next_reservation);
        self.next_reservation = self
            .next_reservation
            .checked_add(1)
            .ok_or(StoreError::ReservationIdExhausted)?;
        self.reservations.insert(
            id,
            Reservation {
                manifest,
                bytes: None,
                stage: PublicationStage::Reserved,
            },
        );
        Ok(id)
    }

    /// Stages exact bytes without making them visible.
    ///
    /// # Errors
    ///
    /// Returns a typed reservation-state or length error. Identity checks belong to
    /// [`Self::verify`], preserving the explicit materialize → verify boundary.
    pub fn materialize(&mut self, id: ReservationId, bytes: Vec<u8>) -> Result<(), StoreError> {
        let reservation = self
            .reservations
            .get_mut(&id)
            .ok_or(StoreError::UnknownReservation(id))?;
        if reservation.stage != PublicationStage::Reserved {
            return Err(StoreError::InvalidStage {
                id,
                expected: PublicationStage::Reserved,
                observed: reservation.stage,
            });
        }
        let observed = u64::try_from(bytes.len()).map_err(|_| ManifestError::LengthOverflow)?;
        if observed != reservation.manifest.object_length {
            return Err(ManifestError::ObjectLengthMismatch {
                expected: reservation.manifest.object_length,
                observed,
            }
            .into());
        }
        reservation.bytes = Some(bytes);
        reservation.stage = PublicationStage::Materializing;
        Ok(())
    }

    /// Verifies staged bytes and advances the reservation to `Verified`.
    ///
    /// # Errors
    ///
    /// Returns a typed state or manifest error. Failed verification remains unpublished in
    /// `Materializing` for explicit diagnosis or abort.
    pub fn verify(&mut self, id: ReservationId) -> Result<(), StoreError> {
        let reservation = self
            .reservations
            .get_mut(&id)
            .ok_or(StoreError::UnknownReservation(id))?;
        if reservation.stage != PublicationStage::Materializing {
            return Err(StoreError::InvalidStage {
                id,
                expected: PublicationStage::Materializing,
                observed: reservation.stage,
            });
        }
        let bytes = reservation
            .bytes
            .as_ref()
            .ok_or(StoreError::MissingMaterializedBytes(id))?;
        reservation.manifest.verify_bytes(bytes)?;
        reservation.stage = PublicationStage::Verified;
        Ok(())
    }

    /// Atomically exposes one verified object and consumes its reservation.
    ///
    /// # Errors
    ///
    /// Returns a typed state or content-address conflict. A conflicting reservation remains
    /// available for diagnosis and explicit abort.
    pub fn publish(&mut self, id: ReservationId) -> Result<PublicationReceipt, StoreError> {
        let reservation = self
            .reservations
            .get(&id)
            .ok_or(StoreError::UnknownReservation(id))?;
        if reservation.stage != PublicationStage::Verified {
            return Err(StoreError::InvalidStage {
                id,
                expected: PublicationStage::Verified,
                observed: reservation.stage,
            });
        }
        let bytes = reservation
            .bytes
            .as_ref()
            .ok_or(StoreError::MissingMaterializedBytes(id))?;
        let object_digest = reservation.manifest.object_digest.clone();
        let manifest_digest = reservation.manifest.manifest_digest.clone();
        let object_deduplicated = self.published.contains_key(&object_digest);
        let manifest_deduplicated = if let Some(existing) = self.published.get(&object_digest) {
            if existing.bytes.as_slice() != bytes.as_slice() {
                return Err(StoreError::ContentAddressConflict(object_digest));
            }
            if let Some(existing_manifest) = existing.manifest(&manifest_digest) {
                if existing_manifest != &reservation.manifest {
                    return Err(StoreError::ManifestAddressConflict(manifest_digest));
                }
                true
            } else {
                false
            }
        } else {
            false
        };

        let reservation = self
            .reservations
            .remove(&id)
            .ok_or(StoreError::UnknownReservation(id))?;
        if let Some(existing) = self.published.get_mut(&object_digest) {
            if !manifest_deduplicated {
                existing
                    .manifests
                    .insert(manifest_digest.clone(), reservation.manifest);
            }
        } else {
            let mut manifests = BTreeMap::new();
            manifests.insert(manifest_digest.clone(), reservation.manifest);
            let bytes = reservation
                .bytes
                .ok_or(StoreError::MissingMaterializedBytes(id))?;
            self.published
                .insert(object_digest.clone(), PublishedObject { bytes, manifests });
        }
        Ok(PublicationReceipt {
            reservation_id: id,
            object_digest,
            manifest_digest,
            object_deduplicated,
            manifest_deduplicated,
            stage: PublicationStage::Published,
        })
    }

    /// Aborts and removes an unpublished reservation.
    ///
    /// # Errors
    ///
    /// Returns [`StoreError::UnknownReservation`] when the reservation is absent.
    pub fn abort(&mut self, id: ReservationId) -> Result<PublicationStage, StoreError> {
        self.reservations
            .remove(&id)
            .ok_or(StoreError::UnknownReservation(id))?;
        Ok(PublicationStage::Aborted)
    }

    /// Returns a published object by logical identity.
    #[must_use]
    pub fn get(&self, digest: &EvidenceDigest) -> Option<&PublishedObject> {
        self.published.get(digest)
    }

    /// Returns an unpublished reservation stage.
    #[must_use]
    pub fn stage(&self, id: ReservationId) -> Option<PublicationStage> {
        self.reservations.get(&id).map(|entry| entry.stage)
    }

    /// Returns the number of visible immutable logical objects.
    #[must_use]
    pub fn published_len(&self) -> usize {
        self.published.len()
    }
}

/// Stable reference-store failures.
#[derive(Debug)]
pub enum StoreError {
    /// Manifest construction or verification failed.
    Manifest(ManifestError),
    /// Reservation identity space was exhausted.
    ReservationIdExhausted,
    /// Reservation does not exist or has already reached a terminal state.
    UnknownReservation(ReservationId),
    /// Reservation is in the wrong publication stage.
    InvalidStage {
        /// Reservation identity.
        id: ReservationId,
        /// Required stage.
        expected: PublicationStage,
        /// Observed stage.
        observed: PublicationStage,
    },
    /// A verified reservation unexpectedly lacked materialized bytes.
    MissingMaterializedBytes(ReservationId),
    /// Identical logical identity mapped to different bytes.
    ContentAddressConflict(EvidenceDigest),
    /// Identical manifest identity mapped to different canonical manifest data.
    ManifestAddressConflict(EvidenceDigest),
}

impl Display for StoreError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Manifest(error) => write!(formatter, "manifest error: {error}"),
            Self::ReservationIdExhausted => formatter.write_str("reservation identity exhausted"),
            Self::UnknownReservation(id) => write!(formatter, "unknown reservation {}", id.get()),
            Self::InvalidStage {
                id,
                expected,
                observed,
            } => write!(
                formatter,
                "reservation {} must be {expected:?}; observed {observed:?}",
                id.get()
            ),
            Self::MissingMaterializedBytes(id) => write!(
                formatter,
                "verified reservation {} has no materialized bytes",
                id.get()
            ),
            Self::ContentAddressConflict(digest) => {
                write!(formatter, "content-address conflict for logical object {digest}")
            }
            Self::ManifestAddressConflict(digest) => write!(
                formatter,
                "content-address conflict for representation manifest {digest}"
            ),
        }
    }
}

impl Error for StoreError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Manifest(error) => Some(error),
            _ => None,
        }
    }
}

impl From<ManifestError> for StoreError {
    fn from(error: ManifestError) -> Self {
        Self::Manifest(error)
    }
}

#[cfg(test)]
mod tests {
    use super::{ReferenceStore, StoreError};
    use crate::{ManifestError, ObjectManifest};
    use fdgr_types::PublicationStage;

    #[test]
    fn store_publishes_root_last_and_deduplicates() {
        let bytes = b"immutable evidence".to_vec();
        let manifest = ObjectManifest::build(&bytes, 4);
        assert!(manifest.is_ok());
        if let Ok(manifest) = manifest {
            let digest = manifest.object_digest.clone();
            let mut store = ReferenceStore::new();
            let reservation = store.reserve(manifest.clone());
            assert!(reservation.is_ok());
            if let Ok(reservation) = reservation {
                assert!(store.get(&digest).is_none());
                assert_eq!(store.stage(reservation), Some(PublicationStage::Reserved));
                assert!(store.materialize(reservation, bytes.clone()).is_ok());
                assert_eq!(
                    store.stage(reservation),
                    Some(PublicationStage::Materializing)
                );
                assert!(store.get(&digest).is_none());
                assert!(store.verify(reservation).is_ok());
                assert_eq!(store.stage(reservation), Some(PublicationStage::Verified));
                assert!(store.get(&digest).is_none());
                assert!(matches!(
                    store.publish(reservation),
                    Ok(ref value)
                        if value.stage == PublicationStage::Published
                            && !value.object_deduplicated
                            && !value.manifest_deduplicated
                ));
                assert!(store.get(&digest).is_some());
            }
            let duplicate = store.reserve(manifest);
            assert!(duplicate.is_ok());
            if let Ok(duplicate) = duplicate {
                assert!(store.materialize(duplicate, bytes).is_ok());
                assert!(store.verify(duplicate).is_ok());
                assert!(matches!(
                    store.publish(duplicate),
                    Ok(ref value)
                        if value.object_deduplicated && value.manifest_deduplicated
                ));
                assert_eq!(store.published_len(), 1);
            }
        }
    }

    #[test]
    fn logical_and_representation_identity_are_separate() {
        let bytes = b"same logical bytes with two chunkings".to_vec();
        let first = ObjectManifest::build(&bytes, 4);
        let second = ObjectManifest::build(&bytes, 7);
        assert!(first.is_ok());
        assert!(second.is_ok());
        if let (Ok(first), Ok(second)) = (first, second) {
            assert_eq!(first.object_digest, second.object_digest);
            assert_ne!(first.manifest_digest, second.manifest_digest);
            let object_digest = first.object_digest.clone();
            let second_manifest = second.manifest_digest.clone();
            let mut store = ReferenceStore::new();
            let first_id = store.reserve(first);
            assert!(first_id.is_ok());
            if let Ok(first_id) = first_id {
                assert!(store.materialize(first_id, bytes.clone()).is_ok());
                assert!(store.verify(first_id).is_ok());
                assert!(store.publish(first_id).is_ok());
            }
            let second_id = store.reserve(second);
            assert!(second_id.is_ok());
            if let Ok(second_id) = second_id {
                assert!(store.materialize(second_id, bytes).is_ok());
                assert!(store.verify(second_id).is_ok());
                assert!(matches!(
                    store.publish(second_id),
                    Ok(ref value)
                        if value.object_deduplicated && !value.manifest_deduplicated
                ));
            }
            assert!(matches!(
                store.get(&object_digest),
                Some(object)
                    if object.manifest_count() == 2
                        && object.manifest(&second_manifest).is_some()
            ));
        }
    }

    #[test]
    fn failed_verification_leaves_nothing_visible() {
        let bytes = b"expected".to_vec();
        let manifest = ObjectManifest::build(&bytes, 4);
        assert!(manifest.is_ok());
        if let Ok(manifest) = manifest {
            let digest = manifest.object_digest.clone();
            let mut store = ReferenceStore::new();
            let reservation = store.reserve(manifest);
            assert!(reservation.is_ok());
            if let Ok(reservation) = reservation {
                assert!(store.materialize(reservation, b"xxxxxxxx".to_vec()).is_ok());
                assert!(matches!(
                    store.verify(reservation),
                    Err(StoreError::Manifest(ManifestError::ChunkDigestMismatch { .. }))
                ));
                assert_eq!(
                    store.stage(reservation),
                    Some(PublicationStage::Materializing)
                );
                assert!(store.get(&digest).is_none());
                assert!(store.publish(reservation).is_err());
            }
        }
    }
}
