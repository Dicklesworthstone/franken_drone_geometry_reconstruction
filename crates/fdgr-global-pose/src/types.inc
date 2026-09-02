/// Public schema identity for one deterministic component-relative pose initialization.
pub const GLOBAL_POSE_INITIALIZATION_SCHEMA: &str = "fdgr.global_pose_initialization/1";
/// Canonical authority level of every emitted camera center.
pub const GLOBAL_POSE_AUTHORITY: &str = "relative_component_gauge";
/// Fixed-point scale shared by rotations, directions, relative edge scales, and positions.
pub const UNIT_NANO: i64 = 1_000_000_000;
/// Parts-per-million denominator.
pub const PPM_SCALE: u64 = 1_000_000;
/// Maximum public component-relative camera-center magnitude.
pub const MAX_CAMERA_CENTER_ABS_NANO: u64 = 9_000_000_000_000_000;
/// Maximum translation-cycle residual admitted by policy.
pub const MAX_TRANSLATION_RESIDUAL_PPM: u64 = 1_000_000;
/// Maximum deterministic operation count.
pub const MAX_GLOBAL_POSE_OPERATIONS: u64 = 100_000_000;
/// Largest integer emitted as an unquoted JSON number.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one global-pose initialization.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GlobalPoseBasis {
    /// Exact component-local pose-graph generation.
    pub pose_graph_generation_digest: EvidenceDigest,
    /// Exact relative edge-scale generation.
    pub edge_scale_generation_digest: EvidenceDigest,
    /// Exact global-initialization policy identity.
    pub policy_digest: EvidenceDigest,
    /// Immutable generation.
    pub generation: u64,
}

/// Semantic gates plus one nonsemantic operation ceiling.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct GlobalPosePolicy {
    /// Maximum normalized translation-cycle residual.
    pub max_translation_cycle_residual_ppm: u64,
    /// Maximum absolute component-relative camera-center coordinate.
    pub max_camera_center_abs_nano: u64,
    /// Hard deterministic operation ceiling, excluded from successful semantic identity.
    pub max_operations: u64,
}

impl Default for GlobalPosePolicy {
    fn default() -> Self {
        Self {
            max_translation_cycle_residual_ppm: 50_000,
            max_camera_center_abs_nano: 1_000_000_000_000_000,
            max_operations: 10_000_000,
        }
    }
}

/// One initialized camera pose in an explicit component-relative gauge.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct InitializedCameraPose {
    /// Stable pose-graph node identity.
    pub node_id: u64,
    /// Source sample index.
    pub sample_index: u64,
    /// Canonical pose-component root and zero-origin gauge.
    pub component_root_node_id: u64,
    /// Relative scale component used by the forest, absent for a singleton camera.
    pub scale_component_root_edge_id: Option<u64>,
    /// `R_node_from_component_root`, row-major and multiplied by [`UNIT_NANO`].
    pub rotation_node_from_root_nano: [i64; 9],
    /// Camera center in the component-root frame and component edge-scale unit, multiplied by
    /// [`UNIT_NANO`].
    pub camera_center_from_root_nano: [i64; 3],
    /// Deterministic forest parent, absent for the component root.
    pub parent_node_id: Option<u64>,
    /// Deterministic forest edge, absent for the component root.
    pub parent_edge_id: Option<u64>,
}

/// Translation-cycle disposition for one non-forest pose edge.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TranslationCycleStatus {
    /// Measured and forest-implied displacements agree within policy.
    Consistent,
    /// Comparable displacements disagree beyond policy.
    Conflicting,
    /// The closing edge belongs to another arbitrary scale gauge.
    IncomparableScaleGauge,
}

impl TranslationCycleStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Consistent => "consistent",
            Self::Conflicting => "conflicting",
            Self::IncomparableScaleGauge => "incomparable_scale_gauge",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Consistent => 1,
            Self::Conflicting => 2,
            Self::IncomparableScaleGauge => 3,
        }
    }
}

/// One translation-cycle assessment.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TranslationCycleAssessment {
    /// Non-forest pose edge closing the cycle.
    pub closing_edge_id: u64,
    /// Ordered forest path from the exact graph analysis.
    pub forest_path_edge_ids: Vec<u64>,
    /// Displacement implied by initialized camera centers.
    pub implied_displacement_nano: [i64; 3],
    /// Displacement measured by the closing edge, absent when its scale gauge is incomparable.
    pub measured_displacement_nano: Option<[i64; 3]>,
    /// Normalized residual, absent when scale gauges are incomparable.
    pub residual_ppm: Option<u64>,
    /// Conservative disposition.
    pub status: TranslationCycleStatus,
}

/// Maturity of one component-relative initialization.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum GlobalPoseComponentStatus {
    /// One camera defines a gauge without an edge.
    Singleton,
    /// A forest was initialized without comparable redundant translation evidence.
    TreeInitialized,
    /// Every comparable observed translation cycle is consistent.
    CycleConsistent,
    /// Upstream rotation-cycle evidence conflicts.
    OrientationConflicted,
    /// Upstream relative edge-scale evidence conflicts.
    ScaleConflicted,
    /// At least one comparable translation cycle conflicts.
    TranslationConflicted,
}

impl GlobalPoseComponentStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Singleton => "singleton",
            Self::TreeInitialized => "tree_initialized",
            Self::CycleConsistent => "cycle_consistent",
            Self::OrientationConflicted => "orientation_conflicted",
            Self::ScaleConflicted => "scale_conflicted",
            Self::TranslationConflicted => "translation_conflicted",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Singleton => 1,
            Self::TreeInitialized => 2,
            Self::CycleConsistent => 3,
            Self::OrientationConflicted => 4,
            Self::ScaleConflicted => 5,
            Self::TranslationConflicted => 6,
        }
    }
}

/// One initialized camera component.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GlobalPoseComponent {
    /// Canonical pose-component root and zero-origin gauge.
    pub component_root_node_id: u64,
    /// Relative scale-component root used by its forest, absent for a singleton.
    pub scale_component_root_edge_id: Option<u64>,
    /// Sorted member camera nodes.
    pub node_ids: Vec<u64>,
    /// Sorted forest pose edges.
    pub forest_edge_ids: Vec<u64>,
    /// Sorted non-forest pose edges.
    pub non_forest_edge_ids: Vec<u64>,
    /// Conservative component maturity.
    pub status: GlobalPoseComponentStatus,
    /// Whether the upstream scale component met its independent cross-validation requirement.
    pub scale_cross_validated: bool,
    /// Sorted comparable translation-conflicting closing edges.
    pub conflicting_edge_ids: Vec<u64>,
    /// Sorted closing edges whose arbitrary scale gauge was incomparable.
    pub incomparable_cycle_edge_ids: Vec<u64>,
}

/// Immutable deterministic component-relative pose initialization.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GlobalPoseInitialization {
    /// Exact immutable basis.
    pub basis: GlobalPoseBasis,
    /// Exact instantiated policy.
    pub policy: GlobalPosePolicy,
    /// Exact pose-graph input retained for deterministic replay.
    pub pose_graph: PoseGraphGeneration,
    /// Exact relative edge-scale input retained for deterministic replay.
    pub edge_scale: EdgeScaleGeneration,
    /// Initialized camera poses in canonical node order.
    pub poses: Vec<InitializedCameraPose>,
    /// Translation-cycle assessments in closing-edge order.
    pub translation_cycles: Vec<TranslationCycleAssessment>,
    /// Component summaries in root order.
    pub components: Vec<GlobalPoseComponent>,
    /// Exact deterministic work charged; operation evidence only.
    pub operation_count: u64,
}

/// Stable global-pose initialization failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum GlobalPoseError {
    /// A mandatory digest was the all-zero sentinel.
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
    /// An upstream semantic identity disagreed with the supplied basis.
    BasisMismatch {
        /// Stable basis field.
        field: &'static str,
        /// Required identity.
        expected: EvidenceDigest,
        /// Observed identity.
        observed: EvidenceDigest,
    },
    /// The edge-scale generation was derived from another pose graph.
    EdgeScalePoseGraphMismatch {
        /// Pose graph supplied to this operation.
        expected: EvidenceDigest,
        /// Pose graph bound by edge-scale evidence.
        observed: EvidenceDigest,
    },
    /// One connected camera forest requires more than one arbitrary edge-scale gauge.
    IncomparableScaleComponents {
        /// Pose-component root.
        component_root_node_id: u64,
        /// Sorted incompatible scale-component roots.
        scale_component_root_edge_ids: Vec<u64>,
    },
    /// An expected node, edge, orientation, scale, component, or cycle was absent.
    InternalLookup {
        /// Stable lookup name.
        field: &'static str,
    },
    /// A propagated camera center exceeded policy.
    PositionBoundExceeded {
        /// Camera node.
        node_id: u64,
        /// Observed absolute coordinate.
        actual: u128,
        /// Maximum admitted coordinate.
        maximum: u128,
    },
    /// Deterministic work exceeded its explicit operation ceiling.
    OperationBudgetExceeded {
        /// Attempted operation count.
        attempted: u64,
        /// Admitted maximum.
        maximum: u64,
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
    /// Pose-graph validation failed.
    PoseGraph(PoseGraphError),
    /// Edge-scale validation failed.
    EdgeScale(EdgeScaleError),
    /// Canonical codec failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for GlobalPoseError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => write!(formatter, "global-pose identity {field} must not be all zero"),
            Self::ZeroValue { field } => write!(formatter, "global-pose field {field} must be nonzero"),
            Self::BoundExceeded { field, actual, maximum } => write!(formatter, "global-pose field {field} is {actual}; maximum is {maximum}"),
            Self::BasisMismatch { field, expected, observed } => write!(formatter, "global-pose basis {field} expected {expected}, observed {observed}"),
            Self::EdgeScalePoseGraphMismatch { expected, observed } => write!(formatter, "edge-scale generation binds pose graph {observed}; expected {expected}"),
            Self::IncomparableScaleComponents { component_root_node_id, scale_component_root_edge_ids } => write!(formatter, "pose component {component_root_node_id} requires incomparable scale components {scale_component_root_edge_ids:?}"),
            Self::InternalLookup { field } => write!(formatter, "global-pose internal lookup failed for {field}"),
            Self::PositionBoundExceeded { node_id, actual, maximum } => write!(formatter, "global-pose node {node_id} camera-center magnitude {actual} exceeds {maximum}"),
            Self::OperationBudgetExceeded { attempted, maximum } => write!(formatter, "global-pose initialization attempted operation {attempted}; maximum is {maximum}"),
            Self::ArithmeticOverflow { field } => write!(formatter, "global-pose arithmetic overflowed while computing {field}"),
            Self::DerivedMismatch { field } => write!(formatter, "global-pose derived field {field} disagrees with deterministic replay"),
            Self::PoseGraph(error) => write!(formatter, "global-pose pose-graph error: {error}"),
            Self::EdgeScale(error) => write!(formatter, "global-pose edge-scale error: {error}"),
            Self::Codec(error) => write!(formatter, "global-pose codec error: {error}"),
            Self::Domain(error) => write!(formatter, "global-pose identity-domain error: {error}"),
            Self::JsonRendering(error) => write!(formatter, "global-pose JSON rendering failed: {error}"),
        }
    }
}

impl Error for GlobalPoseError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::PoseGraph(error) => Some(error),
            Self::EdgeScale(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<PoseGraphError> for GlobalPoseError {
    fn from(error: PoseGraphError) -> Self {
        Self::PoseGraph(error)
    }
}

impl From<EdgeScaleError> for GlobalPoseError {
    fn from(error: EdgeScaleError) -> Self {
        Self::EdgeScale(error)
    }
}

impl From<CodecError> for GlobalPoseError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for GlobalPoseError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
