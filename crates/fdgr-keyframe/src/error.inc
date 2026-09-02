/// Stable validation, selection, replay, and encoding failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum KeyframeError {
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
    /// A bounded integer exceeded its constitutional maximum.
    BoundExceeded {
        /// Stable field name.
        field: &'static str,
        /// Observed magnitude.
        actual: u128,
        /// Maximum magnitude.
        maximum: u128,
    },
    /// No candidate frames were supplied.
    EmptyCandidates,
    /// A sample index occurred more than once.
    DuplicateSampleIndex {
        /// Duplicate sample index.
        sample_index: u64,
    },
    /// A decoded-frame identity occurred more than once.
    DuplicateFrameDigest {
        /// Duplicate frame identity.
        frame_digest: EvidenceDigest,
    },
    /// Candidate visibility cells were not strictly increasing and nonzero.
    NonCanonicalCoverageCells {
        /// Candidate sample index.
        sample_index: u64,
    },
    /// Total candidate-to-cell references exceeded the hard bound.
    CoverageReferenceLimitExceeded {
        /// Observed references.
        actual: usize,
        /// Maximum references.
        maximum: usize,
    },
    /// Every selection weight was zero.
    ZeroSelectionWeights,
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation name.
        field: &'static str,
    },
    /// Deterministic replay disagreed with the retained result.
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

impl Display for KeyframeError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => {
                write!(formatter, "keyframe identity {field} must not be all zero")
            }
            Self::ZeroValue { field } => {
                write!(formatter, "keyframe field {field} must be nonzero")
            }
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "keyframe field {field} is {actual}; maximum is {maximum}"
            ),
            Self::EmptyCandidates => formatter.write_str("keyframe selection requires candidates"),
            Self::DuplicateSampleIndex { sample_index } => write!(
                formatter,
                "keyframe candidate sample index {sample_index} is duplicated"
            ),
            Self::DuplicateFrameDigest { frame_digest } => write!(
                formatter,
                "keyframe candidate frame digest {frame_digest} is duplicated"
            ),
            Self::NonCanonicalCoverageCells { sample_index } => write!(
                formatter,
                "keyframe candidate {sample_index} coverage cells must be strictly increasing and nonzero"
            ),
            Self::CoverageReferenceLimitExceeded { actual, maximum } => write!(
                formatter,
                "keyframe candidates contain {actual} coverage references; maximum is {maximum}"
            ),
            Self::ZeroSelectionWeights => {
                formatter.write_str("at least one keyframe selection weight must be nonzero")
            }
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "keyframe arithmetic overflowed while computing {field}"
            ),
            Self::DerivedMismatch { field } => write!(
                formatter,
                "keyframe derived field {field} disagrees with deterministic replay"
            ),
            Self::Codec(error) => write!(formatter, "keyframe codec error: {error}"),
            Self::Domain(error) => write!(formatter, "keyframe identity-domain error: {error}"),
            Self::JsonRendering(error) => {
                write!(formatter, "keyframe JSON rendering failed: {error}")
            }
        }
    }
}

impl Error for KeyframeError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for KeyframeError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for KeyframeError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
