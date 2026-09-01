#![forbid(unsafe_code)]
//! Append-only reference ledger, anchors, replay, and bounded continuation pages.

use crate::{ANCHOR_DOMAIN, EvidenceEvent, EventKind, LedgerError, MAX_PAGE_EVENTS};
use fdgr_codec::{Encoder, hash_domain};
use fdgr_types::{DigestDomain, EvidenceDigest};
use std::collections::BTreeSet;

/// Complete immutable read anchor for one ledger epoch.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LedgerAnchor {
    /// Stable lineage identity.
    pub lineage: EvidenceDigest,
    /// Observation epoch.
    pub epoch: u64,
    /// Number of events in the epoch.
    pub event_count: u64,
    /// Current head event, absent for an empty epoch.
    pub head_event: Option<EvidenceDigest>,
    /// Domain-separated identity of the complete anchor fields.
    pub anchor_digest: EvidenceDigest,
}

impl LedgerAnchor {
    fn create(
        lineage: EvidenceDigest,
        epoch: u64,
        event_count: u64,
        head_event: Option<EvidenceDigest>,
    ) -> Result<Self, LedgerError> {
        if (event_count == 0) != head_event.is_none() {
            return Err(LedgerError::InvalidAnchorShape);
        }
        let mut anchor = Self {
            lineage,
            epoch,
            event_count,
            head_event,
            anchor_digest: EvidenceDigest::from_bytes([0_u8; 32]),
        };
        anchor.anchor_digest = compute_anchor_digest(&anchor)?;
        Ok(anchor)
    }

    /// Returns the highest event sequence, or `None` for an empty epoch.
    #[must_use]
    pub fn high_water_sequence(&self) -> Option<u64> {
        self.event_count.checked_sub(1)
    }

    /// Recomputes the anchor digest.
    ///
    /// # Errors
    ///
    /// Returns a typed shape, canonical encoding, or identity error.
    pub fn validate(&self) -> Result<(), LedgerError> {
        if (self.event_count == 0) != self.head_event.is_none() {
            return Err(LedgerError::InvalidAnchorShape);
        }
        let expected = compute_anchor_digest(self)?;
        if expected != self.anchor_digest {
            return Err(LedgerError::AnchorIdentityMismatch {
                expected,
                observed: self.anchor_digest.clone(),
            });
        }
        Ok(())
    }
}

/// Bounded immutable page of canonical events.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EventPage {
    /// Anchor against which this page was selected.
    pub anchor: LedgerAnchor,
    /// Exclusive sequence cursor supplied by the caller.
    pub after_sequence: Option<u64>,
    /// Events in canonical sequence order.
    pub events: Vec<EvidenceEvent>,
    /// Exclusive sequence cursor for the next page, absent when complete.
    pub continuation_after: Option<u64>,
    /// Whether all events after the supplied cursor are present.
    pub complete: bool,
}

/// Single-threaded append-only reference ledger.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReferenceLedger {
    lineage: EvidenceDigest,
    epoch: u64,
    events: Vec<EvidenceEvent>,
}

impl ReferenceLedger {
    /// Creates an empty epoch for a stable lineage.
    #[must_use]
    pub fn new(lineage: EvidenceDigest, epoch: u64) -> Self {
        Self {
            lineage,
            epoch,
            events: Vec::new(),
        }
    }

    /// Replays and validates a complete epoch.
    ///
    /// # Errors
    ///
    /// Returns the first stable lineage, epoch, sequence, predecessor, duplicate, or identity
    /// mismatch.
    pub fn replay(
        lineage: EvidenceDigest,
        epoch: u64,
        events: Vec<EvidenceEvent>,
    ) -> Result<Self, LedgerError> {
        let ledger = Self {
            lineage,
            epoch,
            events,
        };
        ledger.validate()?;
        Ok(ledger)
    }

    /// Returns the exact current anchor.
    ///
    /// # Errors
    ///
    /// Returns a canonical identity error if the epoch has an unsupported in-memory length.
    pub fn anchor(&self) -> Result<LedgerAnchor, LedgerError> {
        let event_count =
            u64::try_from(self.events.len()).map_err(|_| LedgerError::LengthOverflow)?;
        let head_event = self.events.last().map(|event| event.event_id.clone());
        LedgerAnchor::create(self.lineage.clone(), self.epoch, event_count, head_event)
    }

    /// Appends one event only when the supplied anchor exactly matches the current epoch head.
    ///
    /// # Errors
    ///
    /// Returns a stale-anchor, identity, length, or canonical encoding error. Refusal leaves the
    /// ledger unchanged.
    pub fn append(
        &mut self,
        expected_anchor: &LedgerAnchor,
        kind: EventKind,
        payload_root: EvidenceDigest,
    ) -> Result<EvidenceEvent, LedgerError> {
        expected_anchor.validate()?;
        let observed_anchor = self.anchor()?;
        if expected_anchor != &observed_anchor {
            return Err(LedgerError::StaleAnchor {
                expected_digest: expected_anchor.anchor_digest.clone(),
                observed_digest: observed_anchor.anchor_digest,
                expected_count: expected_anchor.event_count,
                observed_count: observed_anchor.event_count,
            });
        }
        let sequence =
            u64::try_from(self.events.len()).map_err(|_| LedgerError::LengthOverflow)?;
        let previous = self.events.last().map(|event| event.event_id.clone());
        let event = EvidenceEvent::create(
            self.lineage.clone(),
            self.epoch,
            sequence,
            previous,
            kind,
            payload_root,
        )?;
        self.events.push(event.clone());
        Ok(event)
    }

    /// Returns a bounded continuation page after an exclusive sequence cursor.
    ///
    /// # Errors
    ///
    /// Returns a zero/oversized limit, overflow, or cursor-beyond-head error.
    pub fn page_after(
        &self,
        after_sequence: Option<u64>,
        limit: usize,
    ) -> Result<EventPage, LedgerError> {
        if limit == 0 || limit > MAX_PAGE_EVENTS {
            return Err(LedgerError::InvalidPageLimit {
                actual: limit,
                maximum: MAX_PAGE_EVENTS,
            });
        }
        let start_u64 = match after_sequence {
            Some(sequence) => sequence
                .checked_add(1)
                .ok_or(LedgerError::LengthOverflow)?,
            None => 0,
        };
        let event_count =
            u64::try_from(self.events.len()).map_err(|_| LedgerError::LengthOverflow)?;
        if start_u64 > event_count {
            return Err(LedgerError::CursorBeyondHead {
                cursor: after_sequence,
                event_count,
            });
        }
        let start = usize::try_from(start_u64).map_err(|_| LedgerError::LengthOverflow)?;
        let events: Vec<_> = self.events.iter().skip(start).take(limit).cloned().collect();
        let returned = u64::try_from(events.len()).map_err(|_| LedgerError::LengthOverflow)?;
        let consumed = start_u64
            .checked_add(returned)
            .ok_or(LedgerError::LengthOverflow)?;
        let complete = consumed == event_count;
        let continuation_after = if complete {
            None
        } else {
            events.last().map(|event| event.sequence)
        };
        Ok(EventPage {
            anchor: self.anchor()?,
            after_sequence,
            events,
            continuation_after,
            complete,
        })
    }

    /// Validates the complete ordered event chain and rejects duplicate identities.
    ///
    /// # Errors
    ///
    /// Returns the first stable lineage, epoch, sequence, predecessor, duplicate, or event-ID
    /// mismatch.
    pub fn validate(&self) -> Result<(), LedgerError> {
        let mut previous = None;
        let mut identities = BTreeSet::new();
        for (position, event) in self.events.iter().enumerate() {
            let expected_sequence =
                u64::try_from(position).map_err(|_| LedgerError::LengthOverflow)?;
            if event.lineage != self.lineage {
                return Err(LedgerError::LineageMismatch {
                    sequence: expected_sequence,
                    expected: self.lineage.clone(),
                    observed: event.lineage.clone(),
                });
            }
            if event.epoch != self.epoch {
                return Err(LedgerError::EpochMismatch {
                    sequence: expected_sequence,
                    expected: self.epoch,
                    observed: event.epoch,
                });
            }
            if event.sequence != expected_sequence {
                return Err(LedgerError::SequenceMismatch {
                    expected: expected_sequence,
                    observed: event.sequence,
                });
            }
            if event.previous_event != previous {
                return Err(LedgerError::PreviousEventMismatch {
                    sequence: expected_sequence,
                    expected: previous,
                    observed: event.previous_event.clone(),
                });
            }
            event.validate_identity()?;
            if !identities.insert(event.event_id.clone()) {
                return Err(LedgerError::DuplicateEventIdentity(event.event_id.clone()));
            }
            previous = Some(event.event_id.clone());
        }
        self.anchor()?.validate()
    }

    /// Returns the number of events in the current epoch.
    #[must_use]
    pub fn len(&self) -> usize {
        self.events.len()
    }

    /// Returns whether the current epoch is empty.
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.events.is_empty()
    }

    /// Borrows events in canonical sequence order.
    #[must_use]
    pub fn events(&self) -> &[EvidenceEvent] {
        &self.events
    }
}

fn anchor_domain() -> Result<DigestDomain, LedgerError> {
    DigestDomain::parse(ANCHOR_DOMAIN).map_err(LedgerError::from)
}

fn compute_anchor_digest(anchor: &LedgerAnchor) -> Result<EvidenceDigest, LedgerError> {
    let mut encoder = Encoder::with_capacity(128);
    encoder.put_digest(&anchor.lineage);
    encoder.put_u64(anchor.epoch);
    encoder.put_u64(anchor.event_count);
    encoder.put_bool(anchor.head_event.is_some());
    if let Some(head) = &anchor.head_event {
        encoder.put_digest(head);
    }
    hash_domain(&anchor_domain()?, encoder.as_bytes()).map_err(LedgerError::from)
}

#[cfg(test)]
mod tests {
    use super::ReferenceLedger;
    use crate::{EventKind, LedgerError};
    use fdgr_codec::hash_bytes;
    use fdgr_types::EvidenceDigest;

    fn digest(value: &[u8]) -> EvidenceDigest {
        hash_bytes(value)
    }

    #[test]
    fn append_replay_and_event_codec_are_deterministic() {
        let lineage = digest(b"lineage");
        let mut ledger = ReferenceLedger::new(lineage.clone(), 3);
        let empty = ledger.anchor();
        let first_kind = EventKind::parse("media_imported");
        assert!(empty.is_ok());
        assert!(first_kind.is_ok());
        if let (Ok(empty), Ok(first_kind)) = (empty, first_kind) {
            let first = ledger.append(&empty, first_kind, digest(b"payload-1"));
            assert!(first.is_ok());
            let current = ledger.anchor();
            let second_kind = EventKind::parse("manifest_published");
            assert!(current.is_ok());
            assert!(second_kind.is_ok());
            if let (Ok(current), Ok(second_kind)) = (current, second_kind) {
                assert!(ledger
                    .append(&current, second_kind, digest(b"payload-2"))
                    .is_ok());
            }
            assert!(ledger.validate().is_ok());
            assert!(matches!(
                ReferenceLedger::replay(lineage, 3, ledger.events().to_vec()),
                Ok(ref replayed) if replayed == &ledger
            ));
        }
    }

    #[test]
    fn stale_anchor_refuses_without_mutation() {
        let lineage = digest(b"lineage");
        let mut ledger = ReferenceLedger::new(lineage, 0);
        let stale = ledger.anchor();
        let first_kind = EventKind::parse("first");
        let second_kind = EventKind::parse("second");
        assert!(stale.is_ok());
        assert!(first_kind.is_ok());
        assert!(second_kind.is_ok());
        if let (Ok(stale), Ok(first_kind), Ok(second_kind)) =
            (stale, first_kind, second_kind)
        {
            assert!(ledger
                .append(&stale, first_kind, digest(b"one"))
                .is_ok());
            let before = ledger.len();
            assert!(matches!(
                ledger.append(&stale, second_kind, digest(b"two")),
                Err(LedgerError::StaleAnchor { .. })
            ));
            assert_eq!(ledger.len(), before);
        }
    }

    #[test]
    fn replay_rejects_reordering_and_mutation() {
        let lineage = digest(b"lineage");
        let mut ledger = ReferenceLedger::new(lineage.clone(), 9);
        let anchor = ledger.anchor();
        let first_kind = EventKind::parse("first");
        assert!(anchor.is_ok());
        assert!(first_kind.is_ok());
        if let (Ok(anchor), Ok(first_kind)) = (anchor, first_kind) {
            assert!(ledger
                .append(&anchor, first_kind, digest(b"one"))
                .is_ok());
            let next = ledger.anchor();
            let second_kind = EventKind::parse("second");
            assert!(next.is_ok());
            assert!(second_kind.is_ok());
            if let (Ok(next), Ok(second_kind)) = (next, second_kind) {
                assert!(ledger
                    .append(&next, second_kind, digest(b"two"))
                    .is_ok());
            }
            let mut reordered = ledger.events().to_vec();
            reordered.swap(0, 1);
            assert!(ReferenceLedger::replay(lineage.clone(), 9, reordered).is_err());
            let mut mutated = ledger.events().to_vec();
            if let Some(first) = mutated.first_mut() {
                first.payload_root = digest(b"different");
            }
            assert!(matches!(
                ReferenceLedger::replay(lineage, 9, mutated),
                Err(LedgerError::EventIdentityMismatch { sequence: 0, .. })
            ));
        }
    }

    #[test]
    fn pages_are_bounded_and_continuable() {
        let mut ledger = ReferenceLedger::new(digest(b"lineage"), 1);
        let item_kind = EventKind::parse("item");
        assert!(item_kind.is_ok());
        if let Ok(item_kind) = item_kind {
            for number in 0_u8..5 {
                let anchor = ledger.anchor();
                assert!(anchor.is_ok());
                if let Ok(anchor) = anchor {
                    assert!(ledger
                        .append(&anchor, item_kind.clone(), digest(&[number]))
                        .is_ok());
                }
            }
        }
        let first = ledger.page_after(None, 2);
        assert!(matches!(
            first,
            Ok(ref page)
                if page.events.len() == 2
                    && page.continuation_after == Some(1)
                    && !page.complete
        ));
        let second = ledger.page_after(Some(1), 8);
        assert!(matches!(
            second,
            Ok(ref page)
                if page.events.len() == 3
                    && page.continuation_after.is_none()
                    && page.complete
        ));
    }
}
