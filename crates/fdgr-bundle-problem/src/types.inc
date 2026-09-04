/// Public schema identity for one deterministic structural bundle problem.
pub const BUNDLE_PROBLEM_SCHEMA: &str = "fdgr.bundle_problem/1";
/// Canonical authority emitted by this compiler.
pub const BUNDLE_PROBLEM_AUTHORITY: &str = "admitted_relative_bundle_problem";
/// Semantic policy identity domain.
pub const BUNDLE_PROBLEM_POLICY_SCHEMA: &str = "fdgr.bundle_problem_policy/1";
/// Camera-binding table identity domain.
pub const CAMERA_BINDING_TABLE_SCHEMA: &str = "fdgr.bundle_camera_binding_table/1";
/// Landmark-seed table identity domain.
pub const LANDMARK_SEED_TABLE_SCHEMA: &str = "fdgr.bundle_landmark_seed_table/1";
/// Bundle-observation table identity domain.
pub const BUNDLE_OBSERVATION_TABLE_SCHEMA: &str = "fdgr.bundle_observation_table/1";
/// Bipartite graph node-table identity domain.
pub const BUNDLE_GRAPH_NODE_TABLE_SCHEMA: &str = "fdgr.bundle_graph_node_table/1";
/// Bipartite graph edge-table identity domain.
pub const BUNDLE_GRAPH_EDGE_TABLE_SCHEMA: &str = "fdgr.bundle_graph_edge_table/1";
/// Fixed graph-selection policy identity domain.
pub const BUNDLE_GRAPH_SELECTION_POLICY_SCHEMA: &str =
    "fdgr.bundle_graph_selection_policy/1";
/// Fixed-point denominator shared by component-relative positions.
pub const UNIT_NANO: i64 = 1_000_000_000;
/// Maximum exact cameras plus landmark seeds in one problem.
pub const MAX_BUNDLE_GRAPH_NODES: usize = 100_000;
/// Maximum camera bindings.
pub const MAX_BUNDLE_CAMERAS: usize = 100_000;
/// Maximum landmark seeds.
pub const MAX_BUNDLE_LANDMARKS: usize = 100_000;
/// Maximum optimize plus held-out observations.
pub const MAX_BUNDLE_OBSERVATIONS: usize = 500_000;
/// Maximum absolute component-relative landmark coordinate.
pub const MAX_LANDMARK_POSITION_ABS_NANO: u64 = 9_000_000_000_000_000;
/// Maximum landmark-seed uncertainty.
pub const MAX_LANDMARK_UNCERTAINTY_NANO: u64 = 9_000_000_000_000_000;
/// Maximum absolute fixed-point image coordinate.
pub const MAX_ABS_NANO_PIXEL: i64 = 1_000_000_000_000_000;
/// Maximum localization uncertainty.
pub const MAX_OBSERVATION_UNCERTAINTY_NANO_PIXELS: u64 = 1_000_000_000_000;
/// Maximum deterministic operation count.
pub const MAX_BUNDLE_OPERATIONS: u64 = 1_000_000_000;
/// Maximum graph path expansions delegated to the graph oracle.
pub const MAX_BUNDLE_GRAPH_PATH_EXPANSIONS: u64 = 100_000_000;
/// Largest public integer emitted as an unquoted JSON number.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one structural bundle problem.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleProblemBasis {
    /// Exact pose-refinement generation.
    pub pose_refinement_digest: EvidenceDigest,
    /// Exact canonical camera-binding table.
    pub camera_binding_basis_digest: EvidenceDigest,
    /// Exact canonical landmark-seed table.
    pub landmark_seed_basis_digest: EvidenceDigest,
    /// Exact canonical observation table.
    pub observation_basis_digest: EvidenceDigest,
    /// Exact semantic compiler policy.
    pub policy_digest: EvidenceDigest,
    /// Immutable generation.
    pub generation: u64,
}

/// Semantic support and held-out gates plus nonsemantic execution ceilings.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct BundleProblemPolicy {
    /// Minimum independent optimize cameras required to retain a landmark.
    pub min_optimize_cameras_per_landmark: u32,
    /// Minimum active landmarks required to retain a non-root camera.
    pub min_active_landmarks_per_camera: u32,
    /// Minimum active landmarks that must directly observe the fixed gauge root.
    pub min_root_active_landmarks: u32,
    /// Maximum localization uncertainty admitted to optimize or held-out evidence.
    pub max_observation_uncertainty_nano_pixels: u64,
    /// Minimum eligible held-out observations required for an `admit` decision.
    pub min_held_out_observations_per_component: u32,
    /// Minimum distinct eligible held-out cameras required for an `admit` decision.
    pub min_held_out_cameras_per_component: u32,
    /// Hard graph path-expansion ceiling, excluded from successful semantic identity.
    pub max_graph_path_expansions: u64,
    /// Hard deterministic compiler-operation ceiling, excluded from successful semantic identity.
    pub max_operations: u64,
}

impl Default for BundleProblemPolicy {
    fn default() -> Self {
        Self {
            min_optimize_cameras_per_landmark: 2,
            min_active_landmarks_per_camera: 2,
            min_root_active_landmarks: 2,
            max_observation_uncertainty_nano_pixels: 2_000_000_000,
            min_held_out_observations_per_component: 2,
            min_held_out_cameras_per_component: 1,
            max_graph_path_expansions: 10_000_000,
            max_operations: 100_000_000,
        }
    }
}

/// One exact camera-to-frame-and-calibration binding.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleCameraBinding {
    /// Stable pose-graph camera node.
    pub camera_node_id: u64,
    /// Exact source sample retained by the pose generation.
    pub sample_index: u64,
    /// Exact decoded-frame object identity.
    pub frame_digest: EvidenceDigest,
    /// Exact effective calibration identity for this frame domain.
    pub effective_calibration_digest: EvidenceDigest,
}

/// One component-relative landmark proposal.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleLandmarkSeed {
    /// Stable nonzero landmark identity.
    pub landmark_id: u64,
    /// Exact source multi-view track identity.
    pub source_track_id: u64,
    /// Canonical pose-component root.
    pub component_root_node_id: u64,
    /// Exact arbitrary relative-scale gauge, absent only for a singleton pose component.
    pub scale_component_root_edge_id: Option<u64>,
    /// Exact evidence supporting the seed proposal.
    pub seed_evidence_digest: EvidenceDigest,
    /// Proposed component-relative position multiplied by [`UNIT_NANO`].
    pub seed_position_from_root_nano: [i64; 3],
    /// Declared symmetric seed uncertainty in component-relative nano units.
    pub seed_uncertainty_nano: u64,
}

/// Whether an observation trains or independently adjudicates a future optimizer.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleObservationRole {
    /// Observation may enter the future factor graph.
    Optimize,
    /// Observation is retained outside the factor graph for later adjudication.
    HeldOut,
}

impl BundleObservationRole {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Optimize => "optimize",
            Self::HeldOut => "held_out",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Optimize => 1,
            Self::HeldOut => 2,
        }
    }
}

/// One exact camera observation of a landmark proposal.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleObservation {
    /// Stable nonzero observation identity.
    pub observation_id: u64,
    /// Stable landmark identity.
    pub landmark_id: u64,
    /// Stable camera node identity.
    pub camera_node_id: u64,
    /// Source sample repeated for direct camera-binding verification.
    pub sample_index: u64,
    /// Exact frame identity repeated for direct camera-binding verification.
    pub frame_digest: EvidenceDigest,
    /// Exact source feature-observation identity.
    pub source_feature_observation_id: u64,
    /// Exact evidence supporting this observation.
    pub evidence_digest: EvidenceDigest,
    /// Horizontal coordinate in nano-pixels.
    pub x_nano_pixels: i64,
    /// Vertical coordinate in nano-pixels.
    pub y_nano_pixels: i64,
    /// Symmetric localization uncertainty in nano-pixels.
    pub localization_uncertainty_nano_pixels: u64,
    /// Whether an admitted dynamic mask covers the observation.
    pub dynamic_masked: bool,
    /// Optimize versus held-out role.
    pub role: BundleObservationRole,
}

/// Structural use of one camera after fixed-point support pruning.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleCameraDisposition {
    /// Fixed zero-origin camera retained regardless of support.
    ActiveGaugeRoot,
    /// Non-root camera retained in the optimize core.
    ActiveTraining,
    /// Non-root camera removed for insufficient active landmark support.
    PrunedInsufficientLandmarkSupport,
}

impl BundleCameraDisposition {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ActiveGaugeRoot => "active_gauge_root",
            Self::ActiveTraining => "active_training",
            Self::PrunedInsufficientLandmarkSupport => {
                "pruned_insufficient_landmark_support"
            }
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ActiveGaugeRoot => 1,
            Self::ActiveTraining => 2,
            Self::PrunedInsufficientLandmarkSupport => 3,
        }
    }
}

/// Structural use of one landmark proposal after fixed-point support pruning.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleLandmarkDisposition {
    /// Landmark retained in the optimize core.
    ActiveTraining,
    /// Landmark removed for insufficient independent camera support.
    PrunedInsufficientCameraSupport,
}

impl BundleLandmarkDisposition {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ActiveTraining => "active_training",
            Self::PrunedInsufficientCameraSupport => {
                "pruned_insufficient_camera_support"
            }
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ActiveTraining => 1,
            Self::PrunedInsufficientCameraSupport => 2,
        }
    }
}

/// Stable final disposition of every supplied observation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleObservationDisposition {
    /// Optimize observation retained in the factor graph.
    ActiveOptimize,
    /// Optimize observation rejected because a dynamic mask covered it.
    OptimizeRejectedDynamicMasked,
    /// Optimize observation rejected because uncertainty exceeded policy.
    OptimizeRejectedUncertaintyExceeded,
    /// Optimize observation removed with an unsupported landmark.
    OptimizePrunedLandmarkSupport,
    /// Optimize observation removed with an unsupported non-root camera.
    OptimizePrunedCameraSupport,
    /// Held-out observation eligible to adjudicate an active landmark.
    EligibleHeldOut,
    /// Held-out observation rejected because a dynamic mask covered it.
    HeldOutRejectedDynamicMasked,
    /// Held-out observation rejected because uncertainty exceeded policy.
    HeldOutRejectedUncertaintyExceeded,
    /// Held-out observation cannot adjudicate a landmark absent from the optimize core.
    HeldOutInactiveLandmark,
}

impl BundleObservationDisposition {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ActiveOptimize => "active_optimize",
            Self::OptimizeRejectedDynamicMasked => "optimize_rejected_dynamic_masked",
            Self::OptimizeRejectedUncertaintyExceeded => {
                "optimize_rejected_uncertainty_exceeded"
            }
            Self::OptimizePrunedLandmarkSupport => "optimize_pruned_landmark_support",
            Self::OptimizePrunedCameraSupport => "optimize_pruned_camera_support",
            Self::EligibleHeldOut => "eligible_held_out",
            Self::HeldOutRejectedDynamicMasked => "held_out_rejected_dynamic_masked",
            Self::HeldOutRejectedUncertaintyExceeded => {
                "held_out_rejected_uncertainty_exceeded"
            }
            Self::HeldOutInactiveLandmark => "held_out_inactive_landmark",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ActiveOptimize => 1,
            Self::OptimizeRejectedDynamicMasked => 2,
            Self::OptimizeRejectedUncertaintyExceeded => 3,
            Self::OptimizePrunedLandmarkSupport => 4,
            Self::OptimizePrunedCameraSupport => 5,
            Self::EligibleHeldOut => 6,
            Self::HeldOutRejectedDynamicMasked => 7,
            Self::HeldOutRejectedUncertaintyExceeded => 8,
            Self::HeldOutInactiveLandmark => 9,
        }
    }
}

/// Camera center selected from the exact upstream decision.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundlePoseSelection {
    /// Upstream refinement strictly dominated initialization.
    Refined,
    /// Upstream decision retained initialization.
    Initialization,
    /// Upstream conflict blocked selection; initialization is retained only as evidence.
    BlockedInitialization,
}

impl BundlePoseSelection {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Refined => "refined",
            Self::Initialization => "initialization",
            Self::BlockedInitialization => "blocked_initialization",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Refined => 1,
            Self::Initialization => 2,
            Self::BlockedInitialization => 3,
        }
    }
}

/// Compiled camera record retaining binding, pose selection, and support evidence.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CompiledBundleCamera {
    /// Exact input binding.
    pub binding: BundleCameraBinding,
    /// Canonical component root.
    pub component_root_node_id: u64,
    /// Exact arbitrary relative-scale gauge.
    pub scale_component_root_edge_id: Option<u64>,
    /// Fixed rotation from component root, row-major nano units.
    pub rotation_node_from_root_nano: [i64; 9],
    /// Selected component-relative center in nano units.
    pub selected_camera_center_from_root_nano: [i64; 3],
    /// Source of the selected center.
    pub pose_selection: BundlePoseSelection,
    /// Structural camera disposition.
    pub disposition: BundleCameraDisposition,
    /// Number of distinct active landmarks observed by this camera.
    pub active_landmark_count: u32,
}

/// Compiled landmark record retaining the proposal and structural support evidence.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CompiledBundleLandmark {
    /// Exact input seed.
    pub seed: BundleLandmarkSeed,
    /// Structural landmark disposition.
    pub disposition: BundleLandmarkDisposition,
    /// Sorted active optimize camera nodes.
    pub active_camera_node_ids: Vec<u64>,
    /// Sorted eligible held-out camera nodes.
    pub held_out_camera_node_ids: Vec<u64>,
}

/// Compiled observation retaining the exact input and stable final disposition.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CompiledBundleObservation {
    /// Exact input observation.
    pub observation: BundleObservation,
    /// Stable final structural disposition.
    pub disposition: BundleObservationDisposition,
}

/// Domain represented by one canonical bipartite graph node.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleGraphNodeKind {
    /// Camera node.
    Camera,
    /// Landmark node.
    Landmark,
}

impl BundleGraphNodeKind {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Camera => "camera",
            Self::Landmark => "landmark",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Camera => 1,
            Self::Landmark => 2,
        }
    }
}

/// Mapping from internal graph-node identity to a domain camera or landmark.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleGraphNodeMapping {
    /// Canonical internal graph node.
    pub graph_node_id: u64,
    /// Camera versus landmark namespace.
    pub kind: BundleGraphNodeKind,
    /// Camera node or landmark identity in its domain namespace.
    pub domain_id: u64,
    /// Canonical pose-component root.
    pub component_root_node_id: u64,
    /// Exact row evidence represented by this graph node.
    pub evidence_digest: EvidenceDigest,
}

/// Structural maturity of one pose component.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleProblemComponentStatus {
    /// Upstream pose refinement did not admit a camera state.
    BlockedPoseRefinement,
    /// No supported optimize core survived pruning.
    NoTrainingCore,
    /// Fixed component root lacks required direct landmark support.
    GaugeRootUnobserved,
    /// Active graph evidence is disconnected from the fixed root.
    DisconnectedFromGaugeRoot,
    /// Active graph is connected but has no redundant cycle.
    TreeLike,
    /// Cycles exist, but at least one optimize observation remains a bridge.
    Fragile,
    /// Training graph is redundant but independent held-out evidence is insufficient.
    MissingHeldOutEvidence,
    /// Root-connected, cyclic, bridge-free, and held-out-supported.
    Redundant,
}

impl BundleProblemComponentStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::BlockedPoseRefinement => "blocked_pose_refinement",
            Self::NoTrainingCore => "no_training_core",
            Self::GaugeRootUnobserved => "gauge_root_unobserved",
            Self::DisconnectedFromGaugeRoot => "disconnected_from_gauge_root",
            Self::TreeLike => "tree_like",
            Self::Fragile => "fragile",
            Self::MissingHeldOutEvidence => "missing_held_out_evidence",
            Self::Redundant => "redundant",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::BlockedPoseRefinement => 1,
            Self::NoTrainingCore => 2,
            Self::GaugeRootUnobserved => 3,
            Self::DisconnectedFromGaugeRoot => 4,
            Self::TreeLike => 5,
            Self::Fragile => 6,
            Self::MissingHeldOutEvidence => 7,
            Self::Redundant => 8,
        }
    }
}

/// Admission decision derived directly from structural status.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleProblemDecision {
    /// A structural or upstream defect blocks optimization.
    Block,
    /// Optimization may be diagnostic but cannot support promotion.
    AdmitDiagnostic,
    /// Problem is structurally admitted for bounded optimization evaluation.
    Admit,
}

impl BundleProblemDecision {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Block => "block",
            Self::AdmitDiagnostic => "admit_diagnostic",
            Self::Admit => "admit",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Block => 1,
            Self::AdmitDiagnostic => 2,
            Self::Admit => 3,
        }
    }
}

/// Minimum-cost next action compiled from structural status.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleProblemRecommendation {
    /// Resolve upstream orientation or scale conflict first.
    ResolveUpstreamPoseConflict,
    /// Add independent camera support for landmark tracks.
    AddMultiViewLandmarkSupport,
    /// Observe active landmarks directly from the fixed root neighborhood.
    ObserveGaugeRootNeighborhood,
    /// Capture evidence connecting graph islands to the root.
    CaptureBridgingView,
    /// Add independent observations that eliminate bridge dependence.
    ReinforceBridgeObservations,
    /// Reserve independent observations outside the optimize graph.
    ReserveIndependentHeldOutViews,
    /// Proceed only to a bounded bundle-optimization evaluation.
    ProceedToBoundedBundleOptimization,
}

impl BundleProblemRecommendation {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ResolveUpstreamPoseConflict => "resolve_upstream_pose_conflict",
            Self::AddMultiViewLandmarkSupport => "add_multi_view_landmark_support",
            Self::ObserveGaugeRootNeighborhood => "observe_gauge_root_neighborhood",
            Self::CaptureBridgingView => "capture_bridging_view",
            Self::ReinforceBridgeObservations => "reinforce_bridge_observations",
            Self::ReserveIndependentHeldOutViews => "reserve_independent_held_out_views",
            Self::ProceedToBoundedBundleOptimization => {
                "proceed_to_bounded_bundle_optimization"
            }
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ResolveUpstreamPoseConflict => 1,
            Self::AddMultiViewLandmarkSupport => 2,
            Self::ObserveGaugeRootNeighborhood => 3,
            Self::CaptureBridgingView => 4,
            Self::ReinforceBridgeObservations => 5,
            Self::ReserveIndependentHeldOutViews => 6,
            Self::ProceedToBoundedBundleOptimization => 7,
        }
    }
}

/// One component-level topology certificate and typed decision card.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleProblemComponent {
    /// Canonical pose-component root and fixed zero-origin gauge.
    pub component_root_node_id: u64,
    /// Exact arbitrary relative-scale gauge.
    pub scale_component_root_edge_id: Option<u64>,
    /// Structural maturity.
    pub status: BundleProblemComponentStatus,
    /// Structural admission decision.
    pub decision: BundleProblemDecision,
    /// Minimum-cost next action.
    pub recommendation: BundleProblemRecommendation,
    /// Sorted active camera nodes, including the fixed root.
    pub active_camera_node_ids: Vec<u64>,
    /// Sorted pruned non-root camera nodes.
    pub pruned_camera_node_ids: Vec<u64>,
    /// Sorted active landmark identities.
    pub active_landmark_ids: Vec<u64>,
    /// Sorted pruned landmark identities.
    pub pruned_landmark_ids: Vec<u64>,
    /// Sorted active optimize observation identities.
    pub active_optimize_observation_ids: Vec<u64>,
    /// Sorted eligible held-out observation identities.
    pub eligible_held_out_observation_ids: Vec<u64>,
    /// Sorted eligible held-out camera nodes.
    pub eligible_held_out_camera_node_ids: Vec<u64>,
    /// Sorted optimize observations that are graph bridges.
    pub bridge_observation_ids: Vec<u64>,
    /// Sorted maximum-priority forest observation identities.
    pub forest_observation_ids: Vec<u64>,
    /// Sorted non-forest observation identities.
    pub non_forest_observation_ids: Vec<u64>,
    /// Sorted bipartite graph component identities represented inside this pose component.
    pub graph_component_ids: Vec<u64>,
    /// Whether every active graph node is connected to the fixed root.
    pub root_connected: bool,
    /// Number of distinct active landmarks directly observed by the root.
    pub root_active_landmark_count: u32,
    /// Local bipartite cycle rank.
    pub cycle_rank: u64,
    /// Planning-only scalar equation count (`2 × active observations`).
    pub nominal_equation_count: u64,
    /// Planning-only unknown count with fixed rotations, calibration, and root translation.
    pub nominal_unknown_count: u64,
    /// Signed planning-only equation surplus; not a numerical-rank certificate.
    pub nominal_equation_surplus: i64,
}

/// Immutable deterministic structural bundle-problem generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleProblemGeneration {
    /// Exact immutable basis.
    pub basis: BundleProblemBasis,
    /// Exact instantiated policy.
    pub policy: BundleProblemPolicy,
    /// Exact pose-refinement input retained for replay.
    pub pose_refinement: PoseRefinementGeneration,
    /// Canonical compiled cameras.
    pub cameras: Vec<CompiledBundleCamera>,
    /// Canonical compiled landmarks.
    pub landmarks: Vec<CompiledBundleLandmark>,
    /// Canonical compiled observations.
    pub observations: Vec<CompiledBundleObservation>,
    /// Canonical internal graph-node mapping.
    pub graph_nodes: Vec<BundleGraphNodeMapping>,
    /// Exact deterministic bipartite graph analysis.
    pub graph: GraphAnalysis,
    /// Component decision cards in root order.
    pub components: Vec<BundleProblemComponent>,
    /// Fixed-point pruning passes including the terminal no-change pass; operation evidence only.
    pub pruning_rounds: u32,
    /// Exact deterministic compiler work charged; operation evidence only.
    pub operation_count: u64,
}

/// Stable structural bundle-problem failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum BundleProblemError {
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
    /// A required semantic minimum was not met.
    BelowMinimum {
        /// Stable field name.
        field: &'static str,
        /// Observed value.
        actual: u128,
        /// Required minimum.
        minimum: u128,
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
    /// One supplied basis identity disagreed with independently recomputed evidence.
    BasisMismatch {
        /// Stable basis field.
        field: &'static str,
        /// Required identity.
        expected: EvidenceDigest,
        /// Independently observed identity.
        observed: EvidenceDigest,
    },
    /// No camera pose was supplied by the exact upstream generation.
    EmptyPoseRefinement,
    /// A camera binding was duplicated.
    DuplicateCameraBinding {
        /// Camera node.
        camera_node_id: u64,
    },
    /// A camera in the exact pose generation lacked a binding.
    MissingCameraBinding {
        /// Camera node.
        camera_node_id: u64,
    },
    /// A binding named a camera absent from the exact pose generation.
    ExtraCameraBinding {
        /// Camera node.
        camera_node_id: u64,
    },
    /// A binding sample did not equal the retained pose sample.
    CameraSampleMismatch {
        /// Camera node.
        camera_node_id: u64,
        /// Required sample.
        expected: u64,
        /// Supplied sample.
        observed: u64,
    },
    /// A landmark identity was duplicated.
    DuplicateLandmark {
        /// Duplicate landmark.
        landmark_id: u64,
    },
    /// One source track was used to seed multiple landmarks.
    DuplicateSourceTrack {
        /// Reused source track.
        source_track_id: u64,
    },
    /// A landmark named a pose component that does not exist.
    UnknownComponentRoot {
        /// Landmark.
        landmark_id: u64,
        /// Missing component root.
        component_root_node_id: u64,
    },
    /// A landmark named a scale gauge different from its pose component.
    LandmarkGaugeMismatch {
        /// Landmark.
        landmark_id: u64,
        /// Required scale root.
        expected: Option<u64>,
        /// Supplied scale root.
        observed: Option<u64>,
    },
    /// An observation identity was duplicated.
    DuplicateObservation {
        /// Duplicate observation.
        observation_id: u64,
    },
    /// An observation named a camera absent from the exact pose generation.
    UnknownCamera {
        /// Observation.
        observation_id: u64,
        /// Missing camera.
        camera_node_id: u64,
    },
    /// An observation named an unknown landmark.
    UnknownLandmark {
        /// Observation.
        observation_id: u64,
        /// Missing landmark.
        landmark_id: u64,
    },
    /// More than one observation named the same camera/landmark pair.
    DuplicateCameraLandmarkPair {
        /// Camera node.
        camera_node_id: u64,
        /// Landmark.
        landmark_id: u64,
    },
    /// One camera feature was reused to support more than one bundle observation.
    FeatureObservationReused {
        /// Camera node.
        camera_node_id: u64,
        /// Reused source feature observation.
        source_feature_observation_id: u64,
    },
    /// Observation sample disagreed with the exact camera binding.
    ObservationSampleMismatch {
        /// Observation.
        observation_id: u64,
        /// Required sample.
        expected: u64,
        /// Supplied sample.
        observed: u64,
    },
    /// Observation frame disagreed with the exact camera binding.
    ObservationFrameMismatch {
        /// Observation.
        observation_id: u64,
        /// Required frame.
        expected: EvidenceDigest,
        /// Supplied frame.
        observed: EvidenceDigest,
    },
    /// Observation camera and landmark belong to different pose components.
    ObservationComponentMismatch {
        /// Observation.
        observation_id: u64,
        /// Camera component root.
        camera_component_root_node_id: u64,
        /// Landmark component root.
        landmark_component_root_node_id: u64,
    },
    /// Observation camera and landmark use different arbitrary scale gauges.
    ObservationGaugeMismatch {
        /// Observation.
        observation_id: u64,
        /// Camera scale root.
        camera_scale_component_root_edge_id: Option<u64>,
        /// Landmark scale root.
        landmark_scale_component_root_edge_id: Option<u64>,
    },
    /// Deterministic work exceeded its explicit ceiling.
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
    /// An expected camera, component, graph node, or observation was absent internally.
    InternalLookup {
        /// Stable lookup name.
        field: &'static str,
    },
    /// Deterministic replay disagreed with retained output.
    DerivedMismatch {
        /// Stable derived field name.
        field: &'static str,
    },
    /// Upstream pose-refinement validation failed.
    PoseRefinement(PoseRefinementError),
    /// Bipartite graph analysis failed.
    Graph(GraphError),
    /// Canonical codec failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for BundleProblemError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => write!(
                formatter,
                "bundle-problem identity {field} must not be all zero"
            ),
            Self::ZeroValue { field } => {
                write!(formatter, "bundle-problem field {field} must be nonzero")
            }
            Self::BelowMinimum {
                field,
                actual,
                minimum,
            } => write!(
                formatter,
                "bundle-problem field {field} is {actual}; minimum is {minimum}"
            ),
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "bundle-problem field {field} is {actual}; maximum is {maximum}"
            ),
            Self::BasisMismatch {
                field,
                expected,
                observed,
            } => write!(
                formatter,
                "bundle-problem basis {field} expected {expected}, observed {observed}"
            ),
            Self::EmptyPoseRefinement => {
                formatter.write_str("bundle-problem compilation requires at least one camera pose")
            }
            Self::DuplicateCameraBinding { camera_node_id } => {
                write!(formatter, "bundle camera binding {camera_node_id} is duplicated")
            }
            Self::MissingCameraBinding { camera_node_id } => {
                write!(formatter, "bundle camera {camera_node_id} lacks an exact binding")
            }
            Self::ExtraCameraBinding { camera_node_id } => write!(
                formatter,
                "bundle camera binding {camera_node_id} is absent from the exact pose generation"
            ),
            Self::CameraSampleMismatch {
                camera_node_id,
                expected,
                observed,
            } => write!(
                formatter,
                "bundle camera {camera_node_id} requires sample {expected}; observed {observed}"
            ),
            Self::DuplicateLandmark { landmark_id } => {
                write!(formatter, "bundle landmark {landmark_id} is duplicated")
            }
            Self::DuplicateSourceTrack { source_track_id } => write!(
                formatter,
                "bundle source track {source_track_id} seeds more than one landmark"
            ),
            Self::UnknownComponentRoot {
                landmark_id,
                component_root_node_id,
            } => write!(
                formatter,
                "bundle landmark {landmark_id} names unknown component root {component_root_node_id}"
            ),
            Self::LandmarkGaugeMismatch {
                landmark_id,
                expected,
                observed,
            } => write!(
                formatter,
                "bundle landmark {landmark_id} requires scale gauge {expected:?}; observed {observed:?}"
            ),
            Self::DuplicateObservation { observation_id } => {
                write!(formatter, "bundle observation {observation_id} is duplicated")
            }
            Self::UnknownCamera {
                observation_id,
                camera_node_id,
            } => write!(
                formatter,
                "bundle observation {observation_id} names unknown camera {camera_node_id}"
            ),
            Self::UnknownLandmark {
                observation_id,
                landmark_id,
            } => write!(
                formatter,
                "bundle observation {observation_id} names unknown landmark {landmark_id}"
            ),
            Self::DuplicateCameraLandmarkPair {
                camera_node_id,
                landmark_id,
            } => write!(
                formatter,
                "bundle camera/landmark pair ({camera_node_id}, {landmark_id}) is duplicated"
            ),
            Self::FeatureObservationReused {
                camera_node_id,
                source_feature_observation_id,
            } => write!(
                formatter,
                "bundle camera feature ({camera_node_id}, {source_feature_observation_id}) is reused"
            ),
            Self::ObservationSampleMismatch {
                observation_id,
                expected,
                observed,
            } => write!(
                formatter,
                "bundle observation {observation_id} requires sample {expected}; observed {observed}"
            ),
            Self::ObservationFrameMismatch {
                observation_id,
                expected,
                observed,
            } => write!(
                formatter,
                "bundle observation {observation_id} requires frame {expected}; observed {observed}"
            ),
            Self::ObservationComponentMismatch {
                observation_id,
                camera_component_root_node_id,
                landmark_component_root_node_id,
            } => write!(
                formatter,
                "bundle observation {observation_id} joins pose components {camera_component_root_node_id} and {landmark_component_root_node_id}"
            ),
            Self::ObservationGaugeMismatch {
                observation_id,
                camera_scale_component_root_edge_id,
                landmark_scale_component_root_edge_id,
            } => write!(
                formatter,
                "bundle observation {observation_id} joins scale gauges {camera_scale_component_root_edge_id:?} and {landmark_scale_component_root_edge_id:?}"
            ),
            Self::OperationBudgetExceeded { attempted, maximum } => write!(
                formatter,
                "bundle-problem compilation attempted operation {attempted}; maximum is {maximum}"
            ),
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "bundle-problem arithmetic overflowed while computing {field}"
            ),
            Self::InternalLookup { field } => write!(
                formatter,
                "bundle-problem internal lookup failed for {field}"
            ),
            Self::DerivedMismatch { field } => write!(
                formatter,
                "bundle-problem derived field {field} disagrees with deterministic replay"
            ),
            Self::PoseRefinement(error) => {
                write!(formatter, "bundle-problem pose-refinement error: {error}")
            }
            Self::Graph(error) => write!(formatter, "bundle-problem graph error: {error}"),
            Self::Codec(error) => write!(formatter, "bundle-problem codec error: {error}"),
            Self::Domain(error) => write!(
                formatter,
                "bundle-problem identity-domain error: {error}"
            ),
            Self::JsonRendering(error) => {
                write!(formatter, "bundle-problem JSON rendering failed: {error}")
            }
        }
    }
}

impl Error for BundleProblemError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::PoseRefinement(error) => Some(error),
            Self::Graph(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<PoseRefinementError> for BundleProblemError {
    fn from(error: PoseRefinementError) -> Self {
        Self::PoseRefinement(error)
    }
}

impl From<GraphError> for BundleProblemError {
    fn from(error: GraphError) -> Self {
        Self::Graph(error)
    }
}

impl From<CodecError> for BundleProblemError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for BundleProblemError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
