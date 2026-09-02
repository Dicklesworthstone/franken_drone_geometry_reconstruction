/// Public schema identity for one deterministic epipolar verification generation.
pub const EPIPOLAR_VERIFICATION_SCHEMA: &str = "fdgr.epipolar_verification/1";
/// Fixed scale for normalized image coordinates. `1_000_000` represents one focal length.
pub const COORDINATE_SCALE_PPM: i64 = 1_000_000;
/// Maximum absolute normalized x/y coordinate.
pub const MAX_ABS_COORDINATE_PPM: i64 = 8_000_000;
/// Maximum normalized essential-matrix coefficient after homogeneous canonicalization.
pub const MAX_MATRIX_COEFFICIENT: i64 = 1_000_000;
/// Maximum raw coefficient accepted before canonicalization.
pub const MAX_RAW_MATRIX_COEFFICIENT: i64 = 1_000_000_000_000;
/// Maximum calibrated correspondence observations in one generation.
pub const MAX_EPIPOLAR_OBSERVATIONS: usize = 100_000;
/// Maximum candidate essential matrices in one generation.
pub const MAX_EPIPOLAR_CANDIDATES: usize = 128;
/// Maximum candidate/observation residual evaluations.
pub const MAX_EPIPOLAR_EVALUATIONS: u64 = 2_000_000;
/// Maximum normalized-coordinate uncertainty or residual gate.
pub const MAX_EPIPOLAR_RESIDUAL_PPM: u64 = 10_000_000;
/// Largest integer identity emitted as an unquoted JSON number.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one two-view geometric-verification generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EpipolarBasis {
    /// Exact descriptor-correspondence generation being adjudicated.
    pub correspondence_generation_digest: EvidenceDigest,
    /// Exact calibration/derived-calibration basis used to normalize image points.
    pub calibration_digest: EvidenceDigest,
    /// Exact normalized-observation table identity.
    pub observation_basis_digest: EvidenceDigest,
    /// Exact essential-candidate table identity.
    pub candidate_basis_digest: EvidenceDigest,
    /// Exact policy registry identity.
    pub policy_digest: EvidenceDigest,
    /// Stable descriptor frame-pair identity.
    pub pair_id: u64,
    /// Canonical left keyframe sample index.
    pub left_sample_index: u64,
    /// Canonical right keyframe sample index.
    pub right_sample_index: u64,
    /// Immutable verification generation.
    pub generation: u64,
}

/// One calibrated descriptor correspondence in normalized image coordinates.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NormalizedCorrespondence {
    /// Stable descriptor-match identity.
    pub match_id: u64,
    /// Stable frame-pair identity; must equal the generation basis.
    pub pair_id: u64,
    /// Left feature-observation identity.
    pub left_observation_id: u64,
    /// Right feature-observation identity.
    pub right_observation_id: u64,
    /// Left normalized x coordinate in parts per million of focal length.
    pub left_x_ppm: i64,
    /// Left normalized y coordinate in parts per million of focal length.
    pub left_y_ppm: i64,
    /// Right normalized x coordinate in parts per million of focal length.
    pub right_x_ppm: i64,
    /// Right normalized y coordinate in parts per million of focal length.
    pub right_y_ppm: i64,
    /// Symmetric normalized-coordinate uncertainty in parts per million.
    pub uncertainty_ppm: u32,
    /// Nonzero left-image spatial bin.
    pub left_spatial_bin: u16,
    /// Nonzero right-image spatial bin.
    pub right_spatial_bin: u16,
}

/// Provenance class of an essential-matrix candidate.
#[derive(Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub enum CandidateSource {
    /// Deterministic native minimal solver.
    NativeMinimalSolver,
    /// Deterministic native overdetermined solver.
    NativeLinearSolver,
    /// Telemetry or gimbal prior converted to an essential hypothesis.
    TelemetryPrior,
    /// Isolated learned/model-worker proposal.
    ModelWorker,
    /// Supervised external reference oracle.
    ExternalOracle,
}

impl CandidateSource {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::NativeMinimalSolver => "native_minimal_solver",
            Self::NativeLinearSolver => "native_linear_solver",
            Self::TelemetryPrior => "telemetry_prior",
            Self::ModelWorker => "model_worker",
            Self::ExternalOracle => "external_oracle",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::NativeMinimalSolver => 1,
            Self::NativeLinearSolver => 2,
            Self::TelemetryPrior => 3,
            Self::ModelWorker => 4,
            Self::ExternalOracle => 5,
        }
    }
}

/// One homogeneous essential-matrix proposal.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EssentialCandidate {
    /// Stable candidate identity.
    pub candidate_id: u64,
    /// Proposal source.
    pub source: CandidateSource,
    /// Row-major homogeneous 3×3 matrix. The builder divides by the coefficient GCD and makes the
    /// first nonzero coefficient positive before retaining this value.
    pub matrix: [i64; 9],
}

/// Constitutional geometric-verification gates.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct EpipolarPolicy {
    /// Base symmetric L1 epipolar residual gate in normalized-coordinate ppm.
    pub max_residual_ppm: u64,
    /// Minimum exact inlier count.
    pub min_inliers: u32,
    /// Minimum inlier ratio in parts per million.
    pub min_inlier_ratio_ppm: u32,
    /// Minimum unique support bins in each image.
    pub min_spatial_bins_per_image: u16,
    /// Maximum scale-invariant determinant residual in parts per million.
    pub max_determinant_residual_ppm: u64,
    /// Minimum inlier-count lead over a second gate-passing candidate.
    pub min_inlier_margin: u32,
    /// Hard operation budget. It bounds execution but is excluded from successful semantic identity.
    pub max_evaluations: u64,
}

impl Default for EpipolarPolicy {
    fn default() -> Self {
        Self {
            max_residual_ppm: 2_000,
            min_inliers: 8,
            min_inlier_ratio_ppm: 700_000,
            min_spatial_bins_per_image: 4,
            max_determinant_residual_ppm: 10_000,
            min_inlier_margin: 2,
            max_evaluations: 250_000,
        }
    }
}

/// Stable geometric disposition for one candidate/match residual.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ResidualDisposition {
    /// Residual was defined and within the uncertainty-aware gate.
    Inlier,
    /// Residual was defined but exceeded the uncertainty-aware gate.
    ResidualExceeded,
    /// The candidate induced no usable epipolar line normal for this correspondence.
    UndefinedLine,
}

impl ResidualDisposition {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Inlier => "inlier",
            Self::ResidualExceeded => "residual_exceeded",
            Self::UndefinedLine => "undefined_line",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::Inlier => 1,
            Self::ResidualExceeded => 2,
            Self::UndefinedLine => 3,
        }
    }
}

/// Exact residual evidence for one candidate/match pair.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EpipolarResidual {
    /// Candidate identity.
    pub candidate_id: u64,
    /// Descriptor-match identity.
    pub match_id: u64,
    /// Conservative symmetric L1 residual, absent when the line normal is undefined.
    pub residual_ppm: Option<u64>,
    /// Exact base-plus-observation uncertainty gate.
    pub gate_ppm: u64,
    /// Geometric disposition.
    pub disposition: ResidualDisposition,
}

/// Deterministic evaluation of one normalized candidate.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CandidateEvaluation {
    /// Candidate identity.
    pub candidate_id: u64,
    /// Scale-invariant determinant residual.
    pub determinant_residual_ppm: u64,
    /// Whether the determinant gate passed.
    pub determinant_gate_passed: bool,
    /// Exact inlier count.
    pub inlier_count: u32,
    /// Exact outlier/undefined count.
    pub outlier_count: u32,
    /// Inlier ratio in parts per million.
    pub inlier_ratio_ppm: u32,
    /// Unique left-image spatial bins among inliers.
    pub left_spatial_bin_count: u16,
    /// Unique right-image spatial bins among inliers.
    pub right_spatial_bin_count: u16,
    /// Median inlier residual; zero when no inlier exists.
    pub median_inlier_residual_ppm: u64,
    /// Maximum inlier residual; zero when no inlier exists.
    pub maximum_inlier_residual_ppm: u64,
    /// Whether every constitutional admission gate passed.
    pub passes_admission_gates: bool,
    /// Canonically sorted inlier match identities.
    pub inlier_match_ids: Vec<u64>,
    /// Canonically sorted rejected/undefined match identities.
    pub outlier_match_ids: Vec<u64>,
    /// Residual evidence in canonical match order.
    pub residuals: Vec<EpipolarResidual>,
}

/// Conservative generation-level decision.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum VerificationDecision {
    /// No candidate passed all admission gates.
    Rejected,
    /// At least two candidates passed, but the leader did not clear the configured support margin.
    Ambiguous,
    /// One candidate cleared all gates and the configured runner-up margin.
    EpipolarSupportedHypothesis,
}

impl VerificationDecision {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Rejected => "rejected",
            Self::Ambiguous => "ambiguous",
            Self::EpipolarSupportedHypothesis => "epipolar_supported_hypothesis",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::Rejected => 1,
            Self::Ambiguous => 2,
            Self::EpipolarSupportedHypothesis => 3,
        }
    }
}

/// Immutable deterministic verification result.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EpipolarVerification {
    /// Exact immutable basis.
    pub basis: EpipolarBasis,
    /// Exact instantiated policy.
    pub policy: EpipolarPolicy,
    /// Canonically sorted calibrated correspondences.
    pub observations: Vec<NormalizedCorrespondence>,
    /// Canonically sorted and homogeneous-normalized candidates.
    pub candidates: Vec<EssentialCandidate>,
    /// Candidate evaluations in canonical candidate order.
    pub evaluations: Vec<CandidateEvaluation>,
    /// Conservative decision.
    pub decision: VerificationDecision,
    /// Leading candidate whether or not it was admitted.
    pub best_candidate_id: Option<u64>,
    /// Runner-up candidate under the deterministic ranking.
    pub runner_up_candidate_id: Option<u64>,
    /// Candidate admitted only under `epipolar_supported_hypothesis`.
    pub admitted_candidate_id: Option<u64>,
    /// Best minus runner-up inlier count, or best inlier count when no runner-up exists.
    pub inlier_margin: u32,
    /// Exact number of candidate/observation residual evaluations charged. This is operation
    /// evidence and is excluded from the semantic generation digest.
    pub evaluation_count: u64,
}
