/// Stable validation, arithmetic, scoring, replay, and encoding failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum RelativePoseError {
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
    /// The left/right keyframe pair was not canonical.
    NonCanonicalFramePair,
    /// No bearing matches were supplied.
    EmptyMatches,
    /// No pose candidates were supplied.
    EmptyCandidates,
    /// A match identity was duplicated.
    DuplicateMatchId {
        /// Duplicate identity.
        match_id: u64,
    },
    /// A left/right observation pair was duplicated.
    DuplicateObservationPair {
        /// Left observation identity.
        left_observation_id: u64,
        /// Right observation identity.
        right_observation_id: u64,
    },
    /// A candidate identity was duplicated.
    DuplicateCandidateId {
        /// Duplicate identity.
        candidate_id: u64,
    },
    /// A unit bearing or translation direction was outside tolerance.
    UnitVectorInvalid {
        /// Stable field name.
        field: &'static str,
        /// Observed squared-norm error in parts per million.
        error_ppm: u64,
        /// Maximum error.
        maximum_ppm: u64,
    },
    /// A rotation row or column was not unit length.
    RotationNormInvalid {
        /// Row or column label.
        field: &'static str,
        /// Observed error.
        error_ppm: u64,
    },
    /// Two rotation rows or columns were not orthogonal.
    RotationOrthogonalityInvalid {
        /// Stable pair label.
        field: &'static str,
        /// Observed normalized dot magnitude.
        error_ppm: u64,
    },
    /// Rotation determinant was not positive one within tolerance.
    RotationDeterminantInvalid {
        /// Observed determinant error.
        error_ppm: u64,
    },
    /// Candidate/match work exceeded the explicit operation budget.
    EvaluationBudgetExceeded {
        /// Required evaluation count.
        required: u64,
        /// Admitted maximum.
        maximum: u64,
    },
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation name.
        field: &'static str,
    },
    /// An internal lookup failed after validated canonicalization.
    InternalLookup {
        /// Stable lookup name.
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

impl Display for RelativePoseError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => {
                write!(formatter, "relative-pose identity {field} must not be all zero")
            }
            Self::ZeroValue { field } => {
                write!(formatter, "relative-pose field {field} must be nonzero")
            }
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "relative-pose field {field} is {actual}; maximum is {maximum}"
            ),
            Self::NonCanonicalFramePair => formatter.write_str(
                "relative-pose basis must satisfy left_sample_index < right_sample_index",
            ),
            Self::EmptyMatches => {
                formatter.write_str("relative-pose verification requires bearing matches")
            }
            Self::EmptyCandidates => {
                formatter.write_str("relative-pose verification requires candidates")
            }
            Self::DuplicateMatchId { match_id } => {
                write!(formatter, "bearing-match identity {match_id} is duplicated")
            }
            Self::DuplicateObservationPair {
                left_observation_id,
                right_observation_id,
            } => write!(
                formatter,
                "bearing observation pair ({left_observation_id}, {right_observation_id}) is duplicated"
            ),
            Self::DuplicateCandidateId { candidate_id } => {
                write!(formatter, "relative-pose candidate {candidate_id} is duplicated")
            }
            Self::UnitVectorInvalid {
                field,
                error_ppm,
                maximum_ppm,
            } => write!(
                formatter,
                "relative-pose vector {field} has norm error {error_ppm} ppm; maximum is {maximum_ppm} ppm"
            ),
            Self::RotationNormInvalid { field, error_ppm } => write!(
                formatter,
                "relative-pose rotation {field} has norm error {error_ppm} ppm"
            ),
            Self::RotationOrthogonalityInvalid { field, error_ppm } => write!(
                formatter,
                "relative-pose rotation {field} has orthogonality error {error_ppm} ppm"
            ),
            Self::RotationDeterminantInvalid { error_ppm } => write!(
                formatter,
                "relative-pose rotation determinant error is {error_ppm} ppm"
            ),
            Self::EvaluationBudgetExceeded { required, maximum } => write!(
                formatter,
                "relative-pose verification requires {required} evaluations; maximum is {maximum}"
            ),
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "relative-pose arithmetic overflowed while computing {field}"
            ),
            Self::InternalLookup { field } => {
                write!(formatter, "relative-pose internal lookup failed for {field}")
            }
            Self::DerivedMismatch { field } => write!(
                formatter,
                "relative-pose derived field {field} disagrees with deterministic replay"
            ),
            Self::Codec(error) => write!(formatter, "relative-pose codec error: {error}"),
            Self::Domain(error) => write!(
                formatter,
                "relative-pose identity-domain error: {error}"
            ),
            Self::JsonRendering(error) => write!(
                formatter,
                "relative-pose JSON rendering failed: {error}"
            ),
        }
    }
}

impl Error for RelativePoseError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for RelativePoseError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for RelativePoseError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
