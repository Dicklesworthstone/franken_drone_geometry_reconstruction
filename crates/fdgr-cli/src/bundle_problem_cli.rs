#![forbid(unsafe_code)]
//! Public exact-byte adapter for deterministic structural bundle-problem compilation.

use crate::args::OutputFormat;
use crate::bundle_problem_pipeline_cli::{
    BundleProblemPipelineOptions, BundleProblemPipelineParser, build_bundle_problem,
};
use fdgr_bundle_problem::{
    BUNDLE_PROBLEM_AUTHORITY, BUNDLE_PROBLEM_SCHEMA, BundleProblemGeneration,
};

#[derive(Clone, Debug, Eq, PartialEq)]
struct Options {
    problem: BundleProblemPipelineOptions,
    format: OutputFormat,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "bundle-problem-build")
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr bundle-problem-build <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> <camera-bindings.tsv> <landmark-seeds.tsv> <bundle-observations.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --camera-binding-file-digest <digest> --landmark-seed-file-digest <digest> --bundle-observation-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> --global-pose-policy-digest <digest> --global-pose-generation <n> --pose-refinement-policy-digest <digest> --pose-refinement-generation <n> --bundle-problem-generation <n> [pose, scale, initialization, refinement, and structural gates] [--format text|json]"
    );
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let generation = build_bundle_problem(&options.problem)?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            generation
                .to_json()
                .map_err(|error| format!("bundle-problem JSON rendering failed: {error}"))?
        ),
        OutputFormat::Text => print_text(&generation)?,
    }
    Ok(())
}

fn print_text(generation: &BundleProblemGeneration) -> Result<(), String> {
    let digest = generation
        .digest()
        .map_err(|error| format!("bundle-problem identity failed: {error}"))?;
    let graph_digest = generation
        .graph
        .digest()
        .map_err(|error| format!("bundle graph identity failed: {error}"))?;
    println!("schema: {BUNDLE_PROBLEM_SCHEMA}");
    println!("bundle_problem_digest: {digest}");
    println!("authority: {BUNDLE_PROBLEM_AUTHORITY}");
    println!(
        "pose_refinement_digest: {}",
        generation.basis.pose_refinement_digest
    );
    println!(
        "camera_binding_basis_digest: {}",
        generation.basis.camera_binding_basis_digest
    );
    println!(
        "landmark_seed_basis_digest: {}",
        generation.basis.landmark_seed_basis_digest
    );
    println!(
        "observation_basis_digest: {}",
        generation.basis.observation_basis_digest
    );
    println!("policy_digest: {}", generation.basis.policy_digest);
    println!("generation: {}", generation.basis.generation);
    println!("cameras: {}", generation.cameras.len());
    println!("landmarks: {}", generation.landmarks.len());
    println!("observations: {}", generation.observations.len());
    println!("graph_digest: {graph_digest}");
    println!("graph_nodes: {}", generation.graph_nodes.len());
    println!("graph_edges: {}", generation.graph.edges.len());
    println!("graph_cycle_rank: {}", generation.graph.cycle_rank);
    println!("graph_path_expansions: {}", generation.graph.path_expansions);
    println!("pruning_rounds: {}", generation.pruning_rounds);
    println!("operations: {}", generation.operation_count);
    for component in &generation.components {
        println!(
            "component root={} scale_root={} status={} decision={} recommendation={} root_connected={} root_landmarks={} cycle_rank={} nominal_equations={} nominal_unknowns={} nominal_surplus={} rank_authority=planning_count_only active_cameras={:?} pruned_cameras={:?} active_landmarks={:?} pruned_landmarks={:?} optimize_observations={:?} held_out_observations={:?} held_out_cameras={:?} bridges={:?} forest={:?} non_forest={:?}",
            component.component_root_node_id,
            display_optional_u64(component.scale_component_root_edge_id),
            component.status.as_str(),
            component.decision.as_str(),
            component.recommendation.as_str(),
            component.root_connected,
            component.root_active_landmark_count,
            component.cycle_rank,
            component.nominal_equation_count,
            component.nominal_unknown_count,
            component.nominal_equation_surplus,
            component.active_camera_node_ids,
            component.pruned_camera_node_ids,
            component.active_landmark_ids,
            component.pruned_landmark_ids,
            component.active_optimize_observation_ids,
            component.eligible_held_out_observation_ids,
            component.eligible_held_out_camera_node_ids,
            component.bridge_observation_ids,
            component.forest_observation_ids,
            component.non_forest_observation_ids,
        );
    }
    Ok(())
}

fn parse(arguments: &[String]) -> Result<Options, String> {
    let usage = "usage: fdgr bundle-problem-build <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> <camera-bindings.tsv> <landmark-seeds.tsv> <bundle-observations.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --camera-binding-file-digest <digest> --landmark-seed-file-digest <digest> --bundle-observation-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> --global-pose-policy-digest <digest> --global-pose-generation <n> --pose-refinement-policy-digest <digest> --pose-refinement-generation <n> --bundle-problem-generation <n> [pose, scale, initialization, refinement, and structural gates] [--format text|json]";
    let Some(node_path) = arguments.first() else {
        return Err(usage.to_owned());
    };
    let Some(edge_path) = arguments.get(1) else {
        return Err(usage.to_owned());
    };
    let Some(witness_path) = arguments.get(2) else {
        return Err(usage.to_owned());
    };
    let Some(camera_binding_path) = arguments.get(3) else {
        return Err(usage.to_owned());
    };
    let Some(landmark_seed_path) = arguments.get(4) else {
        return Err(usage.to_owned());
    };
    let Some(observation_path) = arguments.get(5) else {
        return Err(usage.to_owned());
    };
    let mut parser = BundleProblemPipelineParser::new(
        node_path,
        edge_path,
        witness_path,
        camera_binding_path,
        landmark_seed_path,
        observation_path,
    );
    let mut format = OutputFormat::Text;
    let mut position = 6_usize;
    while position < arguments.len() {
        let flag = arguments
            .get(position)
            .ok_or_else(|| usage.to_owned())?;
        let value = arguments
            .get(position.saturating_add(1))
            .ok_or_else(|| format!("missing value for {flag}"))?;
        if parser.apply_option(flag, value)? {
            position = position.saturating_add(2);
            continue;
        }
        match flag.as_str() {
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown bundle-problem-build option {flag:?}")),
        }
        position = position.saturating_add(2);
    }
    Ok(Options {
        problem: parser.finish()?,
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

fn display_optional_u64(value: Option<u64>) -> String {
    value.map_or_else(|| "none".to_owned(), |value| value.to_string())
}

#[cfg(test)]
mod tests {
    use super::is_command;

    #[test]
    fn command_detection_is_exact() {
        assert!(is_command(&["bundle-problem-build".to_owned()]));
        assert!(!is_command(&["bundle-build".to_owned()]));
        assert!(!is_command(&[]));
    }
}
