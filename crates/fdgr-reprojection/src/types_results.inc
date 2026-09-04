/// Stable disposition of one exact bundle observation under the scalar oracle.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ReprojectionObservationDisposition {
    /// Upstream structural or admission logic excluded this observation from evaluation.
    UpstreamExcluded,
    /// Projection and both residual gates passed.
    ProjectedInlier,
    /// The predicted coordinate lies outside the exact calibrated image domain.
    ProjectedOutsideImageDomain,
    /// Absolute image residual exceeded policy.
    ResidualExceeded,
    /// Residual normalized by declared uncertainty exceeded policy.
    NormalizedResidualExceeded,
    /// Point depth was nonpositive or too close to the projection singularity.
    InsufficientPositiveDepth,
    /// Undistorted normalized camera coordinates exceeded the admitted domain.
    NormalizedCoordinateExceeded,
    /// The exact calibration model is not implemented by this scalar oracle.
    ProjectionModelUnsupported,
    /// Static reprojection cannot adjudicate a rolling-shutter image without a motion model.
    RollingShutterUnsupported,
    /// Projection exceeded a constitutional numeric bound.
    ProjectionBoundExceeded,
    /// Checked fixed-point projection arithmetic failed.
    ProjectionArithmeticFailure,
}

impl ReprojectionObservationDisposition {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::UpstreamExcluded => "upstream_excluded",
            Self::ProjectedInlier => "projected_inlier",
            Self::ProjectedOutsideImageDomain => "projected_outside_image_domain",
            Self::ResidualExceeded => "residual_exceeded",
            Self::NormalizedResidualExceeded => "normalized_residual_exceeded",
            Self::InsufficientPositiveDepth => "insufficient_positive_depth",
            Self::NormalizedCoordinateExceeded => "normalized_coordinate_exceeded",
            Self::ProjectionModelUnsupported => "projection_model_unsupported",
            Self::RollingShutterUnsupported => "rolling_shutter_unsupported",
            Self::ProjectionBoundExceeded => "projection_bound_exceeded",
            Self::ProjectionArithmeticFailure => "projection_arithmetic_failure",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::UpstreamExcluded => 1,
            Self::ProjectedInlier => 2,
            Self::ProjectedOutsideImageDomain => 3,
            Self::ResidualExceeded => 4,
            Self::NormalizedResidualExceeded => 5,
            Self::InsufficientPositiveDepth => 6,
            Self::NormalizedCoordinateExceeded => 7,
            Self::ProjectionModelUnsupported => 8,
            Self::RollingShutterUnsupported => 9,
            Self::ProjectionBoundExceeded => 10,
            Self::ProjectionArithmeticFailure => 11,
        }
    }

    const fn is_projected(self) -> bool {
        matches!(
            self,
            Self::ProjectedInlier
                | Self::ProjectedOutsideImageDomain
                | Self::ResidualExceeded
                | Self::NormalizedResidualExceeded
        )
    }

    const fn is_projection_failure(self) -> bool {
        matches!(
            self,
            Self::InsufficientPositiveDepth
                | Self::NormalizedCoordinateExceeded
                | Self::ProjectionModelUnsupported
                | Self::RollingShutterUnsupported
                | Self::ProjectionBoundExceeded
                | Self::ProjectionArithmeticFailure
        )
    }
}

/// Reprojection evidence for one exact observation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReprojectionObservationEvidence {
    /// Stable bundle-observation identity.
    pub observation_id: u64,
    /// Stable landmark identity.
    pub landmark_id: u64,
    /// Stable camera node identity.
    pub camera_node_id: u64,
    /// Canonical pose-component root.
    pub component_root_node_id: u64,
    /// Optimize versus independent held-out role.
    pub role: BundleObservationRole,
    /// Predicted horizontal coordinate when projection succeeded.
    pub projected_x_nano_pixels: Option<i64>,
    /// Predicted vertical coordinate when projection succeeded.
    pub projected_y_nano_pixels: Option<i64>,
    /// Positive camera-frame depth when projection succeeded.
    pub depth_nano: Option<i64>,
    /// Predicted minus observed horizontal residual.
    pub residual_x_nano_pixels: Option<i64>,
    /// Predicted minus observed vertical residual.
    pub residual_y_nano_pixels: Option<i64>,
    /// Euclidean residual magnitude.
    pub residual_nano_pixels: Option<u64>,
    /// Residual normalized by the larger declared observation/calibration uncertainty.
    pub normalized_residual_ppm: Option<u64>,
    /// Declared observation-localization uncertainty.
    pub observation_uncertainty_nano_pixels: u64,
    /// Declared effective-calibration uncertainty.
    pub calibration_uncertainty_nano_pixels: u64,
    /// Exact half-open image-domain result when projection succeeded.
    pub within_image_domain: Option<bool>,
    /// Stable final disposition.
    pub disposition: ReprojectionObservationDisposition,
}

/// Component maturity after scalar reprojection adjudication.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ReprojectionComponentStatus {
    /// Upstream bundle admission blocked numerical use.
    BlockedAdmission,
    /// At least one required observation could not be projected under the exact model.
    ProjectionConflict,
    /// No optimize observation produced a residual.
    NoOptimizeEvidence,
    /// Too few optimize observations produced residual evidence.
    InsufficientOptimizeEvidence,
    /// Too few independently held-out observations produced residual evidence.
    InsufficientHeldOutEvidence,
    /// At least one projected observation violated image-domain or residual policy.
    ResidualConflict,
    /// Upstream admission permits diagnostic evaluation only.
    UpstreamDiagnostic,
    /// Every required observation projected and passed all gates.
    Consistent,
}

impl ReprojectionComponentStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::BlockedAdmission => "blocked_admission",
            Self::ProjectionConflict => "projection_conflict",
            Self::NoOptimizeEvidence => "no_optimize_evidence",
            Self::InsufficientOptimizeEvidence => "insufficient_optimize_evidence",
            Self::InsufficientHeldOutEvidence => "insufficient_held_out_evidence",
            Self::ResidualConflict => "residual_conflict",
            Self::UpstreamDiagnostic => "upstream_diagnostic",
            Self::Consistent => "consistent",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::BlockedAdmission => 1,
            Self::ProjectionConflict => 2,
            Self::NoOptimizeEvidence => 3,
            Self::InsufficientOptimizeEvidence => 4,
            Self::InsufficientHeldOutEvidence => 5,
            Self::ResidualConflict => 6,
            Self::UpstreamDiagnostic => 7,
            Self::Consistent => 8,
        }
    }
}

/// Reprojection decision for one exact pose component.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ReprojectionDecision {
    /// The exact state must not enter numerical optimization.
    Block,
    /// The state may be inspected diagnostically but cannot support promotion.
    AdmitDiagnostic,
    /// The exact state may enter bounded optimizer evaluation.
    Admit,
}

impl ReprojectionDecision {
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

/// Minimum-cost next action compiled from reprojection evidence.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ReprojectionRecommendation {
    /// Repair the upstream structural or admission defect first.
    ResolveBundleAdmission,
    /// Add or retain enough optimize observations.
    AddOptimizeEvidence,
    /// Reserve independently held-out observations from active cameras.
    ReserveIndependentHeldOutViews,
    /// Supply the required time-varying or otherwise exact projection model.
    SupplyExactProjectionModel,
    /// Revisit camera state, landmarks, calibration, or observation evidence.
    RevisitStateOrCalibration,
    /// Proceed only to bounded optimizer evaluation on this exact basis.
    ProceedToBoundedOptimization,
}

impl ReprojectionRecommendation {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ResolveBundleAdmission => "resolve_bundle_admission",
            Self::AddOptimizeEvidence => "add_optimize_evidence",
            Self::ReserveIndependentHeldOutViews => "reserve_independent_held_out_views",
            Self::SupplyExactProjectionModel => "supply_exact_projection_model",
            Self::RevisitStateOrCalibration => "revisit_state_or_calibration",
            Self::ProceedToBoundedOptimization => "proceed_to_bounded_optimization",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ResolveBundleAdmission => 1,
            Self::AddOptimizeEvidence => 2,
            Self::ReserveIndependentHeldOutViews => 3,
            Self::SupplyExactProjectionModel => 4,
            Self::RevisitStateOrCalibration => 5,
            Self::ProceedToBoundedOptimization => 6,
        }
    }
}

/// Residual summary for one observation partition.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReprojectionResidualSummary {
    /// Number of observations eligible for this partition.
    pub eligible_observation_count: u64,
    /// Number producing a projected residual.
    pub projected_observation_count: u64,
    /// Number passing image-domain and residual gates.
    pub inlier_observation_count: u64,
    /// Root-mean-square residual over projected observations.
    pub rms_residual_nano_pixels: Option<u64>,
    /// Median residual over projected observations.
    pub median_residual_nano_pixels: Option<u64>,
    /// Maximum residual over projected observations.
    pub maximum_residual_nano_pixels: Option<u64>,
}

/// Component-level reprojection certificate and decision card.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReprojectionComponent {
    /// Canonical pose-component root.
    pub component_root_node_id: u64,
    /// Exact arbitrary relative-scale gauge.
    pub scale_component_root_edge_id: Option<u64>,
    /// Final status.
    pub status: ReprojectionComponentStatus,
    /// Final decision.
    pub decision: ReprojectionDecision,
    /// Minimum-cost next action.
    pub recommendation: ReprojectionRecommendation,
    /// Optimize-partition residual summary.
    pub optimize: ReprojectionResidualSummary,
    /// Independent held-out residual summary.
    pub held_out: ReprojectionResidualSummary,
    /// Required observations that could not be projected.
    pub unprojectable_observation_ids: Vec<u64>,
    /// Projected observations violating image-domain or residual policy.
    pub conflicting_observation_ids: Vec<u64>,
}

/// Immutable deterministic reprojection evaluation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReprojectionEvaluation {
    /// Exact immutable basis.
    pub basis: ReprojectionBasis,
    /// Exact instantiated policy.
    pub policy: ReprojectionPolicy,
    /// Exact audited bundle admission retained for replay.
    pub bundle_admission: BundleAdmissionGeneration,
    /// Canonical effective calibrations retained for replay.
    pub calibrations: Vec<ReprojectionCalibration>,
    /// Observation evidence in canonical observation order.
    pub observations: Vec<ReprojectionObservationEvidence>,
    /// Component decisions in canonical root order.
    pub components: Vec<ReprojectionComponent>,
    /// Exact deterministic work charged; operation evidence only.
    pub operation_count: u64,
}
