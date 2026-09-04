/// Public schema identity for one deterministic reprojection evaluation.
pub const REPROJECTION_EVALUATION_SCHEMA: &str = "fdgr.reprojection_evaluation/1";
/// Semantic identity domain for the scalar reference algorithm.
pub const REPROJECTION_ALGORITHM_SCHEMA: &str = "fdgr.reprojection_algorithm.scalar/1";
/// Semantic identity domain for reprojection policy.
pub const REPROJECTION_POLICY_SCHEMA: &str = "fdgr.reprojection_policy/1";
/// Semantic identity domain for exact effective-calibration tables.
pub const REPROJECTION_CALIBRATION_TABLE_SCHEMA: &str =
    "fdgr.reprojection_calibration_table/1";
/// Positive authority earned only when every admitted component passes all reprojection gates.
pub const REPROJECTION_AUTHORITY: &str = "audited_relative_reprojection_baseline";
/// Authority emitted by blocked, diagnostic, or mixed generations.
pub const REPROJECTION_EVIDENCE_AUTHORITY: &str = "reprojection_evidence_only";
/// Fixed parts-per-million denominator.
pub const PPM_SCALE: u64 = 1_000_000;
/// Maximum calibration rows in one reference generation.
pub const MAX_REPROJECTION_CALIBRATIONS: usize = 100_000;
/// Maximum observation evidence rows in one reference generation.
pub const MAX_REPROJECTION_OBSERVATIONS: usize = 500_000;
/// Maximum absolute residual admitted by policy.
pub const MAX_REPROJECTION_RESIDUAL_NANO_PIXELS: u64 = 9_000_000_000_000_000;
/// Maximum uncertainty-normalized residual admitted by policy.
pub const MAX_REPROJECTION_NORMALIZED_RESIDUAL_PPM: u64 = 1_000_000_000;
/// Maximum deterministic operation count.
pub const MAX_REPROJECTION_OPERATIONS: u64 = 1_000_000_000;
/// Largest public integer emitted as an unquoted JSON number.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one reprojection evaluation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReprojectionBasis {
    /// Exact audited bundle-admission generation.
    pub bundle_admission_digest: EvidenceDigest,
    /// Exact canonical effective-calibration table.
    pub calibration_table_digest: EvidenceDigest,
    /// Exact scalar algorithm identity.
    pub algorithm_digest: EvidenceDigest,
    /// Exact semantic reprojection-policy identity.
    pub policy_digest: EvidenceDigest,
    /// Immutable generation.
    pub generation: u64,
}

/// Semantic reprojection gates plus one nonsemantic operation ceiling.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ReprojectionPolicy {
    /// Exact calibrated projection bounds.
    pub projection: CalibratedProjectionPolicy,
    /// Maximum Euclidean image residual.
    pub max_residual_nano_pixels: u64,
    /// Maximum residual divided by the larger declared observation/calibration uncertainty.
    pub max_normalized_residual_ppm: u64,
    /// Minimum projectable optimize observations required per admitted component.
    pub min_optimize_observations_per_component: u32,
    /// Minimum projectable independent held-out observations required per admitted component.
    pub min_held_out_observations_per_component: u32,
    /// Hard deterministic operation ceiling, excluded from successful semantic identity.
    pub max_operations: u64,
}

impl Default for ReprojectionPolicy {
    fn default() -> Self {
        Self {
            projection: CalibratedProjectionPolicy::default(),
            max_residual_nano_pixels: 2_000_000_000,
            max_normalized_residual_ppm: 4_000_000,
            min_optimize_observations_per_component: 4,
            min_held_out_observations_per_component: 1,
            max_operations: 100_000_000,
        }
    }
}

/// Exact effective calibration bound to one audited camera.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReprojectionCalibration {
    /// Stable pose-graph camera node.
    pub camera_node_id: u64,
    /// Exact decoded-frame identity.
    pub frame_digest: EvidenceDigest,
    /// Exact effective derived-calibration identity.
    pub effective_calibration_digest: EvidenceDigest,
    /// Fully materialized effective calibration.
    pub calibration: DerivedCalibration,
}
