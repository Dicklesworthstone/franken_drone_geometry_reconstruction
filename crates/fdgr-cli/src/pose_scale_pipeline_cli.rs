#![forbid(unsafe_code)]
//! Shared exact-byte construction for pose-graph and relative edge-scale CLI operations.

use crate::geometry_observation_cli::read_bound_text;
use crate::pose_graph_input_cli::{
    PoseGraphFileBasis, build_pose_graph_from_files, parse_digest_option,
    parse_nonzero_u16_option, parse_nonzero_u32_option, parse_nonzero_u64_option,
    parse_u64_option,
};
use fdgr_edge_scale::{
    EdgeRatioSource, EdgeRatioWitness, EdgeScaleBasis, EdgeScaleGeneration, EdgeScalePolicy,
    derive_edge_scale_subjects, edge_witness_table_digest, reconcile_pose_graph_edge_scales,
};
use fdgr_pose_graph::{PoseGraphGeneration, PoseGraphPolicy};
use fdgr_types::EvidenceDigest;
use std::path::{Path, PathBuf};

const WITNESS_HEADER: &str = "witness_id\tevidence_digest\tcorrelation_group_id\tlower_edge_id\thigher_edge_id\tratio_numerator\tratio_denominator\tuncertainty_ppm\tsupport_count\tsource";
const MAX_WITNESS_TABLE_BYTES: u64 = 128 * 1024 * 1024;
const MAX_WITNESSES: usize = 500_000;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct EdgeScaleFileBasis {
    pub(crate) witness_path: PathBuf,
    pub(crate) witness_file_digest: EvidenceDigest,
    pub(crate) policy_digest: EvidenceDigest,
    pub(crate) generation: u64,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct PoseScalePipelineOptions {
    pub(crate) pose_basis: PoseGraphFileBasis,
    pub(crate) pose_policy: PoseGraphPolicy,
    pub(crate) edge_scale_basis: EdgeScaleFileBasis,
    pub(crate) edge_scale_policy: EdgeScalePolicy,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct PoseScalePipelineParser {
    node_path: PathBuf,
    edge_path: PathBuf,
    witness_path: PathBuf,
    node_file_digest: Option<EvidenceDigest>,
    edge_file_digest: Option<EvidenceDigest>,
    witness_file_digest: Option<EvidenceDigest>,
    graph_selection_policy_digest: Option<EvidenceDigest>,
    rotation_policy_digest: Option<EvidenceDigest>,
    pose_graph_generation: Option<u64>,
    edge_scale_policy_digest: Option<EvidenceDigest>,
    edge_scale_generation: Option<u64>,
    pose_policy: PoseGraphPolicy,
    edge_scale_policy: EdgeScalePolicy,
}

impl PoseScalePipelineParser {
    pub(crate) fn new(node_path: &str, edge_path: &str, witness_path: &str) -> Self {
        Self {
            node_path: PathBuf::from(node_path),
            edge_path: PathBuf::from(edge_path),
            witness_path: PathBuf::from(witness_path),
            node_file_digest: None,
            edge_file_digest: None,
            witness_file_digest: None,
            graph_selection_policy_digest: None,
            rotation_policy_digest: None,
            pose_graph_generation: None,
            edge_scale_policy_digest: None,
            edge_scale_generation: None,
            pose_policy: PoseGraphPolicy::default(),
            edge_scale_policy: EdgeScalePolicy::default(),
        }
    }

    pub(crate) fn apply_option(&mut self, flag: &str, value: &str) -> Result<bool, String> {
        match flag {
            "--node-file-digest" => {
                self.node_file_digest = Some(parse_digest_option(value, "node file digest")?);
            }
            "--pose-edge-file-digest" => {
                self.edge_file_digest = Some(parse_digest_option(value, "pose-edge file digest")?);
            }
            "--scale-witness-file-digest" => {
                self.witness_file_digest =
                    Some(parse_digest_option(value, "scale-witness file digest")?);
            }
            "--graph-selection-policy-digest" => {
                self.graph_selection_policy_digest =
                    Some(parse_digest_option(value, "graph selection policy digest")?);
            }
            "--rotation-policy-digest" => {
                self.rotation_policy_digest =
                    Some(parse_digest_option(value, "rotation policy digest")?);
            }
            "--pose-graph-generation" => {
                self.pose_graph_generation =
                    Some(parse_nonzero_u64_option(value, "pose-graph generation")?);
            }
            "--edge-scale-policy-digest" => {
                self.edge_scale_policy_digest =
                    Some(parse_digest_option(value, "edge-scale policy digest")?);
            }
            "--edge-scale-generation" => {
                self.edge_scale_generation =
                    Some(parse_nonzero_u64_option(value, "edge-scale generation")?);
            }
            "--max-rotation-cycle-residual-ppm" => {
                self.pose_policy.max_rotation_cycle_residual_ppm =
                    parse_u64_option(value, "maximum rotation cycle residual ppm")?;
            }
            "--max-orientation-drift-ppm" => {
                self.pose_policy.max_orientation_drift_ppm =
                    parse_u64_option(value, "maximum orientation drift ppm")?;
            }
            "--max-pose-path-expansions" => {
                self.pose_policy.max_path_expansions =
                    parse_nonzero_u64_option(value, "maximum pose path expansions")?;
            }
            "--max-within-group-residual-ppm" => {
                self.edge_scale_policy.max_within_group_residual_ppm =
                    parse_u64_option(value, "maximum within-group residual ppm")?;
            }
            "--max-consensus-residual-ppm" => {
                self.edge_scale_policy.max_consensus_residual_ppm =
                    parse_u64_option(value, "maximum consensus residual ppm")?;
            }
            "--max-scale-cycle-residual-ppm" => {
                self.edge_scale_policy.max_cycle_residual_ppm =
                    parse_u64_option(value, "maximum scale cycle residual ppm")?;
            }
            "--min-cross-validation-groups" => {
                self.edge_scale_policy.min_cross_validation_groups =
                    parse_nonzero_u16_option(value, "minimum cross-validation groups")?;
            }
            "--max-relative-scale-nano" => {
                self.edge_scale_policy.max_relative_scale_nano =
                    parse_nonzero_u64_option(value, "maximum relative scale nano")?;
            }
            "--max-scale-path-expansions" => {
                self.edge_scale_policy.max_path_expansions =
                    parse_nonzero_u64_option(value, "maximum scale path expansions")?;
            }
            _ => return Ok(false),
        }
        Ok(true)
    }

    pub(crate) fn finish(self) -> Result<PoseScalePipelineOptions, String> {
        Ok(PoseScalePipelineOptions {
            pose_basis: PoseGraphFileBasis {
                node_path: self.node_path,
                edge_path: self.edge_path,
                node_file_digest: self
                    .node_file_digest
                    .ok_or_else(|| "missing --node-file-digest".to_owned())?,
                edge_file_digest: self
                    .edge_file_digest
                    .ok_or_else(|| "missing --pose-edge-file-digest".to_owned())?,
                graph_selection_policy_digest: self.graph_selection_policy_digest.ok_or_else(
                    || "missing --graph-selection-policy-digest".to_owned(),
                )?,
                rotation_policy_digest: self
                    .rotation_policy_digest
                    .ok_or_else(|| "missing --rotation-policy-digest".to_owned())?,
                generation: self
                    .pose_graph_generation
                    .ok_or_else(|| "missing --pose-graph-generation".to_owned())?,
            },
            pose_policy: self.pose_policy,
            edge_scale_basis: EdgeScaleFileBasis {
                witness_path: self.witness_path,
                witness_file_digest: self
                    .witness_file_digest
                    .ok_or_else(|| "missing --scale-witness-file-digest".to_owned())?,
                policy_digest: self
                    .edge_scale_policy_digest
                    .ok_or_else(|| "missing --edge-scale-policy-digest".to_owned())?,
                generation: self
                    .edge_scale_generation
                    .ok_or_else(|| "missing --edge-scale-generation".to_owned())?,
            },
            edge_scale_policy: self.edge_scale_policy,
        })
    }
}

pub(crate) fn build_pose_and_edge_scale(
    options: &PoseScalePipelineOptions,
) -> Result<(PoseGraphGeneration, EdgeScaleGeneration), String> {
    let pose_graph = build_pose_graph_from_files(&options.pose_basis, options.pose_policy)?;
    let pose_graph_digest = pose_graph
        .digest()
        .map_err(|error| format!("pose-graph identity failed: {error}"))?;
    let (subject_digest, subjects) = derive_edge_scale_subjects(&pose_graph)
        .map_err(|error| format!("edge-scale subject derivation failed: {error}"))?;
    let witnesses = read_witnesses(
        &options.edge_scale_basis.witness_path,
        &options.edge_scale_basis.witness_file_digest,
    )?;
    let witness_digest = edge_witness_table_digest(&subjects, &witnesses)
        .map_err(|error| format!("edge-scale witness identity failed: {error}"))?;
    let edge_scale = reconcile_pose_graph_edge_scales(
        EdgeScaleBasis {
            pose_graph_generation_digest: pose_graph_digest,
            edge_subject_basis_digest: subject_digest,
            witness_basis_digest: witness_digest,
            policy_digest: options.edge_scale_basis.policy_digest.clone(),
            generation: options.edge_scale_basis.generation,
        },
        options.edge_scale_policy,
        &pose_graph,
        witnesses,
    )
    .map_err(|error| format!("edge-scale reconciliation rejected: {error}"))?;
    Ok((pose_graph, edge_scale))
}

fn read_witnesses(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<EdgeRatioWitness>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_WITNESS_TABLE_BYTES,
        "edge-scale witness",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "edge-scale witness table is empty".to_owned())?;
    if header != WITNESS_HEADER {
        return Err(format!(
            "edge-scale witness header mismatch: expected {WITNESS_HEADER:?}"
        ));
    }
    let mut witnesses = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("edge-scale witness line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [
            witness_id,
            evidence_digest,
            correlation_group_id,
            lower_edge_id,
            higher_edge_id,
            ratio_numerator,
            ratio_denominator,
            uncertainty_ppm,
            support_count,
            source,
        ] = fields.as_slice()
        else {
            return Err(format!(
                "edge-scale witness line {line_number} must contain exactly ten tab-separated fields"
            ));
        };
        witnesses.push(EdgeRatioWitness {
            witness_id: parse_line_nonzero_u64(witness_id, "witness id", line_number)?,
            evidence_digest: parse_line_digest(evidence_digest, "evidence digest", line_number)?,
            correlation_group_id: parse_line_nonzero_u64(
                correlation_group_id,
                "correlation group id",
                line_number,
            )?,
            lower_edge_id: parse_line_nonzero_u64(lower_edge_id, "lower edge id", line_number)?,
            higher_edge_id: parse_line_nonzero_u64(
                higher_edge_id,
                "higher edge id",
                line_number,
            )?,
            ratio_numerator: parse_line_nonzero_u64(
                ratio_numerator,
                "ratio numerator",
                line_number,
            )?,
            ratio_denominator: parse_line_nonzero_u64(
                ratio_denominator,
                "ratio denominator",
                line_number,
            )?,
            uncertainty_ppm: parse_line_u32(
                uncertainty_ppm,
                "uncertainty ppm",
                line_number,
            )?,
            support_count: parse_line_nonzero_u32(
                support_count,
                "support count",
                line_number,
            )?,
            source: parse_source(source, line_number)?,
        });
        if witnesses.len() > MAX_WITNESSES {
            return Err(format!(
                "edge-scale witness table contains more than {MAX_WITNESSES} records"
            ));
        }
    }
    Ok(witnesses)
}

fn parse_source(value: &str, line: usize) -> Result<EdgeRatioSource, String> {
    match value {
        "shared_track_geometry" => Ok(EdgeRatioSource::SharedTrackGeometry),
        "multi_view_geometry" => Ok(EdgeRatioSource::MultiViewGeometry),
        "telemetry_baseline" => Ok(EdgeRatioSource::TelemetryBaseline),
        "external_oracle" => Ok(EdgeRatioSource::ExternalOracle),
        "model_prior" => Ok(EdgeRatioSource::ModelPrior),
        _ => Err(format!(
            "line {line}: unknown edge-scale source {value:?}; expected shared_track_geometry, multi_view_geometry, telemetry_baseline, external_oracle, or model_prior"
        )),
    }
}

fn parse_line_digest(value: &str, label: &str, line: usize) -> Result<EvidenceDigest, String> {
    parse_digest_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_line_nonzero_u64(value: &str, label: &str, line: usize) -> Result<u64, String> {
    parse_nonzero_u64_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_line_nonzero_u32(value: &str, label: &str, line: usize) -> Result<u32, String> {
    parse_nonzero_u32_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_line_u32(value: &str, label: &str, line: usize) -> Result<u32, String> {
    value
        .parse::<u32>()
        .map_err(|error| format!("line {line}: invalid {label} {value:?}: {error}"))
}
