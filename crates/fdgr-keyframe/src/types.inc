/// Public schema identity for one immutable keyframe selection.
pub const KEYFRAME_SELECTION_SCHEMA: &str = "fdgr.keyframe_selection/1";
/// Fixed denominator for quality and fraction metrics.
pub const PPM_SCALE: u32 = 1_000_000;
/// Maximum candidates admitted by one deterministic selection.
pub const MAX_KEYFRAME_CANDIDATES: usize = 100_000;
/// Maximum selected keyframes in one selection.
pub const MAX_SELECTED_KEYFRAMES: u32 = 16_384;
/// Maximum visibility cells attached to one candidate.
pub const MAX_COVERAGE_CELLS_PER_CANDIDATE: usize = 4_096;
/// Maximum total candidate-to-cell references in one selection.
pub const MAX_TOTAL_COVERAGE_REFERENCES: usize = 2_000_000;
/// Maximum absolute presentation tick admitted by the reference implementation.
pub const MAX_ABS_PRESENTATION_TICK: i128 = 1_000_000_000_000_000_000;
/// Maximum scoring weight.
pub const MAX_SELECTION_WEIGHT: u32 = 1_000_000;

/// Exact immutable basis for one frame-quality and keyframe-selection generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct KeyframeBasis {
    /// Canonical encoded-sample timeline identity.
    pub timeline_digest: EvidenceDigest,
    /// Exact decoded-frame generation supplying the candidate images.
    pub decoded_frame_generation_digest: EvidenceDigest,
    /// Exact calibration or derived-calibration identity applied to the candidates.
    pub calibration_digest: EvidenceDigest,
    /// Exact content identity of the candidate metric/visibility table.
    pub candidate_basis_digest: EvidenceDigest,
    /// Exact selection-policy identity.
    pub policy_digest: EvidenceDigest,
    /// Immutable selection generation.
    pub selection_generation: u64,
}

/// Fixed-point quality and visibility evidence for one decoded frame.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct KeyframeCandidate {
    /// Sample index in the bound media timeline.
    pub sample_index: u64,
    /// Exact decoded-frame object identity.
    pub frame_digest: EvidenceDigest,
    /// Presentation tick in the timeline's native timescale.
    pub presentation_tick: i128,
    /// Sharpness score in parts per million, where larger is better.
    pub sharpness_ppm: u32,
    /// Texture score in parts per million, where larger is better.
    pub texture_ppm: u32,
    /// Dark-clipped fraction in parts per million.
    pub dark_clipped_ppm: u32,
    /// Bright-clipped fraction in parts per million.
    pub bright_clipped_ppm: u32,
    /// Dynamic-content fraction in parts per million.
    pub dynamic_content_ppm: u32,
    /// Geometric overlap with the local predecessor/support neighborhood in parts per million.
    pub overlap_ppm: u32,
    /// Nonzero stable view-sector identity.
    pub view_sector: u32,
    /// Nonzero stable baseline-bin identity.
    pub baseline_bin: u32,
    /// Strictly increasing nonzero visibility-cell identities.
    pub coverage_cells: Vec<u64>,
}

/// Deterministic frame-quality and selection policy.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct KeyframePolicy {
    /// Maximum frames selected by this generation.
    pub max_selected: u32,
    /// Minimum admitted sharpness.
    pub min_sharpness_ppm: u32,
    /// Minimum admitted texture.
    pub min_texture_ppm: u32,
    /// Maximum admitted dark-clipped fraction.
    pub max_dark_clipped_ppm: u32,
    /// Maximum admitted bright-clipped fraction.
    pub max_bright_clipped_ppm: u32,
    /// Maximum admitted dynamic-content fraction.
    pub max_dynamic_content_ppm: u32,
    /// Minimum local overlap required to support later correspondence.
    pub min_overlap_ppm: u32,
    /// Minimum absolute presentation-tick spacing between selected frames.
    pub min_presentation_spacing_ticks: u64,
    /// Weight applied to the aggregate quality score.
    pub quality_weight: u32,
    /// Weight applied to each newly covered cell.
    pub coverage_weight: u32,
    /// Bonus weight for a newly represented view sector.
    pub view_diversity_weight: u32,
    /// Bonus weight for a newly represented baseline bin.
    pub baseline_diversity_weight: u32,
}

impl Default for KeyframePolicy {
    fn default() -> Self {
        Self {
            max_selected: 256,
            min_sharpness_ppm: 250_000,
            min_texture_ppm: 150_000,
            max_dark_clipped_ppm: 250_000,
            max_bright_clipped_ppm: 250_000,
            max_dynamic_content_ppm: 500_000,
            min_overlap_ppm: 100_000,
            min_presentation_spacing_ticks: 0,
            quality_weight: 1,
            coverage_weight: 1_000,
            view_diversity_weight: 250_000,
            baseline_diversity_weight: 250_000,
        }
    }
}

/// Stable reason a candidate did not enter the selected set.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum KeyframeRejectionReason {
    /// Sharpness was below the policy minimum.
    SharpnessBelowMinimum,
    /// Texture was below the policy minimum.
    TextureBelowMinimum,
    /// Dark clipping exceeded the policy maximum.
    DarkClippingExceeded,
    /// Bright clipping exceeded the policy maximum.
    BrightClippingExceeded,
    /// Dynamic content exceeded the policy maximum.
    DynamicContentExceeded,
    /// Local overlap was below the policy minimum.
    OverlapBelowMinimum,
    /// Candidate carried no visibility-cell evidence.
    EmptyCoverage,
    /// Candidate was too close in presentation time to a selected frame.
    TemporalSpacing,
    /// Candidate added no coverage, view sector, or baseline-bin evidence.
    RedundantEvidence,
    /// Candidate remained useful but the configured selection capacity was exhausted.
    Capacity,
}

impl KeyframeRejectionReason {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::SharpnessBelowMinimum => "sharpness_below_minimum",
            Self::TextureBelowMinimum => "texture_below_minimum",
            Self::DarkClippingExceeded => "dark_clipping_exceeded",
            Self::BrightClippingExceeded => "bright_clipping_exceeded",
            Self::DynamicContentExceeded => "dynamic_content_exceeded",
            Self::OverlapBelowMinimum => "overlap_below_minimum",
            Self::EmptyCoverage => "empty_coverage",
            Self::TemporalSpacing => "temporal_spacing",
            Self::RedundantEvidence => "redundant_evidence",
            Self::Capacity => "capacity",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::SharpnessBelowMinimum => 1,
            Self::TextureBelowMinimum => 2,
            Self::DarkClippingExceeded => 3,
            Self::BrightClippingExceeded => 4,
            Self::DynamicContentExceeded => 5,
            Self::OverlapBelowMinimum => 6,
            Self::EmptyCoverage => 7,
            Self::TemporalSpacing => 8,
            Self::RedundantEvidence => 9,
            Self::Capacity => 10,
        }
    }
}

/// One selected frame and its marginal decision evidence at selection time.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SelectedKeyframe {
    /// One-based greedy selection rank.
    pub rank: u32,
    /// Timeline sample index.
    pub sample_index: u64,
    /// Exact decoded-frame identity.
    pub frame_digest: EvidenceDigest,
    /// Exact presentation tick.
    pub presentation_tick: i128,
    /// Aggregate quality score in parts per million.
    pub quality_score_ppm: u32,
    /// Newly covered visibility cells at selection time.
    pub marginal_coverage_cells: u32,
    /// Whether this frame introduced a new view sector.
    pub new_view_sector: bool,
    /// Whether this frame introduced a new baseline bin.
    pub new_baseline_bin: bool,
    /// Deterministic total greedy score, rendered as a decimal string in JSON.
    pub total_score: u128,
}

/// One rejected frame with an explicit stable explanation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RejectedKeyframe {
    /// Timeline sample index.
    pub sample_index: u64,
    /// Exact decoded-frame identity.
    pub frame_digest: EvidenceDigest,
    /// Stable rejection reason.
    pub reason: KeyframeRejectionReason,
    /// Aggregate quality score in parts per million.
    pub quality_score_ppm: u32,
    /// Marginal cells the candidate would add against the final selected set.
    pub marginal_coverage_cells: u32,
    /// Selected sample causing a temporal-spacing conflict, when applicable.
    pub conflicts_with_sample_index: Option<u64>,
}

/// Immutable deterministic keyframe-selection result.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct KeyframeSelection {
    /// Exact selection basis.
    pub basis: KeyframeBasis,
    /// Exact selection policy.
    pub policy: KeyframePolicy,
    /// Canonically sorted original candidates retained for deterministic replay.
    pub candidates: Vec<KeyframeCandidate>,
    /// Selected frames in greedy rank order.
    pub selected: Vec<SelectedKeyframe>,
    /// Rejected frames in sample order.
    pub rejected: Vec<RejectedKeyframe>,
    /// Sorted union of selected visibility cells.
    pub covered_cells: Vec<u64>,
    /// Sorted selected view-sector identities.
    pub selected_view_sectors: Vec<u32>,
    /// Sorted selected baseline-bin identities.
    pub selected_baseline_bins: Vec<u32>,
}
