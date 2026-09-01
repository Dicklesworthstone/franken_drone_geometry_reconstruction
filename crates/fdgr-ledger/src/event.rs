#![forbid(unsafe_code)]
//! Canonical immutable ledger events.

use crate::{
    EVENT_DOMAIN, EVENT_VERSION, EventKind, LedgerError, MAX_EVENT_BYTES, MAX_EVENT_KIND_BYTES,
};
use fdgr_codec::{DecodeLimits, Decoder, Encoder, hash_domain};
use fdgr_types::{DigestDomain, EvidenceDigest};

/// Immutable append-only ledger event.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EvidenceEvent {
    /// Stable lineage identity.
    pub lineage: EvidenceDigest,
    /// Observation epoch. Restore or ambiguous restart creates a successor epoch.
    pub epoch: u64,
    /// Zero-based sequence inside the epoch.
    pub sequence: u64,
    /// Exact predecessor event, absent only for sequence zero.
    pub previous_event: Option<EvidenceDigest>,
    /// Registered semantic event family.
    pub kind: EventKind,
    /// Identity of the immutable event payload object.
    pub payload_root: EvidenceDigest,
    /// Domain-separated identity of this canonical event.
    pub event_id: EvidenceDigest,
}

impl EvidenceEvent {
    /// Creates and authenticates one event.
    ///
    /// # Errors
    ///
    /// Returns a canonical predecessor, encoding, or domain error.
    pub fn create(
        lineage: EvidenceDigest,
        epoch: u64,
        sequence: u64,
        previous_event: Option<EvidenceDigest>,
        kind: EventKind,
        payload_root: EvidenceDigest,
    ) -> Result<Self, LedgerError> {
        validate_predecessor_shape(sequence, previous_event.as_ref())?;
        let mut event = Self {
            lineage,
            epoch,
            sequence,
            previous_event,
            kind,
            payload_root,
            event_id: EvidenceDigest::from_bytes([0_u8; 32]),
        };
        event.event_id = compute_event_id(&event)?;
        Ok(event)
    }

    /// Encodes the complete event, including its authenticated identity.
    ///
    /// # Errors
    ///
    /// Returns a canonical encoding or identity error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, LedgerError> {
        self.validate_identity()?;
        let mut encoder = Encoder::with_capacity(256);
        encode_event_body(self, &mut encoder)?;
        encoder.put_digest(&self.event_id);
        Ok(encoder.into_bytes())
    }

    /// Decodes and validates one canonical event.
    ///
    /// # Errors
    ///
    /// Returns a typed version, bounds, kind, predecessor, or identity error.
    pub fn from_canonical_bytes(bytes: &[u8]) -> Result<Self, LedgerError> {
        let limits = DecodeLimits {
            max_total_bytes: MAX_EVENT_BYTES,
            max_blob_bytes: 0,
            max_string_bytes: MAX_EVENT_KIND_BYTES,
        };
        let mut decoder = Decoder::new(bytes, limits)?;
        let version = decoder.read_u16()?;
        if version != EVENT_VERSION {
            return Err(LedgerError::UnsupportedVersion(version));
        }
        let lineage = decoder.read_digest()?;
        let epoch = decoder.read_u64()?;
        let sequence = decoder.read_u64()?;
        let previous_event = if decoder.read_bool()? {
            Some(decoder.read_digest()?)
        } else {
            None
        };
        let kind = EventKind::parse(decoder.read_str()?)?;
        let payload_root = decoder.read_digest()?;
        let event_id = decoder.read_digest()?;
        decoder.finish()?;
        let event = Self {
            lineage,
            epoch,
            sequence,
            previous_event,
            kind,
            payload_root,
            event_id,
        };
        event.validate_identity()?;
        Ok(event)
    }

    /// Recomputes and validates predecessor shape and event identity.
    ///
    /// # Errors
    ///
    /// Returns a typed predecessor or identity mismatch.
    pub fn validate_identity(&self) -> Result<(), LedgerError> {
        validate_predecessor_shape(self.sequence, self.previous_event.as_ref())?;
        let expected = compute_event_id(self)?;
        if expected != self.event_id {
            return Err(LedgerError::EventIdentityMismatch {
                sequence: self.sequence,
                expected,
                observed: self.event_id.clone(),
            });
        }
        Ok(())
    }
}

fn validate_predecessor_shape(
    sequence: u64,
    previous_event: Option<&EvidenceDigest>,
) -> Result<(), LedgerError> {
    let has_previous = previous_event.is_some();
    if (sequence == 0) == has_previous {
        return Err(LedgerError::InvalidPredecessorShape {
            sequence,
            has_previous,
        });
    }
    Ok(())
}

fn event_domain() -> Result<DigestDomain, LedgerError> {
    DigestDomain::parse(EVENT_DOMAIN).map_err(LedgerError::from)
}

fn encode_event_body(event: &EvidenceEvent, encoder: &mut Encoder) -> Result<(), LedgerError> {
    encoder.put_u16(EVENT_VERSION);
    encoder.put_digest(&event.lineage);
    encoder.put_u64(event.epoch);
    encoder.put_u64(event.sequence);
    encoder.put_bool(event.previous_event.is_some());
    if let Some(previous) = &event.previous_event {
        encoder.put_digest(previous);
    }
    encoder.put_str(event.kind.as_str())?;
    encoder.put_digest(&event.payload_root);
    Ok(())
}

fn compute_event_id(event: &EvidenceEvent) -> Result<EvidenceDigest, LedgerError> {
    let mut encoder = Encoder::with_capacity(256);
    encode_event_body(event, &mut encoder)?;
    hash_domain(&event_domain()?, encoder.as_bytes()).map_err(LedgerError::from)
}

#[cfg(test)]
mod tests {
    use super::EvidenceEvent;
    use crate::EventKind;
    use fdgr_codec::hash_bytes;

    #[test]
    fn event_codec_is_deterministic() {
        let kind = EventKind::parse("media_imported");
        assert!(kind.is_ok());
        if let Ok(kind) = kind {
            let event = EvidenceEvent::create(
                hash_bytes(b"lineage"),
                3,
                0,
                None,
                kind,
                hash_bytes(b"payload"),
            );
            assert!(event.is_ok());
            if let Ok(event) = event {
                let encoded = event.to_canonical_bytes();
                assert!(encoded.is_ok());
                if let Ok(encoded) = encoded {
                    assert!(matches!(
                        EvidenceEvent::from_canonical_bytes(&encoded),
                        Ok(ref decoded) if decoded == &event
                    ));
                }
            }
        }
    }

    #[test]
    fn predecessor_shape_fails_closed() {
        let kind = EventKind::parse("bad_shape");
        assert!(kind.is_ok());
        if let Ok(kind) = kind {
            assert!(EvidenceEvent::create(
                hash_bytes(b"lineage"),
                0,
                0,
                Some(hash_bytes(b"unexpected")),
                kind.clone(),
                hash_bytes(b"payload"),
            )
            .is_err());
            assert!(EvidenceEvent::create(
                hash_bytes(b"lineage"),
                0,
                1,
                None,
                kind,
                hash_bytes(b"payload"),
            )
            .is_err());
        }
    }
}
