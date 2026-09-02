/// Stable validation, matching, tracking, replay, and encoding failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CorrespondenceError {
    /// A mandatory identity was the all-zero sentinel.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A mandatory numeric field was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
    },
    /// A bounded value exceeded its constitutional maximum.
    BoundExceeded {
        /// Stable field name.
        field: &'static str,
        /// Observed magnitude.
        actual: u128,
        /// Maximum magnitude.
        maximum: u128,
    },
    /// No observations were supplied.
    EmptyObservations,
    /// No frame pairs were supplied.
    EmptyPairs,
    /// An observation identity was duplicated.
    DuplicateObservationId {
        /// Duplicate identity.
        observation_id: u64,
    },
    /// A feature identity was duplicated inside one frame.
    DuplicateFeatureInFrame {
        /// Frame sample index.
        sample_index: u64,
        /// Duplicate feature identity.
        feature_id: u32,
    },
    /// One sample index was associated with multiple frame objects.
    FrameIdentityConflict {
        /// Sample index.
        sample_index: u64,
    },
    /// A frame-pair identity was duplicated.
    DuplicatePairId {
        /// Duplicate pair identity.
        pair_id: u64,
    },
    /// The same canonical frame pair was requested more than once.
    DuplicateFramePair {
        /// Left sample index.
        left_sample_index: u64,
        /// Right sample index.
        right_sample_index: u64,
    },
    /// A pair was not canonical or referenced the same frame twice.
    NonCanonicalFramePair {
        /// Pair identity.
        pair_id: u64,
    },
    /// A pair referenced an unknown frame sample.
    UnknownFrameSample {
        /// Pair identity.
        pair_id: u64,
        /// Missing sample index.
        sample_index: u64,
    },
    /// Descriptor-distance work exceeded the explicit budget.
    DistanceBudgetExceeded {
        /// Charged evaluations at refusal.
        attempted: u64,
        /// Admitted maximum.
        maximum: u64,
    },
    /// Accepted match count exceeded the hard output bound.
    MatchLimitExceeded {
        /// Observed count.
        actual: usize,
        /// Maximum count.
        maximum: usize,
    },
    /// Internal observation lookup failed despite validated input.
    UnknownObservation {
        /// Missing observation identity.
        observation_id: u64,
    },
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation name.
        field: &'static str,
    },
    /// Deterministic replay disagreed with retained output.
    DerivedMismatch {
        /// Stable field name.
        field: &'static str,
    },
    /// Canonical encoding or hashing failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for CorrespondenceError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => write!(
                formatter,
                "correspondence identity {field} must not be all zero"
            ),
            Self::ZeroValue { field } => {
                write!(formatter, "correspondence field {field} must be nonzero")
            }
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "correspondence field {field} is {actual}; maximum is {maximum}"
            ),
            Self::EmptyObservations => {
                formatter.write_str("correspondence generation requires feature observations")
            }
            Self::EmptyPairs => {
                formatter.write_str("correspondence generation requires frame pairs")
            }
            Self::DuplicateObservationId { observation_id } => write!(
                formatter,
                "feature observation identity {observation_id} is duplicated"
            ),
            Self::DuplicateFeatureInFrame {
                sample_index,
                feature_id,
            } => write!(
                formatter,
                "feature identity {feature_id} is duplicated in sample {sample_index}"
            ),
            Self::FrameIdentityConflict { sample_index } => write!(
                formatter,
                "sample {sample_index} is associated with multiple frame digests"
            ),
            Self::DuplicatePairId { pair_id } => {
                write!(formatter, "frame-pair identity {pair_id} is duplicated")
            }
            Self::DuplicateFramePair {
                left_sample_index,
                right_sample_index,
            } => write!(
                formatter,
                "frame pair ({left_sample_index}, {right_sample_index}) is duplicated"
            ),
            Self::NonCanonicalFramePair { pair_id } => write!(
                formatter,
                "frame pair {pair_id} must satisfy left_sample_index < right_sample_index"
            ),
            Self::UnknownFrameSample {
                pair_id,
                sample_index,
            } => write!(
                formatter,
                "frame pair {pair_id} references unknown sample {sample_index}"
            ),
            Self::DistanceBudgetExceeded { attempted, maximum } => write!(
                formatter,
                "descriptor matching attempted {attempted} distance evaluations; maximum is {maximum}"
            ),
            Self::MatchLimitExceeded { actual, maximum } => write!(
                formatter,
                "correspondence generation retained {actual} matches; maximum is {maximum}"
            ),
            Self::UnknownObservation { observation_id } => write!(
                formatter,
                "correspondence lookup could not find observation {observation_id}"
            ),
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "correspondence arithmetic overflowed while computing {field}"
            ),
            Self::DerivedMismatch { field } => write!(
                formatter,
                "correspondence derived field {field} disagrees with deterministic replay"
            ),
            Self::Codec(error) => write!(formatter, "correspondence codec error: {error}"),
            Self::Domain(error) => write!(
                formatter,
                "correspondence identity-domain error: {error}"
            ),
            Self::JsonRendering(error) => write!(
                formatter,
                "correspondence JSON rendering failed: {error}"
            ),
        }
    }
}

impl Error for CorrespondenceError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for CorrespondenceError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for CorrespondenceError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
