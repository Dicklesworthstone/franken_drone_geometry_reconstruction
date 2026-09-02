#![forbid(unsafe_code)]
//! Shared exact-byte pose-graph input parsing for public CLI operations.

use crate::geometry_observation_cli::read_bound_text;
use fdgr_pose_graph::{
    PoseGraphBasis, PoseGraphGeneration, PoseGraphPolicy, PoseNode, RelativePoseEdge,
    build_pose_graph,
};
use fdgr_types::EvidenceDigest;
use std::path::Path;

const NODE_HEADER: &str = "node_id\tsample_index\tkeyframe_digest";
const EDGE_HEADER: &str = "edge_id\tverification_digest\tadmitted_candidate_id\tleft_node_id\tright_node_id\tleft_sample_index\tright_sample_index\tr00\tr01\tr02\tr10\tr11\tr12\tr20\tr21\tr22\ttx\tty\ttz\tsupported_match_count\tmedian_residual_nano";
const MAX_NODE_TABLE_BYTES: u64 = 32 * 1024 * 1024;
const MAX_EDGE_TABLE_BYTES: u64 = 128 * 1024 * 1024;
const MAX_POSE_NODES: usize = 100_000;
const MAX_POSE_EDGES: usize = 500_000;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct PoseGraphFileBasis {
    pub(crate) node_path: std::path::PathBuf,
    pub(crate) edge_path: std::path::PathBuf,
    pub(crate) node_file_digest: EvidenceDigest,
    pub(crate) edge_file_digest: EvidenceDigest,
    pub(crate) graph_selection_policy_digest: EvidenceDigest,
    pub(crate) rotation_policy_digest: EvidenceDigest,
    pub(crate) generation: u64,
}

pub(crate) fn build_pose_graph_from_files(
    basis: &PoseGraphFileBasis,
    policy: PoseGraphPolicy,
) -> Result<PoseGraphGeneration, String> {
    let nodes = read_nodes(&basis.node_path, &basis.node_file_digest)?;
    let edges = read_edges(&basis.edge_path, &basis.edge_file_digest)?;
    build_pose_graph(
        PoseGraphBasis {
            node_basis_digest: basis.node_file_digest.clone(),
            edge_basis_digest: basis.edge_file_digest.clone(),
            graph_selection_policy_digest: basis.graph_selection_policy_digest.clone(),
            rotation_policy_digest: basis.rotation_policy_digest.clone(),
            generation: basis.generation,
        },
        policy,
        nodes,
        edges,
    )
    .map_err(|error| format!("pose-graph construction rejected: {error}"))
}

fn read_nodes(path: &Path, expected_digest: &EvidenceDigest) -> Result<Vec<PoseNode>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_NODE_TABLE_BYTES,
        "pose-graph node",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "pose-graph node table is empty".to_owned())?;
    if header != NODE_HEADER {
        return Err(format!(
            "pose-graph node header mismatch: expected {NODE_HEADER:?}"
        ));
    }
    let mut nodes = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("pose-graph node line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [node_id, sample_index, keyframe_digest] = fields.as_slice() else {
            return Err(format!(
                "pose-graph node line {line_number} must contain exactly three tab-separated fields"
            ));
        };
        nodes.push(PoseNode {
            node_id: parse_nonzero_u64(node_id, "node id", line_number)?,
            sample_index: parse_u64(sample_index, "sample index", line_number)?,
            keyframe_digest: parse_digest(keyframe_digest, "keyframe digest", line_number)?,
        });
        if nodes.len() > MAX_POSE_NODES {
            return Err(format!(
                "pose-graph node table contains more than {MAX_POSE_NODES} records"
            ));
        }
    }
    if nodes.is_empty() {
        return Err("pose-graph node table contains no records".to_owned());
    }
    Ok(nodes)
}

fn read_edges(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<RelativePoseEdge>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_EDGE_TABLE_BYTES,
        "pose-graph edge",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "pose-graph edge table is empty".to_owned())?;
    if header != EDGE_HEADER {
        return Err(format!(
            "pose-graph edge header mismatch: expected {EDGE_HEADER:?}"
        ));
    }
    let mut edges = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("pose-graph edge line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [
            edge_id,
            verification_digest,
            admitted_candidate_id,
            left_node_id,
            right_node_id,
            left_sample_index,
            right_sample_index,
            r00,
            r01,
            r02,
            r10,
            r11,
            r12,
            r20,
            r21,
            r22,
            tx,
            ty,
            tz,
            supported_match_count,
            median_residual_nano,
        ] = fields.as_slice()
        else {
            return Err(format!(
                "pose-graph edge line {line_number} must contain exactly twenty-one tab-separated fields"
            ));
        };
        edges.push(RelativePoseEdge {
            edge_id: parse_nonzero_u64(edge_id, "edge id", line_number)?,
            verification_digest: parse_digest(
                verification_digest,
                "verification digest",
                line_number,
            )?,
            admitted_candidate_id: parse_nonzero_u64(
                admitted_candidate_id,
                "admitted candidate id",
                line_number,
            )?,
            left_node_id: parse_nonzero_u64(left_node_id, "left node id", line_number)?,
            right_node_id: parse_nonzero_u64(right_node_id, "right node id", line_number)?,
            left_sample_index: parse_u64(
                left_sample_index,
                "left sample index",
                line_number,
            )?,
            right_sample_index: parse_u64(
                right_sample_index,
                "right sample index",
                line_number,
            )?,
            rotation_right_from_left_nano: [
                parse_i64(r00, "r00", line_number)?,
                parse_i64(r01, "r01", line_number)?,
                parse_i64(r02, "r02", line_number)?,
                parse_i64(r10, "r10", line_number)?,
                parse_i64(r11, "r11", line_number)?,
                parse_i64(r12, "r12", line_number)?,
                parse_i64(r20, "r20", line_number)?,
                parse_i64(r21, "r21", line_number)?,
                parse_i64(r22, "r22", line_number)?,
            ],
            translation_right_from_left_direction_nano: [
                parse_i64(tx, "tx", line_number)?,
                parse_i64(ty, "ty", line_number)?,
                parse_i64(tz, "tz", line_number)?,
            ],
            supported_match_count: parse_nonzero_u32(
                supported_match_count,
                "supported match count",
                line_number,
            )?,
            median_residual_nano: parse_u64(
                median_residual_nano,
                "median residual nano",
                line_number,
            )?,
        });
        if edges.len() > MAX_POSE_EDGES {
            return Err(format!(
                "pose-graph edge table contains more than {MAX_POSE_EDGES} records"
            ));
        }
    }
    Ok(edges)
}

pub(crate) fn parse_digest(value: &str, label: &str, line: usize) -> Result<EvidenceDigest, String> {
    EvidenceDigest::parse(value)
        .map_err(|error| format!("line {line}: invalid {label}: {error}"))
}

pub(crate) fn parse_digest_option(value: &str, label: &str) -> Result<EvidenceDigest, String> {
    EvidenceDigest::parse(value).map_err(|error| format!("invalid {label}: {error}"))
}

pub(crate) fn parse_nonzero_u64_option(value: &str, label: &str) -> Result<u64, String> {
    let parsed = value
        .parse::<u64>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))?;
    if parsed == 0 {
        Err(format!("{label} must be nonzero"))
    } else {
        Ok(parsed)
    }
}

pub(crate) fn parse_u64_option(value: &str, label: &str) -> Result<u64, String> {
    value
        .parse::<u64>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

pub(crate) fn parse_nonzero_u32_option(value: &str, label: &str) -> Result<u32, String> {
    let parsed = value
        .parse::<u32>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))?;
    if parsed == 0 {
        Err(format!("{label} must be nonzero"))
    } else {
        Ok(parsed)
    }
}

pub(crate) fn parse_nonzero_u16_option(value: &str, label: &str) -> Result<u16, String> {
    let parsed = value
        .parse::<u16>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))?;
    if parsed == 0 {
        Err(format!("{label} must be nonzero"))
    } else {
        Ok(parsed)
    }
}

fn parse_nonzero_u64(value: &str, label: &str, line: usize) -> Result<u64, String> {
    parse_nonzero_u64_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_u64(value: &str, label: &str, line: usize) -> Result<u64, String> {
    parse_u64_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_nonzero_u32(value: &str, label: &str, line: usize) -> Result<u32, String> {
    parse_nonzero_u32_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_i64(value: &str, label: &str, line: usize) -> Result<i64, String> {
    value
        .parse::<i64>()
        .map_err(|error| format!("line {line}: invalid {label} {value:?}: {error}"))
}
