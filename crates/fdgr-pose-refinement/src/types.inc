/// Public schema identity for one deterministic pose-refinement generation.
pub const POSE_REFINEMENT_SCHEMA: &str = "fdgr.pose_refinement/1";
/// Canonical authority level of every emitted camera center.
pub const POSE_REFINEMENT_AUTHORITY: &str = "relative_component_gauge";
/// Fixed-point scale shared by rotations, directions, relative scales, and camera centers.
pub const UNIT_NANO: i64 = 1_000_000_000;
/// Maximum semantic iteration count for the scalar reference solver.
pub const MAX_REFINEMENT_ITERATIONS: u32 = 10_000;
/// Maximum absolute camera-center coordinate admitted by this reference solver.
pub const MAX_REFINEMENT_POSITION_ABS_NANO: u64 = 1_000_000_000_000_000;
/// Maximum robust residual transition admitted by policy.
pub const MAX_REFINEMENT_HUBER_DELTA_NANO: u64 = 1_000_000_000_000_000;
/// Maximum damping pseudo-weight.
pub const MAX_REFINEMENT_DAMPING_WEIGHT: u32 = 100;
/// Maximum admitted factor weight.
pub const MAX_REFINEMENT_FACTOR_WEIGHT: u32 = 100;
/// Maximum deterministic operation count.
pub const MAX_REFINEMENT_OPERATIONS: u64 = 100_000_000;
/// Largest public integer emitted as a JSON number.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one pose-refinement generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseRefinementBasis {
    /// Exact global-pose initialization being refined.
    pub initialization_digest: EvidenceDigest,
    /// Exact semantic refinement-policy identity.
    pub policy_digest: EvidenceDigest,
    /// Immutable generation.
    pub generation: u64,
}

/// Semantic translation-relaxation policy plus one nonsemantic operation ceiling.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PoseRefinementPolicy {
    /// Maximum deterministic Gauss-Seidel sweeps.
    pub max_iterations: u32,
    /// Maximum coordinate change considered converged.
    pub convergence_delta_nano: u64,
    /// Initial residual at which factor weights begin to be downweighted.
    pub huber_delta_nano: u64,
    /// Positive pseudo-weight retaining part of the current iterate at every update.
    pub damping_weight: u32,
    /// Maximum support-derived weight assigned to one pose factor.
    pub max_factor_weight: u32,
    /// Maximum absolute component-relative camera-center coordinate.
    pub max_camera_center_abs_nano: u64,
    /// Hard deterministic operation ceiling, excluded from successful semantic identity.
    pub max_operations: u64,
}

impl Default for PoseRefinementPolicy {
    fn default() -> Self {
        Self {
            max_iterations: 100,
            convergence_delta_nano: 100,
            huber_delta_nano: 250_000_000,
            damping_weight: 1,
            max_factor_weight: 100,
            max_camera_center_abs_nano: MAX_REFINEMENT_POSITION_ABS_NANO,
            max_operations: 10_000_000,
        }
    }
}

/// Deterministic disposition of one pose factor.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PoseFactorDisposition {
    /// The factor retained its full support-derived weight.
    ActiveFullWeight,
    /// The factor remained active with a reduced fixed robust weight.
    ActiveDownweighted,
    /// The edge scale belongs to another arbitrary gauge.
    IncomparableScaleGauge,
    /// The containing component has an upstream orientation or scale conflict.
    BlockedUpstreamConflict,
}

impl PoseFactorDisposition {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ActiveFullWeight => "active_full_weight",
            Self::ActiveDownweighted => "active_downweighted",
            Self::IncomparableScaleGauge => "incomparable_scale_gauge",
            Self::BlockedUpstreamConflict => "blocked_upstream_conflict",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ActiveFullWeight => 1,
            Self::ActiveDownweighted => 2,
            Self::IncomparableScaleGauge => 3,
            Self::BlockedUpstreamConflict => 4,
        }
    }
}

/// Deterministic terminal state of one component refinement.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PoseRefinementComponentStatus {
    /// A one-camera component has no translation degrees of freedom.
    Singleton,
    /// An upstream orientation or scale conflict prevented refinement.
    BlockedUpstreamConflict,
    /// No edge displacement was comparable to the component gauge.
    NoComparableFactors,
    /// Every active factor was already exactly satisfied.
    AlreadySatisfied,
    /// The coordinate-change convergence threshold was reached.
    Converged,
    /// The semantic iteration ceiling was reached after objective improvement.
    IterationLimit,
    /// A sweep failed to reduce the fixed robust weighted objective and was reverted.
    NoImprovement,
}

impl PoseRefinementComponentStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Singleton => "singleton",
            Self::BlockedUpstreamConflict => "blocked_upstream_conflict",
            Self::NoComparableFactors => "no_comparable_factors",
            Self::AlreadySatisfied => "already_satisfied",
            Self::Converged => "converged",
            Self::IterationLimit => "iteration_limit",
            Self::NoImprovement => "no_improvement",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Singleton => 1,
            Self::BlockedUpstreamConflict => 2,
            Self::NoComparableFactors => 3,
            Self::AlreadySatisfied => 4,
            Self::Converged => 5,
            Self::IterationLimit => 6,
            Self::NoImprovement => 7,
        }
    }
}

/// Agent-readable decision about whether refined centers dominate the initialization.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PoseRefinementDecision {
    /// The refined centers have a strictly lower fixed robust weighted objective.
    AcceptRefined,
    /// The initialization remains the safer or equally good state.
    RetainInitialization,
    /// Upstream conflict blocks any refinement decision.
    Blocked,
}

impl PoseRefinementDecision {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::AcceptRefined => "accept_refined",
            Self::RetainInitialization => "retain_initialization",
            Self::Blocked => "blocked",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::AcceptRefined => 1,
            Self::RetainInitialization => 2,
            Self::Blocked => 3,
        }
    }
}

/// Stable reason supporting a component decision.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PoseRefinementReason {
    /// The component has no adjustable translation.
    Singleton,
    /// The initialization already satisfies every active factor.
    AlreadySatisfied,
    /// The fixed robust weighted objective strictly improved.
    ObjectiveImproved,
    /// A candidate sweep did not improve the objective.
    NoImprovement,
    /// No factor shared the component's arbitrary scale gauge.
    NoComparableFactors,
    /// Upstream orientation or scale evidence conflicts.
    UpstreamConflict,
}

impl PoseRefinementReason {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Singleton => "singleton",
            Self::AlreadySatisfied => "already_satisfied",
            Self::ObjectiveImproved => "objective_improved",
            Self::NoImprovement => "no_improvement",
            Self::NoComparableFactors => "no_comparable_factors",
            Self::UpstreamConflict => "upstream_conflict",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Singleton => 1,
            Self::AlreadySatisfied => 2,
            Self::ObjectiveImproved => 3,
            Self::NoImprovement => 4,
            Self::NoComparableFactors => 5,
            Self::UpstreamConflict => 6,
        }
    }
}

/// One camera pose before and after translation-only relaxation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RefinedCameraPose {
    /// Stable pose-graph node identity.
    pub node_id: u64,
    /// Source sample index.
    pub sample_index: u64,
    /// Canonical component root and zero-origin gauge.
    pub component_root_node_id: u64,
    /// Relative scale component used by this pose component.
    pub scale_component_root_edge_id: Option<u64>,
    /// Unchanged `R_node_from_component_root`, row-major and multiplied by [`UNIT_NANO`].
    pub rotation_node_from_root_nano: [i64; 9],
    /// Camera center supplied by the exact initialization.
    pub initial_camera_center_from_root_nano: [i64; 3],
    /// Camera center after deterministic translation relaxation.
    pub refined_camera_center_from_root_nano: [i64; 3],
    /// Refined minus initial center.
    pub adjustment_nano: [i64; 3],
    /// Whether this camera is the immutable zero-origin component root.
    pub root_pinned: bool,
}

/// One pose-edge factor assessment.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseFactorAssessment {
    /// Stable pose-edge identity.
    pub edge_id: u64,
    /// Canonical component root.
    pub component_root_node_id: u64,
    /// Left camera node.
    pub left_node_id: u64,
    /// Right camera node.
    pub right_node_id: u64,
    /// Support-derived weight before robust downweighting.
    pub base_weight: u32,
    /// Fixed robust weight used by the solver; zero when the factor was not active.
    pub robust_weight: u32,
    /// Comparable measured displacement, absent for another arbitrary scale gauge.
    pub measured_displacement_nano: Option<[i64; 3]>,
    /// Displacement implied by initialization centers.
    pub initial_implied_displacement_nano: [i64; 3],
    /// Displacement implied by refined centers.
    pub refined_implied_displacement_nano: [i64; 3],
    /// Initial maximum-coordinate residual, absent when gauges are incomparable.
    pub initial_residual_nano: Option<u64>,
    /// Final maximum-coordinate residual, absent when gauges are incomparable.
    pub final_residual_nano: Option<u64>,
    /// Conservative factor disposition.
    pub disposition: PoseFactorDisposition,
}

/// One component-level refinement result and decision card.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseRefinementComponent {
    /// Canonical component root and pinned zero origin.
    pub component_root_node_id: u64,
    /// Relative scale-component root, absent for a singleton.
    pub scale_component_root_edge_id: Option<u64>,
    /// Terminal solver state.
    pub status: PoseRefinementComponentStatus,
    /// Agent-readable accept/retain/block decision.
    pub decision: PoseRefinementDecision,
    /// Stable reason for the decision.
    pub reason: PoseRefinementReason,
    /// Completed deterministic sweeps.
    pub iterations: u32,
    /// Initial robust weighted RMS residual, absent when no factor was eligible.
    pub initial_rms_residual_nano: Option<u64>,
    /// Final robust weighted RMS residual, absent when no factor was eligible.
    pub final_rms_residual_nano: Option<u64>,
    /// Initial minus final RMS residual.
    pub rms_improvement_nano: Option<u64>,
    /// Largest maximum-coordinate camera-center adjustment.
    pub max_adjustment_nano: u64,
    /// Whether retaining the initialization weakly dominates accepting a refinement.
    pub do_nothing_dominates: bool,
    /// Sorted member camera nodes.
    pub node_ids: Vec<u64>,
    /// Sorted pose-edge factors in the component.
    pub factor_edge_ids: Vec<u64>,
    /// Sorted comparable factors used by the solver.
    pub active_factor_edge_ids: Vec<u64>,
    /// Sorted active factors remaining beyond the robust transition.
    pub downweighted_edge_ids: Vec<u64>,
}

/// Immutable deterministic translation-only pose refinement.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseRefinementGeneration {
    /// Exact immutable basis.
    pub basis: PoseRefinementBasis,
    /// Exact instantiated policy.
    pub policy: PoseRefinementPolicy,
    /// Exact initialization retained for deterministic replay.
    pub initialization: GlobalPoseInitialization,
    /// Refined poses in canonical node order.
    pub poses: Vec<RefinedCameraPose>,
    /// Factor assessments in canonical edge order.
    pub factors: Vec<PoseFactorAssessment>,
    /// Component decision cards in canonical root order.
    pub components: Vec<PoseRefinementComponent>,
    /// Total completed component sweeps.
    pub iteration_count: u64,
    /// Exact deterministic work charged; operation evidence only.
    pub operation_count: u64,
}

/// Stable pose-refinement failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum PoseRefinementError {
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
    /// The supplied initialization identity disagreed with exact replay.
    BasisMismatch {
        /// Required identity.
        expected: EvidenceDigest,
        /// Independently observed identity.
        observed: EvidenceDigest,
    },
    /// A validated edge joined different pose components.
    CrossComponentEdge {
        /// Pose edge.
        edge_id: u64,
        /// Left component root.
        left_component_root_node_id: u64,
        /// Right component root.
        right_component_root_node_id: u64,
    },
    /// An expected pose, edge scale, component, factor, or adjacency record was absent.
    InternalLookup {
        /// Stable lookup name.
        field: &'static str,
    },
    /// A candidate camera center exceeded policy.
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
    /// Global-pose initialization validation failed.
    Initialization(GlobalPoseError),
    /// Canonical codec failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for PoseRefinementError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => {
                write!(formatter, "pose-refinement identity {field} must not be all zero")
            }
            Self::ZeroValue { field } => {
                write!(formatter, "pose-refinement field {field} must be nonzero")
            }
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "pose-refinement field {field} is {actual}; maximum is {maximum}"
            ),
            Self::BasisMismatch { expected, observed } => write!(
                formatter,
                "pose-refinement basis initialization_digest expected {expected}, observed {observed}"
            ),
            Self::CrossComponentEdge {
                edge_id,
                left_component_root_node_id,
                right_component_root_node_id,
            } => write!(
                formatter,
                "pose-refinement edge {edge_id} joins components {left_component_root_node_id} and {right_component_root_node_id}"
            ),
            Self::InternalLookup { field } => {
                write!(formatter, "pose-refinement internal lookup failed for {field}")
            }
            Self::PositionBoundExceeded {
                node_id,
                actual,
                maximum,
            } => write!(
                formatter,
                "pose-refinement node {node_id} camera-center magnitude {actual} exceeds {maximum}"
            ),
            Self::OperationBudgetExceeded { attempted, maximum } => write!(
                formatter,
                "pose refinement attempted operation {attempted}; maximum is {maximum}"
            ),
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "pose-refinement arithmetic overflowed while computing {field}"
            ),
            Self::DerivedMismatch { field } => write!(
                formatter,
                "pose-refinement derived field {field} disagrees with deterministic replay"
            ),
            Self::Initialization(error) => {
                write!(formatter, "pose-refinement initialization error: {error}")
            }
            Self::Codec(error) => write!(formatter, "pose-refinement codec error: {error}"),
            Self::Domain(error) => {
                write!(formatter, "pose-refinement identity-domain error: {error}")
            }
            Self::JsonRendering(error) => {
                write!(formatter, "pose-refinement JSON rendering failed: {error}")
            }
        }
    }
}

impl Error for PoseRefinementError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Initialization(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<GlobalPoseError> for PoseRefinementError {
    fn from(error: GlobalPoseError) -> Self {
        Self::Initialization(error)
    }
}

impl From<CodecError> for PoseRefinementError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for PoseRefinementError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
