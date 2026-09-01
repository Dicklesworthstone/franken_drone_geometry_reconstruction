#![forbid(unsafe_code)]
//! Stable ledger error vocabulary.

use crate::EventKindError;
use fdgr_codec::CodecError;
use fdgr_types::{DomainError, EvidenceDigest};
use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// Stable ledger failures.
#[derive(Debug)]
pub enum LedgerError {
    /// Event kind is invalid.
    EventKind(EventKindError),
    /// Canonical codec failed.
    Codec(CodecError),
    /// Domain separator failed validation.
    Domain(DomainError),
    /// A length conversion or increment overflowed.
    LengthOverflow,
    /// Canonical ledger-event version is unsupported.
    UnsupportedVersion(u16),
    /// Sequence-zero predecessor rule was violated.
    InvalidPredecessorShape {
        /// Event sequence.
        sequence: u64,
        /// Whether a predecessor was present.
        has_previous: bool,
    },
    /// Event identity does not authenticate its fields.
    EventIdentityMismatch {
        /// Event sequence.
        sequence: u64,
        /// Recomputed identity.
        expected: EvidenceDigest,
        /// Encoded identity.
        observed: EvidenceDigest,
    },
    /// Empty/non-empty anchor shape is inconsistent.
    InvalidAnchorShape,
    /// Anchor identity does not authenticate its fields.
    AnchorIdentityMismatch {
        /// Recomputed identity.
        expected: EvidenceDigest,
        /// Encoded identity.
        observed: EvidenceDigest,
    },
    /// Optimistic append basis is no longer current.
    StaleAnchor {
        /// Caller-supplied anchor identity.
        expected_digest: EvidenceDigest,
        /// Current anchor identity.
        observed_digest: EvidenceDigest,
        /// Caller-supplied event count.
        expected_count: u64,
        /// Current event count.
        observed_count: u64,
    },
    /// Event belongs to a different lineage.
    LineageMismatch {
        /// Expected sequence.
        sequence: u64,
        /// Required lineage.
        expected: EvidenceDigest,
        /// Observed lineage.
        observed: EvidenceDigest,
    },
    /// Event belongs to a different epoch.
    EpochMismatch {
        /// Expected sequence.
        sequence: u64,
        /// Required epoch.
        expected: u64,
        /// Observed epoch.
        observed: u64,
    },
    /// Event sequence is not canonical.
    SequenceMismatch {
        /// Required sequence.
        expected: u64,
        /// Observed sequence.
        observed: u64,
    },
    /// Event predecessor does not equal the immediately preceding identity.
    PreviousEventMismatch {
        /// Event sequence.
        sequence: u64,
        /// Required predecessor.
        expected: Option<EvidenceDigest>,
        /// Observed predecessor.
        observed: Option<EvidenceDigest>,
    },
    /// Event identity appeared twice in one epoch.
    DuplicateEventIdentity(EvidenceDigest),
    /// Page limit was zero or above the hard bound.
    InvalidPageLimit {
        /// Requested events.
        actual: usize,
        /// Maximum events.
        maximum: usize,
    },
    /// Cursor starts beyond the current event count.
    CursorBeyondHead {
        /// Exclusive caller cursor.
        cursor: Option<u64>,
        /// Current event count.
        event_count: u64,
    },
}

impl Display for LedgerError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::EventKind(error) => write!(formatter, "event-kind error: {error}"),
            Self::Codec(error) => write!(formatter, "codec error: {error}"),
            Self::Domain(error) => write!(formatter, "domain error: {error}"),
            Self::LengthOverflow => formatter.write_str("ledger length overflow"),
            Self::UnsupportedVersion(version) => {
                write!(formatter, "unsupported ledger-event version {version}")
            }
            Self::InvalidPredecessorShape {
                sequence,
                has_previous,
            } => write!(
                formatter,
                "sequence {sequence} predecessor presence is {has_previous}, violating canonical shape"
            ),
            Self::EventIdentityMismatch {
                sequence,
                expected,
                observed,
            } => write!(
                formatter,
                "event {sequence} identity mismatch: expected {expected}, observed {observed}"
            ),
            Self::InvalidAnchorShape => formatter.write_str("ledger anchor shape is inconsistent"),
            Self::AnchorIdentityMismatch { expected, observed } => write!(
                formatter,
                "anchor identity mismatch: expected {expected}, observed {observed}"
            ),
            Self::StaleAnchor {
                expected_digest,
                observed_digest,
                expected_count,
                observed_count,
            } => write!(
                formatter,
                "stale append anchor: supplied {expected_digest} at count {expected_count}, current {observed_digest} at count {observed_count}"
            ),
            Self::LineageMismatch {
                sequence,
                expected,
                observed,
            } => write!(
                formatter,
                "event {sequence} lineage mismatch: expected {expected}, observed {observed}"
            ),
            Self::EpochMismatch {
                sequence,
                expected,
                observed,
            } => write!(
                formatter,
                "event {sequence} epoch mismatch: expected {expected}, observed {observed}"
            ),
            Self::SequenceMismatch { expected, observed } => write!(
                formatter,
                "event sequence mismatch: expected {expected}, observed {observed}"
            ),
            Self::PreviousEventMismatch {
                sequence,
                expected,
                observed,
            } => write!(
                formatter,
                "event {sequence} predecessor mismatch: expected {expected:?}, observed {observed:?}"
            ),
            Self::DuplicateEventIdentity(identity) => {
                write!(formatter, "duplicate event identity {identity}")
            }
            Self::InvalidPageLimit { actual, maximum } => write!(
                formatter,
                "event page limit must be in 1..={maximum}; received {actual}"
            ),
            Self::CursorBeyondHead {
                cursor,
                event_count,
            } => write!(
                formatter,
                "event cursor {cursor:?} is beyond event count {event_count}"
            ),
        }
    }
}

impl Error for LedgerError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::EventKind(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<EventKindError> for LedgerError {
    fn from(error: EventKindError) -> Self {
        Self::EventKind(error)
    }
}

impl From<CodecError> for LedgerError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for LedgerError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
