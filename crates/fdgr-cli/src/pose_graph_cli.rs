#![forbid(unsafe_code)]
//! Exact-byte public adapter for deterministic pose-graph construction.

use crate::args::OutputFormat;
use crate::pose_graph_input_cli::{
    PoseGraphFileBasis, build_pose_graph_from_files, parse_digest_option,
    parse_nonzero_u64_option, parse_u64_option,
};
use fdgr_pose_graph::PoseGraphPolicy;
use std::path::PathBuf;

#[derive(Clone, Debug, Eq, PartialEq)]
struct Options {
    basis: PoseGraphFileBasis,
    policy: PoseGraphPolicy,
    format: OutputFormat,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "pose-graph-build")
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr pose-graph-build <nodes.tsv> <edges.tsv> --node-file-digest <digest> --edge-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --generation <n> [pose-graph gates] [--format text|json]"
    );
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let graph = build_pose_graph_from_files(&options.basis, options.policy)?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            graph
                .to_json()
                .map_err(|error| format!("pose-graph JSON rendering failed: {error}"))?
        ),
        OutputFormat::Text => {
            let digest = graph
                .digest()
                .map_err(|error| format!("pose-graph identity failed: {error}"))?;
            println!("schema: fdgr.pose_graph_generation/1");
            println!("generation_digest: {digest}");
            println!("nodes: {}", graph.nodes.len());
            println!("edges: {}", graph.edges.len());
            println!("components: {}", graph.components.len());
            println!("cycles: {}", graph.cycle_assessments.len());
            println!("translation_status: {}", graph.translation_status.as_str());
            println!("forest_edges: {:?}", graph.topology.forest_edge_ids);
            println!("bridge_edges: {:?}", graph.topology.bridge_edge_ids);
            for component in &graph.components {
                println!(
                    "component {}: orientation_status={} nodes={} edges={} conflicts={:?}",
                    component.component_root_node_id,
                    component.orientation_status.as_str(),
                    component.node_ids.len(),
                    component.edge_ids.len(),
                    component.conflicting_edge_ids,
                );
            }
        }
    }
    Ok(())
}

fn parse(arguments: &[String]) -> Result<Options, String> {
    let usage = "usage: fdgr pose-graph-build <nodes.tsv> <edges.tsv> --node-file-digest <digest> --edge-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --generation <n> [--max-rotation-cycle-residual-ppm n] [--max-orientation-drift-ppm n] [--max-path-expansions n] [--format text|json]";
    let Some(node_path) = arguments.first() else {
        return Err(usage.to_owned());
    };
    let Some(edge_path) = arguments.get(1) else {
        return Err(usage.to_owned());
    };
    let mut node_file_digest = None;
    let mut edge_file_digest = None;
    let mut graph_selection_policy_digest = None;
    let mut rotation_policy_digest = None;
    let mut generation = None;
    let mut policy = PoseGraphPolicy::default();
    let mut format = OutputFormat::Text;
    let mut position = 2_usize;
    while position < arguments.len() {
        let flag = arguments
            .get(position)
            .ok_or_else(|| usage.to_owned())?;
        let value = arguments
            .get(position.saturating_add(1))
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--node-file-digest" => {
                node_file_digest = Some(parse_digest_option(value, "node file digest")?);
            }
            "--edge-file-digest" => {
                edge_file_digest = Some(parse_digest_option(value, "edge file digest")?);
            }
            "--graph-selection-policy-digest" => {
                graph_selection_policy_digest =
                    Some(parse_digest_option(value, "graph selection policy digest")?);
            }
            "--rotation-policy-digest" => {
                rotation_policy_digest =
                    Some(parse_digest_option(value, "rotation policy digest")?);
            }
            "--generation" => {
                generation = Some(parse_nonzero_u64_option(value, "generation")?);
            }
            "--max-rotation-cycle-residual-ppm" => {
                policy.max_rotation_cycle_residual_ppm =
                    parse_u64_option(value, "maximum rotation cycle residual ppm")?;
            }
            "--max-orientation-drift-ppm" => {
                policy.max_orientation_drift_ppm =
                    parse_u64_option(value, "maximum orientation drift ppm")?;
            }
            "--max-path-expansions" => {
                policy.max_path_expansions =
                    parse_nonzero_u64_option(value, "maximum path expansions")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown pose-graph-build option {flag:?}")),
        }
        position = position.saturating_add(2);
    }
    Ok(Options {
        basis: PoseGraphFileBasis {
            node_path: PathBuf::from(node_path),
            edge_path: PathBuf::from(edge_path),
            node_file_digest: node_file_digest
                .ok_or_else(|| "missing --node-file-digest".to_owned())?,
            edge_file_digest: edge_file_digest
                .ok_or_else(|| "missing --edge-file-digest".to_owned())?,
            graph_selection_policy_digest: graph_selection_policy_digest
                .ok_or_else(|| "missing --graph-selection-policy-digest".to_owned())?,
            rotation_policy_digest: rotation_policy_digest
                .ok_or_else(|| "missing --rotation-policy-digest".to_owned())?,
            generation: generation.ok_or_else(|| "missing --generation".to_owned())?,
        },
        policy,
        format,
    })
}

fn parse_format(value: &str) -> Result<OutputFormat, String> {
    match value {
        "text" => Ok(OutputFormat::Text),
        "json" => Ok(OutputFormat::Json),
        _ => Err(format!(
            "unknown output format {value:?}; expected text or json"
        )),
    }
}
