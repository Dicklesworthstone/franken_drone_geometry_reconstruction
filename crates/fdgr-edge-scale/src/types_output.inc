/// One pose-edge magnitude in an arbitrary connected-component gauge.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReconciledEdgeScale {
    /// Pose-graph edge identity.
    pub edge_id: u64,
    /// Pose component containing the edge.
    pub pose_component_root_node_id: u64,
    /// Canonical edge-scale component root.
    pub scale_component_root_edge_id: u64,
    /// Relative magnitude multiplied by [`UNIT_NANO`].
    pub relative_scale_nano: u64,
    /// Parent pose-edge variable in the ratio forest, absent for the root.
    pub parent_edge_id: Option<u64>,
    /// Parent aggregate relation, absent for the root.
    pub parent_relation_id: Option<u64>,
}

/// Maturity of one connected edge-scale component.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum EdgeScaleComponentStatus {
    /// One pose edge has no admitted ratio relation.
    Isolated,
    /// Ratios connect the component but no redundant cycle is observed.
    TreeOnly,
    /// Every observed redundant relation agrees with the forest solution.
    CycleConsistent,
    /// At least one redundant relation conflicts.
    Conflicted,
}

impl EdgeScaleComponentStatus {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Isolated => "isolated",
            Self::TreeOnly => "tree_only",
            Self::CycleConsistent => "cycle_consistent",
            Self::Conflicted => "conflicted",
        }
    }

    pub(crate) const fn code(self) -> u8 {
        match self {
            Self::Isolated => 1,
            Self::TreeOnly => 2,
            Self::CycleConsistent => 3,
            Self::Conflicted => 4,
        }
    }
}

/// One connected edge-scale component.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EdgeScaleComponent {
    /// Canonical root pose-edge identity and arbitrary gauge anchor.
    pub scale_component_root_edge_id: u64,
    /// Pose component containing every member edge.
    pub pose_component_root_node_id: u64,
    /// Sorted member pose-edge identities.
    pub edge_ids: Vec<u64>,
    /// Sorted admitted aggregate relation identities.
    pub relation_ids: Vec<u64>,
    /// Sorted scale-forest relation identities.
    pub forest_relation_ids: Vec<u64>,
    /// Sorted independent supporting correlation groups.
    pub independent_correlation_group_ids: Vec<u64>,
    /// Component maturity.
    pub status: EdgeScaleComponentStatus,
    /// Whether independent evidence meets the configured cross-validation count.
    pub cross_validated: bool,
    /// Sorted conflicting redundant relations.
    pub conflicting_relation_ids: Vec<u64>,
}

/// Immutable correlation-aware relative edge-scale generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EdgeScaleGeneration {
    /// Exact immutable basis.
    pub basis: EdgeScaleBasis,
    /// Exact instantiated policy.
    pub policy: EdgeScalePolicy,
    /// Canonically sorted pose-edge subjects.
    pub subjects: Vec<EdgeScaleSubject>,
    /// Canonically sorted and fraction-normalized raw witnesses.
    pub witnesses: Vec<EdgeRatioWitness>,
    /// One retained assessment per raw witness.
    pub witness_assessments: Vec<WitnessAssessment>,
    /// Admitted pairwise aggregate relations.
    pub relations: Vec<EdgeScaleRelation>,
    /// Deterministic graph topology over edge variables and aggregate relations.
    pub topology: GraphAnalysis,
    /// Reconciled scales in canonical pose-edge order.
    pub scales: Vec<ReconciledEdgeScale>,
    /// Forest and redundant-cycle relation assessments.
    pub relation_assessments: Vec<RelationAssessment>,
    /// Connected edge-scale components.
    pub components: Vec<EdgeScaleComponent>,
}

