/// Public schema identity for one deterministic correspondence generation.
pub const CORRESPONDENCE_GENERATION_SCHEMA: &str = "fdgr.correspondence_generation/1";
/// Fixed denominator for response and ratio values.
pub const PPM_SCALE: u32 = 1_000_000;
/// Descriptor width in bits.
pub const DESCRIPTOR_BITS: u16 = 256;
/// Descriptor width in machine words.
pub const DESCRIPTOR_WORDS: usize = 4;
/// Maximum feature observations in one reference generation.
pub const MAX_FEATURE_OBSERVATIONS: usize = 200_000;
/// Maximum explicit frame-pair requests.
pub const MAX_FRAME_PAIRS: usize = 4_096;
/// Maximum observations belonging to one frame.
pub const MAX_FEATURES_PER_FRAME: usize = 50_000;
/// Maximum accepted pairwise edges retained by one generation.
pub const MAX_MATCH_EDGES: usize = 2_000_000;
/// Maximum operation budget for descriptor-distance evaluations.
pub const MAX_DISTANCE_EVALUATIONS: u64 = 1_000_000_000;
/// Maximum absolute fixed-point pixel coordinate.
pub const MAX_ABS_NANO_PIXEL: i64 = 1_000_000_000_000_000;
/// Maximum declared localization uncertainty.
pub const MAX_UNCERTAINTY_NANO_PIXELS: u64 = 1_000_000_000_000;
/// Largest integer identity emitted as a JSON number without IEEE-754 loss.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one correspondence generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CorrespondenceBasis {
    /// Exact keyframe-selection generation.
    pub keyframe_selection_digest: EvidenceDigest,
    /// Exact calibration or derived-calibration generation.
    pub calibration_digest: EvidenceDigest,
    /// Exact source feature-table identity.
    pub feature_basis_digest: EvidenceDigest,
    /// Exact requested frame-pair table identity.
    pub pair_basis_digest: EvidenceDigest,
    /// Exact matcher-policy registry identity.
    pub policy_digest: EvidenceDigest,
    /// Immutable correspondence generation.
    pub generation: u64,
}

/// Canonical 256-bit binary descriptor.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Descriptor256(
    /// Four little-indexed words whose bit ordering is part of the schema.
    pub [u64; DESCRIPTOR_WORDS],
);

/// One exact feature observation in a calibrated keyframe.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FeatureObservation {
    /// Stable nonzero observation identity.
    pub observation_id: u64,
    /// Timeline sample index.
    pub sample_index: u64,
    /// Exact decoded-frame object identity.
    pub frame_digest: EvidenceDigest,
    /// Stable nonzero feature identity within the frame.
    pub feature_id: u32,
    /// Horizontal location in nano-pixels.
    pub x_nano_pixels: i64,
    /// Vertical location in nano-pixels.
    pub y_nano_pixels: i64,
    /// Detector response in parts per million.
    pub response_ppm: u32,
    /// Symmetric localization uncertainty in nano-pixels.
    pub uncertainty_nano_pixels: u64,
    /// Exact binary descriptor.
    pub descriptor: Descriptor256,
    /// Whether an admitted dynamic mask covers this observation.
    pub dynamic_masked: bool,
}

/// Explicit frame pair to match. Canonical pairs require left sample index less than right.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FramePair {
    /// Stable nonzero pair identity.
    pub pair_id: u64,
    /// Earlier/canonical left sample index.
    pub left_sample_index: u64,
    /// Later/canonical right sample index.
    pub right_sample_index: u64,
}

/// Deterministic bounded descriptor-matching policy.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct MatchPolicy {
    /// Maximum accepted Hamming distance.
    pub max_hamming_distance: u16,
    /// Maximum best/second-best ratio in parts per million.
    pub ratio_threshold_ppm: u32,
    /// Whether an accepted proposal must have an observed second-best candidate.
    pub require_second_best: bool,
    /// Whether the right feature must choose the left feature as its unique nearest neighbor.
    pub require_mutual: bool,
    /// Minimum detector response.
    pub min_response_ppm: u32,
    /// Maximum localization uncertainty.
    pub max_uncertainty_nano_pixels: u64,
    /// Whether dynamic-masked observations are excluded.
    pub reject_dynamic_masked: bool,
    /// Hard descriptor-distance operation budget. This limits execution but is excluded from the
    /// semantic generation digest after successful completion.
    pub max_distance_evaluations: u64,
}

impl Default for MatchPolicy {
    fn default() -> Self {
        Self {
            max_hamming_distance: 96,
            ratio_threshold_ppm: 800_000,
            require_second_best: true,
            require_mutual: true,
            min_response_ppm: 100_000,
            max_uncertainty_nano_pixels: 2_000_000_000,
            reject_dynamic_masked: true,
            max_distance_evaluations: 10_000_000,
        }
    }
}

/// Stable reason a proposed correspondence did not survive.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MatchRejectionReason {
    /// Left observation response was below the policy minimum.
    ResponseBelowMinimum,
    /// Left observation uncertainty exceeded the policy maximum.
    UncertaintyExceeded,
    /// Left observation was covered by a dynamic mask.
    DynamicMasked,
    /// No eligible observation existed in the right frame.
    NoEligibleRight,
    /// More than one right observation shared the best distance.
    NearestTie,
    /// No second-best observation existed to establish ambiguity margin.
    SecondBestUnavailable,
    /// Unique best distance exceeded the policy maximum.
    DistanceExceeded,
    /// Best/second-best ratio failed the policy threshold.
    RatioFailed,
    /// The proposed right observation did not uniquely select the left in reverse.
    MutualFailed,
    /// Merging the edge would place two observations from one frame in a track.
    FrameCollision,
}

impl MatchRejectionReason {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ResponseBelowMinimum => "response_below_minimum",
            Self::UncertaintyExceeded => "uncertainty_exceeded",
            Self::DynamicMasked => "dynamic_masked",
            Self::NoEligibleRight => "no_eligible_right",
            Self::NearestTie => "nearest_tie",
            Self::SecondBestUnavailable => "second_best_unavailable",
            Self::DistanceExceeded => "distance_exceeded",
            Self::RatioFailed => "ratio_failed",
            Self::MutualFailed => "mutual_failed",
            Self::FrameCollision => "frame_collision",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::ResponseBelowMinimum => 1,
            Self::UncertaintyExceeded => 2,
            Self::DynamicMasked => 3,
            Self::NoEligibleRight => 4,
            Self::NearestTie => 5,
            Self::SecondBestUnavailable => 6,
            Self::DistanceExceeded => 7,
            Self::RatioFailed => 8,
            Self::MutualFailed => 9,
            Self::FrameCollision => 10,
        }
    }
}

/// One explicit rejected left-to-right proposal.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MatchRejection {
    /// Frame-pair identity.
    pub pair_id: u64,
    /// Left observation identity.
    pub left_observation_id: u64,
    /// Best right candidate when one existed.
    pub candidate_right_observation_id: Option<u64>,
    /// Stable rejection reason.
    pub reason: MatchRejectionReason,
    /// Best Hamming distance when one existed.
    pub best_distance: Option<u16>,
    /// Second-best Hamming distance when one existed.
    pub second_best_distance: Option<u16>,
}

/// One descriptor edge admitted into the collision-safe track graph.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PairwiseMatch {
    /// Frame-pair identity.
    pub pair_id: u64,
    /// Left observation identity.
    pub left_observation_id: u64,
    /// Right observation identity.
    pub right_observation_id: u64,
    /// Exact Hamming distance.
    pub hamming_distance: u16,
    /// Second-best left-to-right distance, if available.
    pub second_best_distance: Option<u16>,
    /// Rounded-up best/second ratio in parts per million, if available.
    pub ratio_ppm: Option<u32>,
    /// Whether mutual-nearest evidence was required and satisfied.
    pub mutual: bool,
    /// Whether this edge closed a cycle inside an already valid track.
    pub creates_cycle: bool,
}

/// One deterministic multi-view feature track.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FeatureTrack {
    /// Stable generation-local track identity.
    pub track_id: u64,
    /// Sorted observation identities.
    pub observation_ids: Vec<u64>,
    /// Sorted sample indices, exactly one per observation.
    pub sample_indices: Vec<u64>,
    /// Sorted pair identities supporting the track.
    pub supporting_pair_ids: Vec<u64>,
    /// Number of accepted descriptor edges inside the track.
    pub edge_count: u32,
}

/// Immutable deterministic correspondence and track result.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CorrespondenceGeneration {
    /// Exact immutable basis.
    pub basis: CorrespondenceBasis,
    /// Exact matching policy.
    pub policy: MatchPolicy,
    /// Canonically sorted input observations retained for replay.
    pub observations: Vec<FeatureObservation>,
    /// Canonically sorted requested frame pairs.
    pub pairs: Vec<FramePair>,
    /// Accepted descriptor edges in deterministic processing order.
    pub accepted_matches: Vec<PairwiseMatch>,
    /// Rejected proposals in stable order.
    pub rejected_matches: Vec<MatchRejection>,
    /// Multi-view tracks in canonical order.
    pub tracks: Vec<FeatureTrack>,
    /// Sorted observations excluded by response, uncertainty, or dynamic-mask policy.
    pub ineligible_observation_ids: Vec<u64>,
    /// Sorted observations not belonging to a multi-view track.
    pub unmatched_observation_ids: Vec<u64>,
    /// Exact number of descriptor-distance evaluations charged. This is operation evidence and is
    /// excluded from the semantic generation digest.
    pub distance_evaluations: u64,
}
