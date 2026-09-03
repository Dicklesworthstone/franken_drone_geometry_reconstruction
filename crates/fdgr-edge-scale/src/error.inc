/// Stable relative edge-scale construction errors.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum EdgeScaleError {
    /// A mandatory digest was all zero.
    ZeroIdentity { field: &'static str },
    /// A mandatory numeric field was zero.
    ZeroValue { field: &'static str },
    /// A constitutional bound was exceeded.
    BoundExceeded { field: &'static str, actual: u128, maximum: u128 },
    /// A subject field was duplicated.
    DuplicateSubject { field: &'static str, value: u64 },
    /// A witness identity was duplicated.
    DuplicateWitness { witness_id: u64 },
    /// A witness did not use canonical endpoint order.
    NonCanonicalWitness { witness_id: u64 },
    /// A witness referenced an unknown pose edge.
    UnknownEdge { witness_id: u64, edge_id: u64 },
    /// A positive exact ratio could not be represented in the fixed component gauge.
    UnrepresentableRatio { witness_id: u64 },
    /// An exact derived basis disagreed with the supplied identity.
    BasisMismatch {
        /// Stable basis field.
        field: &'static str,
        /// Required identity.
        expected: EvidenceDigest,
        /// Independently derived identity.
        observed: EvidenceDigest,
    },
    /// A semantic policy combination was internally impossible.
    InvalidPolicy { field: &'static str },
    /// A validated intermediate object lacked an expected record.
    InternalLookup { field: &'static str },
    /// Checked arithmetic overflowed.
    ArithmeticOverflow { field: &'static str },
    /// Deterministic replay disagreed with retained output.
    DerivedMismatch { field: &'static str },
    /// Pose-graph validation or identity derivation failed.
    PoseGraph(PoseGraphError),
    /// Graph analysis failed.
    Graph(GraphError),
    /// Canonical codec failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for EdgeScaleError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => write!(formatter, "edge-scale identity {field} must not be all zero"),
            Self::ZeroValue { field } => write!(formatter, "edge-scale field {field} must be nonzero"),
            Self::BoundExceeded { field, actual, maximum } => write!(formatter, "edge-scale field {field} is {actual}; maximum is {maximum}"),
            Self::DuplicateSubject { field, value } => write!(formatter, "edge-scale subject {field} value {value} is duplicated"),
            Self::DuplicateWitness { witness_id } => write!(formatter, "edge-scale witness {witness_id} is duplicated"),
            Self::NonCanonicalWitness { witness_id } => write!(formatter, "edge-scale witness {witness_id} must satisfy lower_edge_id < higher_edge_id"),
            Self::UnknownEdge { witness_id, edge_id } => write!(formatter, "edge-scale witness {witness_id} references unknown pose edge {edge_id}"),
            Self::UnrepresentableRatio { witness_id } => write!(formatter, "edge-scale witness {witness_id} ratio is outside the representable component gauge"),
            Self::BasisMismatch { field, expected, observed } => write!(formatter, "edge-scale basis {field} expected {expected}, observed {observed}"),
            Self::InvalidPolicy { field } => write!(formatter, "edge-scale policy {field} is internally invalid"),
            Self::InternalLookup { field } => write!(formatter, "edge-scale internal lookup failed for {field}"),
            Self::ArithmeticOverflow { field } => write!(formatter, "edge-scale arithmetic overflowed while computing {field}"),
            Self::DerivedMismatch { field } => write!(formatter, "edge-scale derived field {field} disagrees with deterministic replay"),
            Self::PoseGraph(error) => write!(formatter, "edge-scale pose-graph error: {error}"),
            Self::Graph(error) => write!(formatter, "edge-scale graph error: {error}"),
            Self::Codec(error) => write!(formatter, "edge-scale codec error: {error}"),
            Self::Domain(error) => write!(formatter, "edge-scale identity-domain error: {error}"),
            Self::JsonRendering(error) => write!(formatter, "edge-scale JSON rendering failed: {error}"),
        }
    }
}

impl Error for EdgeScaleError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::PoseGraph(error) => Some(error),
            Self::Graph(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<PoseGraphError> for EdgeScaleError { fn from(error: PoseGraphError) -> Self { Self::PoseGraph(error) } }
impl From<GraphError> for EdgeScaleError { fn from(error: GraphError) -> Self { Self::Graph(error) } }
impl From<CodecError> for EdgeScaleError { fn from(error: CodecError) -> Self { Self::Codec(error) } }
impl From<DomainError> for EdgeScaleError { fn from(error: DomainError) -> Self { Self::Domain(error) } }
