/// Public schema identity for one deterministic bundle-admission audit.
pub const BUNDLE_ADMISSION_SCHEMA: &str = "fdgr.bundle_admission/1";
/// Canonical authority emitted by a fully admitted audit.
pub const BUNDLE_ADMISSION_AUTHORITY: &str = "audited_relative_bundle_problem";
/// Semantic policy identity domain.
pub const BUNDLE_ADMISSION_POLICY_SCHEMA: &str = "fdgr.bundle_admission_policy/1";
/// Camera-domain table identity domain.
pub const CAMERA_DOMAIN_TABLE_SCHEMA: &str = "fdgr.bundle_camera_domain_table/1";
/// Landmark seed-provenance table identity domain.
pub const SEED_PROVENANCE_TABLE_SCHEMA: &str = "fdgr.bundle_seed_provenance_table/1";
/// Fixed nano-pixel denominator.
pub const NANO_PIXEL_SCALE: i64 = 1_000_000_000;
/// Maximum admitted image width or height.
pub const MAX_IMAGE_DIMENSION: u32 = 65_535;
/// Maximum camera-domain rows.
pub const MAX_CAMERA_DOMAINS: usize = 100_000;
/// Maximum landmark seed-provenance rows.
pub const MAX_SEED_PROVENANCES: usize = 100_000;
/// Maximum support observations named by one landmark seed.
pub const MAX_SEED_SUPPORT_OBSERVATIONS: usize = 100_000;
/// Maximum semantic seed uncertainty.
pub const MAX_SEED_UNCERTAINTY_NANO: u64 = 9_000_000_000_000_000;
/// Maximum deterministic audit operations.
pub const MAX_ADMISSION_OPERATIONS: u64 = 1_000_000_000;
/// Largest public identity emitted as an unquoted JSON integer.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one audit generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleAdmissionBasis {
    /// Exact structural bundle-problem generation.
    pub bundle_problem_digest: EvidenceDigest,
    /// Exact canonical camera-domain table.
    pub camera_domain_basis_digest: EvidenceDigest,
    /// Exact canonical landmark seed-provenance table.
    pub seed_provenance_basis_digest: EvidenceDigest,
    /// Exact semantic audit policy.
    pub policy_digest: EvidenceDigest,
    /// Immutable generation.
    pub generation: u64,
}

/// Semantic audit gates plus one nonsemantic execution ceiling.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct BundleAdmissionPolicy {
    /// Maximum seed uncertainty compatible with optimize-only provenance admission.
    pub max_seed_uncertainty_nano: u64,
    /// Minimum surviving optimize observations that must have initialized an active seed.
    pub min_seed_support_observations: u32,
    /// Minimum distinct surviving optimize cameras that must have initialized an active seed.
    pub min_seed_support_cameras: u32,
    /// Whether a held-out observation's camera must remain in the final optimize core.
    pub require_active_held_out_camera: bool,
    /// Hard deterministic operation ceiling, excluded from successful semantic identity.
    pub max_operations: u64,
}

impl Default for BundleAdmissionPolicy {
    fn default() -> Self {
        Self {
            max_seed_uncertainty_nano: 2_000_000_000,
            min_seed_support_observations: 2,
            min_seed_support_cameras: 2,
            require_active_held_out_camera: true,
            max_operations: 100_000_000,
        }
    }
}

/// Exact calibrated image domain associated with one structural camera binding.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleCameraDomain {
    /// Stable camera node identity.
    pub camera_node_id: u64,
    /// Exact decoded-frame object identity.
    pub frame_digest: EvidenceDigest,
    /// Exact effective-calibration identity for this image domain.
    pub effective_calibration_digest: EvidenceDigest,
    /// Calibrated image width in pixels.
    pub image_width: u32,
    /// Calibrated image height in pixels.
    pub image_height: u32,
}

/// Exact optimize-only observation provenance for one landmark seed.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LandmarkSeedProvenance {
    /// Stable landmark identity.
    pub landmark_id: u64,
    /// Canonically sorted observation identities used to initialize the seed.
    pub support_observation_ids: Vec<u64>,
}

/// Stable final audit disposition of one supplied bundle observation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ObservationAuditDisposition {
    /// Active optimize observation lies inside the exact image domain.
    ActiveOptimizeInDomain,
    /// Eligible held-out observation remains independent and its camera is active.
    EligibleHeldOutIndependent,
    /// Upstream eligible held-out observation names a camera absent from the optimize core.
    HeldOutInactiveCamera,
    /// Observation lies outside its exact calibrated image domain.
    OutsideImageDomain,
    /// Observation was already excluded by structural compilation.
    UpstreamExcluded,
}

impl ObservationAuditDisposition {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ActiveOptimizeInDomain => "active_optimize_in_domain",
            Self::EligibleHeldOutIndependent => "eligible_held_out_independent",
            Self::HeldOutInactiveCamera => "held_out_inactive_camera",
            Self::OutsideImageDomain => "outside_image_domain",
            Self::UpstreamExcluded => "upstream_excluded",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ActiveOptimizeInDomain => 1,
            Self::EligibleHeldOutIndependent => 2,
            Self::HeldOutInactiveCamera => 3,
            Self::OutsideImageDomain => 4,
            Self::UpstreamExcluded => 5,
        }
    }
}

/// Audit evidence for one exact bundle observation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleObservationAudit {
    /// Stable observation identity.
    pub observation_id: u64,
    /// Stable landmark identity.
    pub landmark_id: u64,
    /// Stable camera identity.
    pub camera_node_id: u64,
    /// Whether the coordinate lies inside its exact image domain.
    pub within_image_domain: bool,
    /// Whether the camera remains in the final optimize core.
    pub camera_active: bool,
    /// Whether the landmark remains in the final optimize core.
    pub landmark_active: bool,
    /// Whether this optimize observation contributed to seed initialization.
    pub seed_support_used: bool,
    /// Stable final audit disposition.
    pub disposition: ObservationAuditDisposition,
}

/// Optimize-only seed provenance evidence for one landmark.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LandmarkSeedAudit {
    /// Stable landmark identity.
    pub landmark_id: u64,
    /// Canonical declared seed-support observations.
    pub declared_support_observation_ids: Vec<u64>,
    /// Declared support observations surviving as in-domain active optimize evidence.
    pub surviving_support_observation_ids: Vec<u64>,
    /// Distinct cameras represented by surviving support.
    pub surviving_support_camera_node_ids: Vec<u64>,
    /// Whether the seed uncertainty satisfies audit policy.
    pub seed_uncertainty_within_policy: bool,
    /// Whether all support-count, camera-count, and uncertainty gates passed.
    pub provenance_proven: bool,
}

/// Structural maturity after image-domain, seed, and held-out auditing.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleAdmissionComponentStatus {
    /// Upstream structural compilation blocked the component.
    BlockedUpstream,
    /// An active or eligible observation lies outside its bound image domain.
    InvalidImageDomain,
    /// One or more active landmark seeds lack sufficient optimize-only provenance.
    UnprovenSeedPartition,
    /// Stronger held-out independence checks no longer satisfy structural policy.
    InsufficientIndependentHeldOut,
    /// Upstream structural compilation intentionally admitted diagnostic use only.
    UpstreamDiagnostic,
    /// Structural and audit gates all pass.
    Admitted,
}

impl BundleAdmissionComponentStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::BlockedUpstream => "blocked_upstream",
            Self::InvalidImageDomain => "invalid_image_domain",
            Self::UnprovenSeedPartition => "unproven_seed_partition",
            Self::InsufficientIndependentHeldOut => "insufficient_independent_held_out",
            Self::UpstreamDiagnostic => "upstream_diagnostic",
            Self::Admitted => "admitted",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::BlockedUpstream => 1,
            Self::InvalidImageDomain => 2,
            Self::UnprovenSeedPartition => 3,
            Self::InsufficientIndependentHeldOut => 4,
            Self::UpstreamDiagnostic => 5,
            Self::Admitted => 6,
        }
    }
}

/// Audit decision for one pose component.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleAdmissionDecision {
    /// Numerical optimization is blocked.
    Block,
    /// Diagnostic optimization may run but cannot support promotion.
    AdmitDiagnostic,
    /// The component may enter bounded optimization evaluation.
    Admit,
}

impl BundleAdmissionDecision {
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

/// Minimum-cost next action compiled from audit status.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BundleAdmissionRecommendation {
    /// Resolve the structural bundle-problem defect first.
    ResolveUpstreamStructuralProblem,
    /// Correct frame, calibration, crop, resize, or image-coordinate binding.
    BindCorrectImageDomain,
    /// Rebuild landmark seeds from the surviving optimize partition.
    RebuildSeedsFromOptimizePartition,
    /// Retain held-out evidence only from active independent cameras.
    ReserveIndependentHeldOutViews,
    /// Follow the upstream diagnostic recommendation.
    FollowUpstreamDiagnostic,
    /// Proceed only to bounded bundle-optimization evaluation.
    ProceedToBoundedBundleOptimization,
}

impl BundleAdmissionRecommendation {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ResolveUpstreamStructuralProblem => "resolve_upstream_structural_problem",
            Self::BindCorrectImageDomain => "bind_correct_image_domain",
            Self::RebuildSeedsFromOptimizePartition => {
                "rebuild_seeds_from_optimize_partition"
            }
            Self::ReserveIndependentHeldOutViews => "reserve_independent_held_out_views",
            Self::FollowUpstreamDiagnostic => "follow_upstream_diagnostic",
            Self::ProceedToBoundedBundleOptimization => {
                "proceed_to_bounded_bundle_optimization"
            }
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ResolveUpstreamStructuralProblem => 1,
            Self::BindCorrectImageDomain => 2,
            Self::RebuildSeedsFromOptimizePartition => 3,
            Self::ReserveIndependentHeldOutViews => 4,
            Self::FollowUpstreamDiagnostic => 5,
            Self::ProceedToBoundedBundleOptimization => 6,
        }
    }
}

/// One component-level audit certificate and typed next action.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleAdmissionComponent {
    /// Canonical pose-component root.
    pub component_root_node_id: u64,
    /// Exact arbitrary relative-scale gauge.
    pub scale_component_root_edge_id: Option<u64>,
    /// Final audit status.
    pub status: BundleAdmissionComponentStatus,
    /// Final audit decision.
    pub decision: BundleAdmissionDecision,
    /// Minimum-cost next action.
    pub recommendation: BundleAdmissionRecommendation,
    /// Active or eligible observations outside their image domains.
    pub invalid_image_observation_ids: Vec<u64>,
    /// Active landmarks lacking sufficient optimize-only seed provenance.
    pub unproven_seed_landmark_ids: Vec<u64>,
    /// Upstream eligible held-out observations whose cameras are inactive.
    pub inactive_held_out_observation_ids: Vec<u64>,
    /// Held-out observations surviving all independence gates.
    pub independent_held_out_observation_ids: Vec<u64>,
    /// Distinct active cameras represented by independent held-out evidence.
    pub independent_held_out_camera_node_ids: Vec<u64>,
}

/// Immutable deterministic bundle-admission audit generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BundleAdmissionGeneration {
    /// Exact immutable basis.
    pub basis: BundleAdmissionBasis,
    /// Exact instantiated policy.
    pub policy: BundleAdmissionPolicy,
    /// Exact structural input retained for replay.
    pub bundle_problem: BundleProblemGeneration,
    /// Canonical camera domains retained for replay.
    pub camera_domains: Vec<BundleCameraDomain>,
    /// Canonical seed provenance retained for replay.
    pub seed_provenance: Vec<LandmarkSeedProvenance>,
    /// Observation audits in canonical observation order.
    pub observations: Vec<BundleObservationAudit>,
    /// Landmark audits in canonical landmark order.
    pub landmarks: Vec<LandmarkSeedAudit>,
    /// Component decisions in canonical root order.
    pub components: Vec<BundleAdmissionComponent>,
    /// Exact deterministic work charged; operation evidence only.
    pub operation_count: u64,
}

/// Stable bundle-admission failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum BundleAdmissionError {
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
    /// A semantic minimum was not met.
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
    /// One exact basis identity disagreed with independently recomputed evidence.
    BasisMismatch {
        /// Stable basis field.
        field: &'static str,
        /// Required identity.
        expected: EvidenceDigest,
        /// Independently observed identity.
        observed: EvidenceDigest,
    },
    /// A camera-domain row was duplicated.
    DuplicateCameraDomain {
        /// Duplicate camera node.
        camera_node_id: u64,
    },
    /// A structural camera lacked an image-domain binding.
    MissingCameraDomain {
        /// Camera node.
        camera_node_id: u64,
    },
    /// An image-domain binding named no structural camera.
    ExtraCameraDomain {
        /// Camera node.
        camera_node_id: u64,
    },
    /// Camera-domain frame identity disagreed with the structural binding.
    CameraFrameMismatch {
        /// Camera node.
        camera_node_id: u64,
    },
    /// Camera-domain calibration identity disagreed with the structural binding.
    CameraCalibrationMismatch {
        /// Camera node.
        camera_node_id: u64,
    },
    /// A seed-provenance row was duplicated.
    DuplicateSeedProvenance {
        /// Duplicate landmark.
        landmark_id: u64,
    },
    /// A structural landmark lacked seed provenance.
    MissingSeedProvenance {
        /// Landmark identity.
        landmark_id: u64,
    },
    /// A seed-provenance row named no structural landmark.
    ExtraSeedProvenance {
        /// Landmark identity.
        landmark_id: u64,
    },
    /// One support observation was duplicated inside one seed-provenance row.
    DuplicateSeedSupportObservation {
        /// Landmark identity.
        landmark_id: u64,
        /// Duplicate observation.
        observation_id: u64,
    },
    /// Seed provenance named an unknown observation.
    UnknownSeedSupportObservation {
        /// Landmark identity.
        landmark_id: u64,
        /// Missing observation.
        observation_id: u64,
    },
    /// Seed provenance named an observation of another landmark.
    SeedSupportLandmarkMismatch {
        /// Declared landmark.
        landmark_id: u64,
        /// Observation identity.
        observation_id: u64,
        /// Landmark actually observed.
        observed_landmark_id: u64,
    },
    /// A held-out observation was used to initialize a landmark seed.
    SeedSupportUsesHeldOut {
        /// Landmark identity.
        landmark_id: u64,
        /// Held-out observation.
        observation_id: u64,
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
    /// An expected upstream or audit record was absent internally.
    InternalLookup {
        /// Stable lookup name.
        field: &'static str,
    },
    /// Deterministic replay disagreed with retained output.
    DerivedMismatch {
        /// Stable derived field.
        field: &'static str,
    },
    /// Structural bundle-problem validation failed.
    BundleProblem(BundleProblemError),
    /// Canonical codec failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for BundleAdmissionError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => {
                write!(formatter, "bundle-admission identity {field} must not be all zero")
            }
            Self::ZeroValue { field } => {
                write!(formatter, "bundle-admission field {field} must be nonzero")
            }
            Self::BelowMinimum {
                field,
                actual,
                minimum,
            } => write!(
                formatter,
                "bundle-admission field {field} is {actual}; minimum is {minimum}"
            ),
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "bundle-admission field {field} is {actual}; maximum is {maximum}"
            ),
            Self::BasisMismatch {
                field,
                expected,
                observed,
            } => write!(
                formatter,
                "bundle-admission basis {field} expected {expected}, observed {observed}"
            ),
            Self::DuplicateCameraDomain { camera_node_id } => write!(
                formatter,
                "bundle camera domain {camera_node_id} is duplicated"
            ),
            Self::MissingCameraDomain { camera_node_id } => write!(
                formatter,
                "bundle camera {camera_node_id} lacks an exact image-domain binding"
            ),
            Self::ExtraCameraDomain { camera_node_id } => write!(
                formatter,
                "bundle camera domain {camera_node_id} is absent from the structural problem"
            ),
            Self::CameraFrameMismatch { camera_node_id } => write!(
                formatter,
                "bundle camera domain {camera_node_id} uses the wrong frame identity"
            ),
            Self::CameraCalibrationMismatch { camera_node_id } => write!(
                formatter,
                "bundle camera domain {camera_node_id} uses the wrong effective calibration identity"
            ),
            Self::DuplicateSeedProvenance { landmark_id } => write!(
                formatter,
                "bundle seed provenance for landmark {landmark_id} is duplicated"
            ),
            Self::MissingSeedProvenance { landmark_id } => write!(
                formatter,
                "bundle landmark {landmark_id} lacks seed provenance"
            ),
            Self::ExtraSeedProvenance { landmark_id } => write!(
                formatter,
                "bundle seed provenance names unknown landmark {landmark_id}"
            ),
            Self::DuplicateSeedSupportObservation {
                landmark_id,
                observation_id,
            } => write!(
                formatter,
                "bundle landmark {landmark_id} repeats seed-support observation {observation_id}"
            ),
            Self::UnknownSeedSupportObservation {
                landmark_id,
                observation_id,
            } => write!(
                formatter,
                "bundle landmark {landmark_id} names unknown seed-support observation {observation_id}"
            ),
            Self::SeedSupportLandmarkMismatch {
                landmark_id,
                observation_id,
                observed_landmark_id,
            } => write!(
                formatter,
                "bundle landmark {landmark_id} names seed-support observation {observation_id} of landmark {observed_landmark_id}"
            ),
            Self::SeedSupportUsesHeldOut {
                landmark_id,
                observation_id,
            } => write!(
                formatter,
                "bundle landmark {landmark_id} uses held-out observation {observation_id} for seed initialization"
            ),
            Self::OperationBudgetExceeded { attempted, maximum } => write!(
                formatter,
                "bundle-admission audit attempted operation {attempted}; maximum is {maximum}"
            ),
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "bundle-admission arithmetic overflowed while computing {field}"
            ),
            Self::InternalLookup { field } => write!(
                formatter,
                "bundle-admission internal lookup failed for {field}"
            ),
            Self::DerivedMismatch { field } => write!(
                formatter,
                "bundle-admission derived field {field} disagrees with deterministic replay"
            ),
            Self::BundleProblem(error) => {
                write!(formatter, "bundle-admission structural problem error: {error}")
            }
            Self::Codec(error) => write!(formatter, "bundle-admission codec error: {error}"),
            Self::Domain(error) => write!(
                formatter,
                "bundle-admission identity-domain error: {error}"
            ),
            Self::JsonRendering(error) => write!(
                formatter,
                "bundle-admission JSON rendering failed: {error}"
            ),
        }
    }
}

impl Error for BundleAdmissionError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::BundleProblem(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<BundleProblemError> for BundleAdmissionError {
    fn from(error: BundleProblemError) -> Self {
        Self::BundleProblem(error)
    }
}

impl From<CodecError> for BundleAdmissionError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for BundleAdmissionError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
