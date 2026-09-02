/// Public schema identity for one two-view relative-pose verification generation.
pub const RELATIVE_POSE_VERIFICATION_SCHEMA: &str = "fdgr.relative_pose_verification/1";
/// Fixed unit-vector and rotation scale.
pub const UNIT_NANO: i64 = 1_000_000_000;
/// Parts-per-million denominator.
pub const PPM_SCALE: u32 = 1_000_000;
/// Maximum bearing matches in one verification generation.
pub const MAX_BEARING_MATCHES: usize = 50_000;
/// Maximum pose candidates in one verification generation.
pub const MAX_POSE_CANDIDATES: usize = 64;
/// Maximum match/candidate evaluations.
pub const MAX_POSE_EVALUATIONS: u64 = 2_000_000;
/// Largest integer identity emitted as a JSON number without IEEE-754 loss.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;
/// Maximum admitted bearing uncertainty.
pub const MAX_BEARING_UNCERTAINTY_NANO: u64 = 250_000_000;
/// Maximum unit-vector norm error.
pub const MAX_UNIT_NORM_ERROR_PPM: u64 = 10_000;
/// Maximum rotation row/column norm or orthogonality error.
pub const MAX_ROTATION_ERROR_PPM: u64 = 5_000;
/// Maximum rotation determinant error.
pub const MAX_ROTATION_DETERMINANT_ERROR_PPM: u64 = 10_000;

/// Exact immutable basis for one two-view verification.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RelativePoseBasis {
    /// Exact correspondence generation supplying candidate match identities.
    pub correspondence_generation_digest: EvidenceDigest,
    /// Exact calibration/derived-calibration generation supplying bearings.
    pub calibration_digest: EvidenceDigest,
    /// Exact bearing-match table identity.
    pub bearing_basis_digest: EvidenceDigest,
    /// Exact pose-candidate table identity.
    pub candidate_basis_digest: EvidenceDigest,
    /// Exact semantic policy identity.
    pub policy_digest: EvidenceDigest,
    /// Canonical left keyframe sample index.
    pub left_sample_index: u64,
    /// Canonical right keyframe sample index.
    pub right_sample_index: u64,
    /// Immutable verification generation.
    pub generation: u64,
}

/// One calibrated correspondence represented as unit bearings in each camera frame.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BearingMatch {
    /// Stable nonzero match identity.
    pub match_id: u64,
    /// Left correspondence observation identity.
    pub left_observation_id: u64,
    /// Right correspondence observation identity.
    pub right_observation_id: u64,
    /// Left-camera unit bearing multiplied by [`UNIT_NANO`].
    pub left_bearing_nano: [i64; 3],
    /// Right-camera unit bearing multiplied by [`UNIT_NANO`].
    pub right_bearing_nano: [i64; 3],
    /// Conservative angular/bearing uncertainty in nano-units.
    pub uncertainty_nano: u64,
}

/// Provenance class of one relative-pose candidate. No source class grants authority.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PoseCandidateSource {
    /// Candidate from a calibrated five-point solver.
    FivePoint,
    /// Candidate from a calibrated eight-point solver.
    EightPoint,
    /// Candidate proposed from device motion evidence.
    TelemetryPrior,
    /// Candidate proposed by a learned or foundation model.
    ModelPrior,
    /// Explicit diagnostic or manually constructed hypothesis.
    DiagnosticHypothesis,
}

impl PoseCandidateSource {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::FivePoint => "five_point",
            Self::EightPoint => "eight_point",
            Self::TelemetryPrior => "telemetry_prior",
            Self::ModelPrior => "model_prior",
            Self::DiagnosticHypothesis => "diagnostic_hypothesis",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::FivePoint => 1,
            Self::EightPoint => 2,
            Self::TelemetryPrior => 3,
            Self::ModelPrior => 4,
            Self::DiagnosticHypothesis => 5,
        }
    }
}

/// One exact left-to-right rigid-motion candidate up to translation scale.
///
/// The convention is `x_right = R_left_to_right * x_left + t_left_origin_in_right`. The
/// translation vector is a unit direction only; this structure cannot establish metric baseline.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RelativePoseCandidate {
    /// Stable nonzero candidate identity.
    pub candidate_id: u64,
    /// Exact candidate-generation evidence identity.
    pub evidence_digest: EvidenceDigest,
    /// Candidate source class.
    pub source: PoseCandidateSource,
    /// Row-major left-to-right rotation multiplied by [`UNIT_NANO`].
    pub rotation_left_to_right_nano: [i64; 9],
    /// Unit translation direction multiplied by [`UNIT_NANO`].
    pub translation_left_origin_in_right_nano: [i64; 3],
}

/// Deterministic geometric-verification gates.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct RelativePosePolicy {
    /// Maximum normalized epipolar residual before per-match uncertainty is added.
    pub max_epipolar_residual_nano: u64,
    /// Minimum epipolar-plane normal magnitude; lower values are epipole-degenerate.
    pub min_epipolar_normal_nano: u64,
    /// Minimum `1 - |cos(parallax)|` in nano-units.
    pub min_parallax_nano: u64,
    /// Minimum accepted matches.
    pub min_inlier_matches: u32,
    /// Minimum accepted-match ratio.
    pub min_inlier_ratio_ppm: u32,
    /// Minimum positive-depth ratio across all matches.
    pub min_positive_depth_ratio_ppm: u32,
    /// Maximum admitted median residual among inliers.
    pub max_median_residual_nano: u64,
    /// Require both triangulated depth numerators to be positive.
    pub require_cheirality: bool,
    /// Hard operation budget. It limits execution but is excluded from successful semantic identity.
    pub max_evaluations: u64,
}

impl Default for RelativePosePolicy {
    fn default() -> Self {
        Self {
            max_epipolar_residual_nano: 5_000_000,
            min_epipolar_normal_nano: 10_000_000,
            min_parallax_nano: 500_000,
            min_inlier_matches: 12,
            min_inlier_ratio_ppm: 600_000,
            min_positive_depth_ratio_ppm: 600_000,
            max_median_residual_nano: 2_500_000,
            require_cheirality: true,
            max_evaluations: 1_000_000,
        }
    }
}

/// Stable reason one match does not support one candidate.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PoseMatchRejectionReason {
    /// Translation and the rotated left bearing did not define a stable epipolar plane.
    EpipolarPlaneDegenerate,
    /// Normalized epipolar residual exceeded the uncertainty-aware gate.
    EpipolarResidualExceeded,
    /// Two-view angular diversity was below the declared threshold.
    ParallaxInsufficient,
    /// The least-squares depth numerators were not both positive.
    CheiralityFailed,
}

impl PoseMatchRejectionReason {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::EpipolarPlaneDegenerate => "epipolar_plane_degenerate",
            Self::EpipolarResidualExceeded => "epipolar_residual_exceeded",
            Self::ParallaxInsufficient => "parallax_insufficient",
            Self::CheiralityFailed => "cheirality_failed",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::EpipolarPlaneDegenerate => 1,
            Self::EpipolarResidualExceeded => 2,
            Self::ParallaxInsufficient => 3,
            Self::CheiralityFailed => 4,
        }
    }
}

/// Candidate-specific result for one bearing match.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseMatchAssessment {
    /// Match identity.
    pub match_id: u64,
    /// Normalized epipolar residual in nano-units.
    pub epipolar_residual_nano: u64,
    /// `1 - |cos(parallax)|` in nano-units.
    pub parallax_nano: u64,
    /// Whether the left depth numerator was positive.
    pub left_depth_positive: bool,
    /// Whether the right depth numerator was positive.
    pub right_depth_positive: bool,
    /// Whether this match passed every enabled gate.
    pub inlier: bool,
    /// Stable rejection reason when not an inlier.
    pub rejection_reason: Option<PoseMatchRejectionReason>,
}

/// Complete deterministic evaluation of one pose candidate.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PoseCandidateEvaluation {
    /// Candidate identity.
    pub candidate_id: u64,
    /// Candidate evidence identity.
    pub evidence_digest: EvidenceDigest,
    /// Candidate source class.
    pub source: PoseCandidateSource,
    /// Number of matches evaluated.
    pub match_count: u32,
    /// Number of matches passing all enabled gates.
    pub inlier_count: u32,
    /// Inlier ratio in parts per million.
    pub inlier_ratio_ppm: u32,
    /// Number of matches with two positive depth numerators.
    pub positive_depth_count: u32,
    /// Positive-depth ratio in parts per million.
    pub positive_depth_ratio_ppm: u32,
    /// Median epipolar residual among inliers, or the maximum sentinel when none survived.
    pub median_inlier_residual_nano: u64,
    /// Maximum epipolar residual among inliers, or the maximum sentinel when none survived.
    pub maximum_inlier_residual_nano: u64,
    /// Median parallax among inliers.
    pub median_inlier_parallax_nano: u64,
    /// Whether the candidate independently satisfied all aggregate gates.
    pub accepted: bool,
    /// Per-match evidence in canonical match order.
    pub assessments: Vec<PoseMatchAssessment>,
}

/// Resolution status for the exact candidate set.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RelativePoseStatus {
    /// No candidate independently satisfied the aggregate gates.
    NoAcceptedCandidate,
    /// Multiple candidates shared the best semantic score.
    Ambiguous,
    /// One candidate uniquely achieved the best accepted score.
    GeometricallyVerified,
}

impl RelativePoseStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::NoAcceptedCandidate => "no_accepted_candidate",
            Self::Ambiguous => "ambiguous",
            Self::GeometricallyVerified => "geometrically_verified",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::NoAcceptedCandidate => 1,
            Self::Ambiguous => 2,
            Self::GeometricallyVerified => 3,
        }
    }
}

/// Immutable verification over one exact bearing/candidate set.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RelativePoseVerification {
    /// Exact immutable basis.
    pub basis: RelativePoseBasis,
    /// Exact semantic and operation policy.
    pub policy: RelativePosePolicy,
    /// Canonically sorted input matches retained for replay.
    pub matches: Vec<BearingMatch>,
    /// Canonically sorted input candidates retained for replay.
    pub candidates: Vec<RelativePoseCandidate>,
    /// Candidate evaluations in canonical candidate order.
    pub evaluations: Vec<PoseCandidateEvaluation>,
    /// Candidate-set resolution status.
    pub status: RelativePoseStatus,
    /// Unique selected candidate only when status is geometrically verified.
    pub selected_candidate_id: Option<u64>,
    /// Canonically sorted tied best candidate identities when ambiguous.
    pub ambiguous_candidate_ids: Vec<u64>,
    /// Exact evaluation count. This is operation evidence and is excluded from semantic identity.
    pub evaluation_count: u64,
}
