/// Public schema identity for one relative pose-edge scale generation.
pub const EDGE_SCALE_GENERATION_SCHEMA: &str = "fdgr.edge_scale_generation/1";
/// Fixed component-gauge scale. `1_000_000_000` is one arbitrary edge-scale unit.
pub const UNIT_NANO: u64 = 1_000_000_000;
/// Parts-per-million denominator.
pub const PPM_SCALE: u64 = 1_000_000;
/// Maximum pose edges in one reference generation.
pub const MAX_SCALE_EDGES: usize = 100_000;
/// Maximum raw witnesses in one reference generation.
pub const MAX_SCALE_WITNESSES: usize = 500_000;
/// Maximum numerator or denominator of one exact ratio witness after canonicalization.
pub const MAX_RATIO_TERM: u64 = 1_000_000_000_000;
/// Maximum component-relative scale emitted by the scalar reference.
pub const MAX_RELATIVE_SCALE_NANO: u64 = 1_000_000_000_000_000;
/// Maximum residual gate.
pub const MAX_SCALE_RESIDUAL_PPM: u64 = 1_000_000;
/// Largest public identity emitted as a JSON number.
pub const MAX_SAFE_JSON_INTEGER: u64 = 9_007_199_254_740_991;

/// Exact immutable basis for one edge-scale generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EdgeScaleBasis {
    /// Exact pose-graph generation whose edges are being scaled.
    pub pose_graph_generation_digest: EvidenceDigest,
    /// Exact edge-subject table identity.
    pub edge_subject_basis_digest: EvidenceDigest,
    /// Exact raw witness table identity.
    pub witness_basis_digest: EvidenceDigest,
    /// Exact aggregation and cycle policy identity.
    pub policy_digest: EvidenceDigest,
    /// Immutable generation.
    pub generation: u64,
}

/// One pose-graph edge whose magnitude remains unknown.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EdgeScaleSubject {
    /// Stable pose-graph edge identity.
    pub edge_id: u64,
    /// Exact pairwise relative-pose verification identity.
    pub pose_verification_digest: EvidenceDigest,
    /// Pose-component root containing this edge.
    pub pose_component_root_node_id: u64,
}

/// Provenance class of one relative edge-ratio witness.
#[derive(Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub enum EdgeRatioSource {
    /// Shared multi-view tracks seen by both pair-local reconstructions.
    SharedTrackGeometry,
    /// A jointly optimized multi-view geometric proposal.
    MultiViewGeometry,
    /// Clock-aligned device telemetry baseline ratio.
    TelemetryBaseline,
    /// Isolated external reference oracle.
    ExternalOracle,
    /// Learned or heuristic prior.
    ModelPrior,
}

impl EdgeRatioSource {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::SharedTrackGeometry => "shared_track_geometry",
            Self::MultiViewGeometry => "multi_view_geometry",
            Self::TelemetryBaseline => "telemetry_baseline",
            Self::ExternalOracle => "external_oracle",
            Self::ModelPrior => "model_prior",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::SharedTrackGeometry => 1,
            Self::MultiViewGeometry => 2,
            Self::TelemetryBaseline => 3,
            Self::ExternalOracle => 4,
            Self::ModelPrior => 5,
        }
    }

    pub(crate) const fn rank(self) -> u64 {
        match self {
            Self::SharedTrackGeometry => 5,
            Self::MultiViewGeometry => 4,
            Self::TelemetryBaseline => 3,
            Self::ExternalOracle => 2,
            Self::ModelPrior => 1,
        }
    }
}

/// One exact ratio witness. Canonical form requires `lower_edge_id < higher_edge_id` and records
/// `length(lower) / length(higher) = ratio_numerator / ratio_denominator`.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EdgeRatioWitness {
    /// Stable witness identity.
    pub witness_id: u64,
    /// Exact evidence object supporting this witness.
    pub evidence_digest: EvidenceDigest,
    /// Dependence class. Multiple observations in one class count as one independent vote.
    pub correlation_group_id: u64,
    /// Canonical lower pose-edge identity.
    pub lower_edge_id: u64,
    /// Canonical higher pose-edge identity.
    pub higher_edge_id: u64,
    /// Positive exact ratio numerator.
    pub ratio_numerator: u64,
    /// Positive exact ratio denominator.
    pub ratio_denominator: u64,
    /// Declared symmetric uncertainty in parts per million.
    pub uncertainty_ppm: u32,
    /// Positive support count used only after independence collapse.
    pub support_count: u32,
    /// Provenance class.
    pub source: EdgeRatioSource,
}

/// Semantic aggregation gates plus one nonsemantic graph-work ceiling.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct EdgeScalePolicy {
    /// Maximum disagreement inside one declared correlation group.
    pub max_within_group_residual_ppm: u64,
    /// Maximum disagreement for independent groups supporting one edge pair.
    pub max_consensus_residual_ppm: u64,
    /// Maximum redundant relation residual after forest propagation.
    pub max_cycle_residual_ppm: u64,
    /// Minimum independent groups required to call a component cross-validated.
    pub min_cross_validation_groups: u16,
    /// Maximum relative scale emitted in one arbitrary component gauge.
    pub max_relative_scale_nano: u64,
    /// Maximum graph path expansions. Operation evidence only.
    pub max_path_expansions: u64,
}

impl Default for EdgeScalePolicy {
    fn default() -> Self {
        Self {
            max_within_group_residual_ppm: 20_000,
            max_consensus_residual_ppm: 50_000,
            max_cycle_residual_ppm: 75_000,
            min_cross_validation_groups: 2,
            max_relative_scale_nano: MAX_RELATIVE_SCALE_NANO,
            max_path_expansions: 10_000_000,
        }
    }
}

/// Disposition of one raw witness after dependence collapse and robust consensus.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum WitnessDisposition {
    /// Representative or agreeing evidence supporting an admitted relation.
    Supporting,
    /// Agreeing evidence from a group already represented by another witness.
    CorrelatedDuplicate,
    /// A declared correlation group contradicted itself and was excluded as a whole.
    CorrelatedConflict,
    /// An independent group disagreed with the selected consensus.
    ConsensusOutlier,
    /// Independent consensus modes tied without an admissible preference.
    ConsensusAmbiguous,
    /// The witness related edges from disconnected pose components and was not used.
    CrossPoseComponent,
}

impl WitnessDisposition {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Supporting => "supporting",
            Self::CorrelatedDuplicate => "correlated_duplicate",
            Self::CorrelatedConflict => "correlated_conflict",
            Self::ConsensusOutlier => "consensus_outlier",
            Self::ConsensusAmbiguous => "consensus_ambiguous",
            Self::CrossPoseComponent => "cross_pose_component",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::Supporting => 1,
            Self::CorrelatedDuplicate => 2,
            Self::CorrelatedConflict => 3,
            Self::ConsensusOutlier => 4,
            Self::ConsensusAmbiguous => 5,
            Self::CrossPoseComponent => 6,
        }
    }
}

/// Retained adjudication of one raw witness.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct WitnessAssessment {
    /// Raw witness identity.
    pub witness_id: u64,
    /// Admitted aggregate relation, when one exists.
    pub relation_id: Option<u64>,
    /// Final disposition.
    pub disposition: WitnessDisposition,
    /// Residual to the selected within-group or cross-group reference, when defined.
    pub residual_ppm: Option<u64>,
}

/// One aggregate independent-consensus relation between two pose-edge magnitudes.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EdgeScaleRelation {
    /// Stable generation-local relation identity.
    pub relation_id: u64,
    /// Canonical lower pose-edge identity.
    pub lower_edge_id: u64,
    /// Canonical higher pose-edge identity.
    pub higher_edge_id: u64,
    /// `length(lower) / length(higher)` multiplied by [`UNIT_NANO`].
    pub ratio_lower_over_higher_nano: u64,
    /// Canonically sorted supporting raw witness identities.
    pub supporting_witness_ids: Vec<u64>,
    /// Canonically sorted independent supporting correlation groups.
    pub supporting_correlation_group_ids: Vec<u64>,
    /// Canonically sorted excluded outlier witness identities for this pair.
    pub rejected_witness_ids: Vec<u64>,
    /// Conservative maximum uncertainty among admitted group representatives.
    pub uncertainty_ppm: u32,
    /// Strongest admitted provenance rank.
    pub evidence_rank: u64,
    /// Saturating aggregate support after one representative per group.
    pub independent_support_count: u64,
}

/// Cycle role and residual of one admitted aggregate relation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RelationStatus {
    /// Relation belongs to the deterministic scale forest.
    Forest,
    /// Redundant relation agrees with forest-propagated scales.
    Consistent,
    /// Redundant relation conflicts with forest-propagated scales.
    Conflicting,
}

impl RelationStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Forest => "forest",
            Self::Consistent => "consistent",
            Self::Conflicting => "conflicting",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::Forest => 1,
            Self::Consistent => 2,
            Self::Conflicting => 3,
        }
    }
}

/// Assessment of one aggregate scale relation after gauge propagation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RelationAssessment {
    /// Aggregate relation identity.
    pub relation_id: u64,
    /// Ratio implied by propagated endpoint scales.
    pub implied_ratio_nano: u64,
    /// Symmetric relative residual in parts per million.
    pub residual_ppm: u64,
    /// Forest/cycle disposition.
    pub status: RelationStatus,
}

