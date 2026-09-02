/// Stable basis, matrix, residual, budget, replay, and encoding failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum EpipolarError {
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
    /// The frame pair was not in canonical sample order.
    NonCanonicalFramePair,
    /// No normalized correspondences were supplied.
    EmptyObservations,
    /// No essential candidates were supplied.
    EmptyCandidates,
    /// A match identity was duplicated.
    DuplicateMatchId {
        /// Duplicate match identity.
        match_id: u64,
    },
    /// The same pair of feature observations occurred more than once.
    DuplicateObservationPair {
        /// Left feature-observation identity.
        left_observation_id: u64,
        /// Right feature-observation identity.
        right_observation_id: u64,
    },
    /// One observation named a different frame-pair basis.
    PairIdentityMismatch {
        /// Match identity.
        match_id: u64,
        /// Required pair identity.
        expected: u64,
        /// Observed pair identity.
        observed: u64,
    },
    /// A candidate identity was duplicated.
    DuplicateCandidateId {
        /// Duplicate candidate identity.
        candidate_id: u64,
    },
    /// Two candidate identities represented the same homogeneous matrix.
    DuplicateCandidateMatrix {
        /// First candidate identity.
        first_candidate_id: u64,
        /// Duplicate candidate identity.
        duplicate_candidate_id: u64,
    },
    /// An essential candidate was the zero matrix.
    ZeroMatrix {
        /// Candidate identity.
        candidate_id: u64,
    },
    /// A matrix coefficient exceeded the raw or normalized bound.
    MatrixCoefficientExceeded {
        /// Candidate identity.
        candidate_id: u64,
        /// Stable bound stage.
        stage: &'static str,
        /// Observed magnitude.
        actual: u128,
        /// Maximum magnitude.
        maximum: u128,
    },
    /// Candidate/observation work exceeded the explicit operation budget.
    EvaluationBudgetExceeded {
        /// Required evaluations.
        required: u64,
        /// Admitted maximum.
        maximum: u64,
    },
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation name.
        field: &'static str,
    },
    /// Deterministic replay disagreed with a retained result.
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

impl Display for EpipolarError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => {
                write!(formatter, "epipolar identity {field} must not be all zero")
            }
            Self::ZeroValue { field } => {
                write!(formatter, "epipolar field {field} must be nonzero")
            }
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "epipolar field {field} is {actual}; maximum is {maximum}"
            ),
            Self::NonCanonicalFramePair => formatter.write_str(
                "epipolar basis must satisfy left_sample_index < right_sample_index",
            ),
            Self::EmptyObservations => {
                formatter.write_str("epipolar verification requires correspondences")
            }
            Self::EmptyCandidates => {
                formatter.write_str("epipolar verification requires essential candidates")
            }
            Self::DuplicateMatchId { match_id } => {
                write!(formatter, "epipolar match identity {match_id} is duplicated")
            }
            Self::DuplicateObservationPair {
                left_observation_id,
                right_observation_id,
            } => write!(
                formatter,
                "epipolar observation pair ({left_observation_id}, {right_observation_id}) is duplicated"
            ),
            Self::PairIdentityMismatch {
                match_id,
                expected,
                observed,
            } => write!(
                formatter,
                "epipolar match {match_id} names pair {observed}; expected {expected}"
            ),
            Self::DuplicateCandidateId { candidate_id } => write!(
                formatter,
                "essential candidate identity {candidate_id} is duplicated"
            ),
            Self::DuplicateCandidateMatrix {
                first_candidate_id,
                duplicate_candidate_id,
            } => write!(
                formatter,
                "essential candidates {first_candidate_id} and {duplicate_candidate_id} normalize to the same homogeneous matrix"
            ),
            Self::ZeroMatrix { candidate_id } => {
                write!(formatter, "essential candidate {candidate_id} is the zero matrix")
            }
            Self::MatrixCoefficientExceeded {
                candidate_id,
                stage,
                actual,
                maximum,
            } => write!(
                formatter,
                "essential candidate {candidate_id} {stage} coefficient magnitude is {actual}; maximum is {maximum}"
            ),
            Self::EvaluationBudgetExceeded { required, maximum } => write!(
                formatter,
                "epipolar verification requires {required} residual evaluations; maximum is {maximum}"
            ),
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "epipolar arithmetic overflowed while computing {field}"
            ),
            Self::DerivedMismatch { field } => write!(
                formatter,
                "epipolar derived field {field} disagrees with deterministic replay"
            ),
            Self::Codec(error) => write!(formatter, "epipolar codec error: {error}"),
            Self::Domain(error) => {
                write!(formatter, "epipolar identity-domain error: {error}")
            }
            Self::JsonRendering(error) => {
                write!(formatter, "epipolar JSON rendering failed: {error}")
            }
        }
    }
}

impl Error for EpipolarError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for EpipolarError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for EpipolarError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
