/// Public schema identity for one graph analysis.
pub const GRAPH_ANALYSIS_SCHEMA: &str = "fdgr.graph_analysis/1";
/// Maximum nodes in one reference analysis.
pub const MAX_GRAPH_NODES: usize = 100_000;
/// Maximum edges in one reference analysis.
pub const MAX_GRAPH_EDGES: usize = 500_000;
/// Maximum path-search expansions in one reference analysis.
pub const MAX_PATH_EXPANSIONS: u64 = 100_000_000;
/// Largest integer emitted as an unquoted JSON number.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable graph basis.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GraphBasis {
    /// Exact node-table identity.
    pub node_basis_digest: EvidenceDigest,
    /// Exact edge-table identity.
    pub edge_basis_digest: EvidenceDigest,
    /// Exact semantic edge-priority policy identity.
    pub selection_policy_digest: EvidenceDigest,
    /// Immutable analysis generation.
    pub generation: u64,
}

/// One canonical graph node.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GraphNode {
    /// Stable nonzero node identity.
    pub node_id: u64,
    /// Exact domain evidence represented by this node.
    pub evidence_digest: EvidenceDigest,
}

/// Deterministic maximum-forest ordering key.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct EdgePriority {
    /// Higher values are preferred.
    pub primary: u64,
    /// Lower values are preferred after the primary key.
    pub secondary: u64,
}

/// One canonical undirected graph edge.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GraphEdge {
    /// Stable nonzero edge identity.
    pub edge_id: u64,
    /// Canonical lower endpoint.
    pub left_node_id: u64,
    /// Canonical higher endpoint.
    pub right_node_id: u64,
    /// Domain-defined deterministic forest priority.
    pub priority: EdgePriority,
    /// Exact evidence admitting this edge.
    pub evidence_digest: EvidenceDigest,
}

/// Operation ceiling for the deliberately simple reference implementation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct GraphBudget {
    /// Maximum breadth-first path expansions used to materialize fundamental cycles.
    pub max_path_expansions: u64,
}

impl Default for GraphBudget {
    fn default() -> Self {
        Self {
            max_path_expansions: 10_000_000,
        }
    }
}

/// One connected component in canonical order.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GraphComponent {
    /// Stable component identity equal to its smallest node identity.
    pub component_id: u64,
    /// Sorted member node identities.
    pub node_ids: Vec<u64>,
    /// Sorted member edge identities.
    pub edge_ids: Vec<u64>,
    /// Sorted maximum-priority forest edges in this component.
    pub forest_edge_ids: Vec<u64>,
}

/// One fundamental cycle induced by a non-forest edge.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FundamentalCycle {
    /// Non-forest edge closing the cycle.
    pub closing_edge_id: u64,
    /// Canonical lower endpoint of the closing edge.
    pub left_node_id: u64,
    /// Canonical higher endpoint of the closing edge.
    pub right_node_id: u64,
    /// Ordered forest path from the left endpoint to the right endpoint.
    pub forest_path_edge_ids: Vec<u64>,
}

/// Immutable deterministic graph analysis.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GraphAnalysis {
    /// Exact immutable basis.
    pub basis: GraphBasis,
    /// Canonically sorted nodes retained for replay.
    pub nodes: Vec<GraphNode>,
    /// Canonically sorted edges retained for replay.
    pub edges: Vec<GraphEdge>,
    /// Canonical connected components.
    pub components: Vec<GraphComponent>,
    /// Maximum-priority spanning-forest edge identities.
    pub forest_edge_ids: Vec<u64>,
    /// Non-forest edge identities.
    pub non_forest_edge_ids: Vec<u64>,
    /// Fundamental cycle witnesses, one per non-forest edge.
    pub fundamental_cycles: Vec<FundamentalCycle>,
    /// Edges absent from every cycle.
    pub bridge_edge_ids: Vec<u64>,
    /// Cyclomatic number `m - n + components`.
    pub cycle_rank: u64,
    /// Exact path-search expansions charged. Operation evidence only.
    pub path_expansions: u64,
}

/// Stable graph validation and analysis errors.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum GraphError {
    /// A mandatory digest was all zero.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A mandatory numeric value was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
    },
    /// A constitutional bound was exceeded.
    BoundExceeded {
        /// Stable field name.
        field: &'static str,
        /// Observed magnitude.
        actual: u128,
        /// Constitutional maximum.
        maximum: u128,
    },
    /// No nodes were supplied.
    EmptyNodes,
    /// A node identity was duplicated.
    DuplicateNode {
        /// Duplicate node identity.
        node_id: u64,
    },
    /// An edge identity was duplicated.
    DuplicateEdge {
        /// Duplicate edge identity.
        edge_id: u64,
    },
    /// Two edges named the same unordered endpoint pair.
    DuplicateEndpointPair {
        /// Canonical lower endpoint.
        left_node_id: u64,
        /// Canonical higher endpoint.
        right_node_id: u64,
    },
    /// An edge was a self-loop or had reversed endpoints.
    NonCanonicalEdge {
        /// Invalid edge identity.
        edge_id: u64,
    },
    /// An edge referenced a node absent from the exact node table.
    UnknownNode {
        /// Referencing edge identity.
        edge_id: u64,
        /// Missing node identity.
        node_id: u64,
    },
    /// A bounded path search exhausted its operation budget.
    PathBudgetExceeded {
        /// Attempted operation count.
        attempted: u64,
        /// Admitted maximum.
        maximum: u64,
    },
    /// A forest path could not be reconstructed after validated connectivity.
    ForestPathMissing {
        /// Requested path start.
        left_node_id: u64,
        /// Requested path end.
        right_node_id: u64,
    },
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation name.
        field: &'static str,
    },
    /// Deterministic replay disagreed with retained output.
    DerivedMismatch {
        /// Stable derived field name.
        field: &'static str,
    },
    /// Canonical codec failure.
    Codec(CodecError),
    /// Identity-domain failure.
    Domain(DomainError),
    /// JSON rendering failure.
    JsonRendering(String),
}

impl Display for GraphError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => write!(formatter, "graph identity {field} must not be all zero"),
            Self::ZeroValue { field } => write!(formatter, "graph field {field} must be nonzero"),
            Self::BoundExceeded { field, actual, maximum } => write!(formatter, "graph field {field} is {actual}; maximum is {maximum}"),
            Self::EmptyNodes => formatter.write_str("graph analysis requires at least one node"),
            Self::DuplicateNode { node_id } => write!(formatter, "graph node {node_id} is duplicated"),
            Self::DuplicateEdge { edge_id } => write!(formatter, "graph edge {edge_id} is duplicated"),
            Self::DuplicateEndpointPair { left_node_id, right_node_id } => write!(formatter, "graph endpoint pair ({left_node_id}, {right_node_id}) is duplicated"),
            Self::NonCanonicalEdge { edge_id } => write!(formatter, "graph edge {edge_id} must satisfy left_node_id < right_node_id"),
            Self::UnknownNode { edge_id, node_id } => write!(formatter, "graph edge {edge_id} references unknown node {node_id}"),
            Self::PathBudgetExceeded { attempted, maximum } => write!(formatter, "graph cycle materialization attempted {attempted} path expansions; maximum is {maximum}"),
            Self::ForestPathMissing { left_node_id, right_node_id } => write!(formatter, "graph forest path from {left_node_id} to {right_node_id} is missing"),
            Self::ArithmeticOverflow { field } => write!(formatter, "graph arithmetic overflowed while computing {field}"),
            Self::DerivedMismatch { field } => write!(formatter, "graph derived field {field} disagrees with deterministic replay"),
            Self::Codec(error) => write!(formatter, "graph codec error: {error}"),
            Self::Domain(error) => write!(formatter, "graph identity-domain error: {error}"),
            Self::JsonRendering(error) => write!(formatter, "graph JSON rendering failed: {error}"),
        }
    }
}

impl Error for GraphError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for GraphError {
    fn from(error: CodecError) -> Self { Self::Codec(error) }
}

impl From<DomainError> for GraphError {
    fn from(error: DomainError) -> Self { Self::Domain(error) }
}

