/// Public schema identity for one component-local pose-graph generation.
pub const POSE_GRAPH_GENERATION_SCHEMA: &str = "fdgr.pose_graph_generation/1";
/// Fixed rotation and unit-direction scale.
pub const UNIT_NANO: i64 = 1_000_000_000;
/// Parts-per-million denominator.
pub const PPM_SCALE: u64 = 1_000_000;
/// Maximum input rotation validation error.
pub const MAX_INPUT_ROTATION_ERROR_PPM: u64 = 10_000;
/// Maximum policy-bound cycle or composition error.
pub const MAX_ROTATION_ERROR_PPM: u64 = 2_000_000;
/// Largest public identity emitted as a JSON number.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one pose-graph generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseGraphBasis {
    /// Exact keyframe/node table identity.
    pub node_basis_digest: EvidenceDigest,
    /// Exact relative-pose edge table identity.
    pub edge_basis_digest: EvidenceDigest,
    /// Exact graph forest-selection policy identity.
    pub graph_selection_policy_digest: EvidenceDigest,
    /// Exact orientation-consistency policy identity.
    pub rotation_policy_digest: EvidenceDigest,
    /// Immutable generation.
    pub generation: u64,
}

/// One exact keyframe node.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseNode {
    /// Stable nonzero graph node identity.
    pub node_id: u64,
    /// Canonical source sample index.
    pub sample_index: u64,
    /// Exact keyframe evidence identity.
    pub keyframe_digest: EvidenceDigest,
}

/// One admitted pairwise relative-pose hypothesis.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RelativePoseEdge {
    /// Stable nonzero edge identity.
    pub edge_id: u64,
    /// Exact relative-pose verification identity.
    pub verification_digest: EvidenceDigest,
    /// Exact candidate admitted by that verification.
    pub admitted_candidate_id: u64,
    /// Canonical lower graph endpoint.
    pub left_node_id: u64,
    /// Canonical higher graph endpoint.
    pub right_node_id: u64,
    /// Source sample represented by the left endpoint.
    pub left_sample_index: u64,
    /// Source sample represented by the right endpoint.
    pub right_sample_index: u64,
    /// `R_right_from_left`, row-major and multiplied by [`UNIT_NANO`].
    pub rotation_right_from_left_nano: [i64; 9],
    /// Unit direction `t_right_from_left`, multiplied by [`UNIT_NANO`].
    pub translation_right_from_left_direction_nano: [i64; 3],
    /// Exact supported correspondence count used for forest priority.
    pub supported_match_count: u32,
    /// Median residual used as the lower-is-better secondary priority.
    pub median_residual_nano: u64,
}

/// Semantic cycle/composition gates plus a nonsemantic operation ceiling.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PoseGraphPolicy {
    /// Maximum entrywise rotation-cycle residual.
    pub max_rotation_cycle_residual_ppm: u64,
    /// Maximum accumulated component-orientation drift.
    pub max_orientation_drift_ppm: u64,
    /// Maximum graph path expansions; excluded from semantic identity.
    pub max_path_expansions: u64,
}

impl Default for PoseGraphPolicy {
    fn default() -> Self {
        Self {
            max_rotation_cycle_residual_ppm: 5_000,
            max_orientation_drift_ppm: 10_000,
            max_path_expansions: 10_000_000,
        }
    }
}

/// One component-local orientation rooted at an arbitrary canonical gauge.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NodeOrientation {
    /// Graph node identity.
    pub node_id: u64,
    /// Source sample index.
    pub sample_index: u64,
    /// Canonical root node of this connected component.
    pub component_root_node_id: u64,
    /// `R_node_from_component_root`, row-major and multiplied by [`UNIT_NANO`].
    pub rotation_node_from_root_nano: [i64; 9],
    /// Tree parent node, absent for the component root.
    pub parent_node_id: Option<u64>,
    /// Tree edge supplying the orientation, absent for the component root.
    pub parent_edge_id: Option<u64>,
}

/// Rotation status of one fundamental cycle.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RotationCycleStatus {
    /// Measured and forest-implied relative rotations agree within policy.
    Consistent,
    /// The cycle residual exceeds policy.
    Conflicting,
}

impl RotationCycleStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Consistent => "consistent",
            Self::Conflicting => "conflicting",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Consistent => 1,
            Self::Conflicting => 2,
        }
    }
}

/// One fundamental rotation-cycle assessment.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RotationCycleAssessment {
    /// Non-forest edge closing the cycle.
    pub closing_edge_id: u64,
    /// Ordered forest path from the edge's left endpoint to its right endpoint.
    pub forest_path_edge_ids: Vec<u64>,
    /// Conservative entrywise residual in parts per million.
    pub rotation_residual_ppm: u64,
    /// Policy disposition.
    pub status: RotationCycleStatus,
}

/// Component-level orientation maturity.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum OrientationStatus {
    /// A component has no redundant cycle evidence.
    TreeOnly,
    /// Every observed fundamental cycle is consistent.
    CycleConsistent,
    /// At least one fundamental cycle conflicts.
    Conflicted,
}

impl OrientationStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::TreeOnly => "tree_only",
            Self::CycleConsistent => "cycle_consistent",
            Self::Conflicted => "conflicted",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::TreeOnly => 1,
            Self::CycleConsistent => 2,
            Self::Conflicted => 3,
        }
    }
}

/// Translation maturity is explicit and never inferred from unit edge directions.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TranslationStatus {
    /// Pairwise directions exist, but edge baselines are not mutually synchronized.
    DirectionOnlyScaleUnderdetermined,
}

impl TranslationStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        "direction_only_scale_underdetermined"
    }

    const fn code(self) -> u8 { 1 }
}

/// One connected pose-graph component.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseGraphComponent {
    /// Canonical root node.
    pub component_root_node_id: u64,
    /// Sorted member nodes.
    pub node_ids: Vec<u64>,
    /// Sorted member edges.
    pub edge_ids: Vec<u64>,
    /// Sorted forest edges.
    pub forest_edge_ids: Vec<u64>,
    /// Orientation maturity.
    pub orientation_status: OrientationStatus,
    /// Sorted cycle-closing edges whose rotations conflict.
    pub conflicting_edge_ids: Vec<u64>,
}

/// Immutable pose-graph orientation generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseGraphGeneration {
    /// Exact immutable basis.
    pub basis: PoseGraphBasis,
    /// Exact instantiated policy.
    pub policy: PoseGraphPolicy,
    /// Canonically sorted input nodes.
    pub nodes: Vec<PoseNode>,
    /// Canonically sorted input edges.
    pub edges: Vec<RelativePoseEdge>,
    /// Exact deterministic topology analysis.
    pub topology: GraphAnalysis,
    /// Component-local orientations in canonical node order.
    pub orientations: Vec<NodeOrientation>,
    /// Fundamental cycle assessments in closing-edge order.
    pub cycle_assessments: Vec<RotationCycleAssessment>,
    /// Pose-graph components.
    pub components: Vec<PoseGraphComponent>,
    /// Translation maturity. No camera centers are emitted.
    pub translation_status: TranslationStatus,
}

/// Stable pose-graph construction errors.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum PoseGraphError {
    /// A mandatory digest was all zero.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A mandatory number was zero.
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
        /// Maximum admitted magnitude.
        maximum: u128,
    },
    /// A node identity or sample was duplicated.
    DuplicateNode {
        /// Duplicate field class.
        field: &'static str,
        /// Duplicate value.
        value: u64,
    },
    /// An edge identity, endpoint pair, or verification/candidate pair was duplicated.
    DuplicateEdge {
        /// Duplicate field class.
        field: &'static str,
        /// Stable value identifying the duplicate.
        value: u64,
    },
    /// An edge endpoint order was not canonical.
    NonCanonicalEdge {
        /// Invalid edge identity.
        edge_id: u64,
    },
    /// An edge referenced an unknown node.
    UnknownNode {
        /// Referencing edge identity.
        edge_id: u64,
        /// Missing node identity.
        node_id: u64,
    },
    /// Edge sample metadata disagreed with its endpoint node.
    SampleMismatch {
        /// Edge identity.
        edge_id: u64,
        /// Endpoint node identity.
        node_id: u64,
        /// Sample index declared by the node.
        expected: u64,
        /// Sample index declared by the edge.
        observed: u64,
    },
    /// A rotation failed the constitutional rigid-rotation gate.
    RotationInvalid {
        /// Rotation field or stage.
        field: &'static str,
        /// Observed aggregate error.
        error_ppm: u64,
        /// Maximum admitted error.
        maximum_ppm: u64,
    },
    /// A translation direction was not unit length.
    TranslationDirectionInvalid {
        /// Edge identity.
        edge_id: u64,
        /// Observed squared-norm error.
        error_ppm: u64,
    },
    /// Component orientation drift exceeded policy.
    OrientationDriftExceeded {
        /// Node whose composed orientation exceeded the gate.
        node_id: u64,
        /// Observed aggregate error.
        error_ppm: u64,
        /// Maximum admitted error.
        maximum_ppm: u64,
    },
    /// A validated topology object lacked an expected edge, node, or orientation.
    InternalLookup {
        /// Stable lookup name.
        field: &'static str,
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
    /// Graph-topology analysis failed.
    Graph(GraphError),
    /// Canonical codec failure.
    Codec(CodecError),
    /// Identity-domain failure.
    Domain(DomainError),
    /// JSON rendering failure.
    JsonRendering(String),
}

impl Display for PoseGraphError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => write!(formatter, "pose-graph identity {field} must not be all zero"),
            Self::ZeroValue { field } => write!(formatter, "pose-graph field {field} must be nonzero"),
            Self::BoundExceeded { field, actual, maximum } => write!(formatter, "pose-graph field {field} is {actual}; maximum is {maximum}"),
            Self::DuplicateNode { field, value } => write!(formatter, "pose-graph node {field} value {value} is duplicated"),
            Self::DuplicateEdge { field, value } => write!(formatter, "pose-graph edge {field} value {value} is duplicated"),
            Self::NonCanonicalEdge { edge_id } => write!(formatter, "pose-graph edge {edge_id} must satisfy left_node_id < right_node_id"),
            Self::UnknownNode { edge_id, node_id } => write!(formatter, "pose-graph edge {edge_id} references unknown node {node_id}"),
            Self::SampleMismatch { edge_id, node_id, expected, observed } => write!(formatter, "pose-graph edge {edge_id} endpoint {node_id} expected sample {expected}, observed {observed}"),
            Self::RotationInvalid { field, error_ppm, maximum_ppm } => write!(formatter, "pose-graph rotation {field} has error {error_ppm} ppm; maximum is {maximum_ppm}"),
            Self::TranslationDirectionInvalid { edge_id, error_ppm } => write!(formatter, "pose-graph edge {edge_id} translation direction has norm error {error_ppm} ppm"),
            Self::OrientationDriftExceeded { node_id, error_ppm, maximum_ppm } => write!(formatter, "pose-graph node {node_id} accumulated orientation drift {error_ppm} ppm; maximum is {maximum_ppm}"),
            Self::InternalLookup { field } => write!(formatter, "pose-graph internal lookup failed for {field}"),
            Self::ArithmeticOverflow { field } => write!(formatter, "pose-graph arithmetic overflowed while computing {field}"),
            Self::DerivedMismatch { field } => write!(formatter, "pose-graph derived field {field} disagrees with deterministic replay"),
            Self::Graph(error) => write!(formatter, "pose-graph topology error: {error}"),
            Self::Codec(error) => write!(formatter, "pose-graph codec error: {error}"),
            Self::Domain(error) => write!(formatter, "pose-graph identity-domain error: {error}"),
            Self::JsonRendering(error) => write!(formatter, "pose-graph JSON rendering failed: {error}"),
        }
    }
}

impl Error for PoseGraphError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Graph(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<GraphError> for PoseGraphError { fn from(error: GraphError) -> Self { Self::Graph(error) } }
impl From<CodecError> for PoseGraphError { fn from(error: CodecError) -> Self { Self::Codec(error) } }
impl From<DomainError> for PoseGraphError { fn from(error: DomainError) -> Self { Self::Domain(error) } }
