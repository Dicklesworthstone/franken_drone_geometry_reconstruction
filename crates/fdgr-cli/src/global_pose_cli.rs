#![forbid(unsafe_code)]
//! Public deterministic component-relative camera-pose initialization.

use crate::args::OutputFormat;
use crate::pose_graph_input_cli::{
    parse_digest_option, parse_nonzero_u64_option, parse_u64_option,
};
use crate::pose_scale_pipeline_cli::{
    PoseScalePipelineOptions, PoseScalePipelineParser, build_pose_and_edge_scale,
};
use fdgr_global_pose::{
    GLOBAL_POSE_AUTHORITY, GLOBAL_POSE_INITIALIZATION_SCHEMA, GlobalPoseBasis,
    GlobalPoseInitialization, GlobalPosePolicy, initialize_global_pose,
};
use fdgr_types::EvidenceDigest;

#[derive(Clone, Debug, Eq, PartialEq)]
struct Options {
    pipeline: PoseScalePipelineOptions,
    policy_digest: EvidenceDigest,
    generation: u64,
    policy: GlobalPosePolicy,
    format: OutputFormat,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "global-pose-initialize")
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr global-pose-initialize <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> --global-pose-policy-digest <digest> --global-pose-generation <n> [pose, scale, and initialization gates] [--format text|json]"
    );
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let (pose_graph, edge_scale) = build_pose_and_edge_scale(&options.pipeline)?;
    let pose_graph_generation_digest = pose_graph
        .digest()
        .map_err(|error| format!("pose-graph identity failed: {error}"))?;
    let edge_scale_generation_digest = edge_scale
        .digest()
        .map_err(|error| format!("edge-scale identity failed: {error}"))?;
    let initialization = initialize_global_pose(
        GlobalPoseBasis {
            pose_graph_generation_digest,
            edge_scale_generation_digest,
            policy_digest: options.policy_digest,
            generation: options.generation,
        },
        options.policy,
        pose_graph,
        edge_scale,
    )
    .map_err(|error| format!("global-pose initialization rejected: {error}"))?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            initialization
                .to_json()
                .map_err(|error| format!("global-pose JSON rendering failed: {error}"))?
        ),
        OutputFormat::Text => print_text(&initialization)?,
    }
    Ok(())
}

fn print_text(initialization: &GlobalPoseInitialization) -> Result<(), String> {
    let digest = initialization
        .digest()
        .map_err(|error| format!("global-pose identity failed: {error}"))?;
    println!("schema: {GLOBAL_POSE_INITIALIZATION_SCHEMA}");
    println!("initialization_digest: {digest}");
    println!("authority: {GLOBAL_POSE_AUTHORITY}");
    println!("unit: component_edge_scale_unit_nano");
    println!(
        "pose_graph_generation_digest: {}",
        initialization.basis.pose_graph_generation_digest
    );
    println!(
        "edge_scale_generation_digest: {}",
        initialization.basis.edge_scale_generation_digest
    );
    println!("poses: {}", initialization.poses.len());
    println!(
        "translation_cycles: {}",
        initialization.translation_cycles.len()
    );
    println!("components: {}", initialization.components.len());
    println!("operations: {}", initialization.operation_count);
    for component in &initialization.components {
        println!(
            "component root={} scale_root={} status={} scale_cross_validated={} nodes={:?} forest_edges={:?} non_forest_edges={:?} conflicts={:?} incomparable_cycles={:?}",
            component.component_root_node_id,
            display_optional_u64(component.scale_component_root_edge_id),
            component.status.as_str(),
            component.scale_cross_validated,
            component.node_ids,
            component.forest_edge_ids,
            component.non_forest_edge_ids,
            component.conflicting_edge_ids,
            component.incomparable_cycle_edge_ids,
        );
    }
    for pose in &initialization.poses {
        println!(
            "pose node={} sample={} component_root={} scale_root={} center_nano={:?} parent_node={} parent_edge={}",
            pose.node_id,
            pose.sample_index,
            pose.component_root_node_id,
            display_optional_u64(pose.scale_component_root_edge_id),
            pose.camera_center_from_root_nano,
            display_optional_u64(pose.parent_node_id),
            display_optional_u64(pose.parent_edge_id),
        );
    }
    for cycle in &initialization.translation_cycles {
        println!(
            "translation_cycle closing_edge={} status={} residual_ppm={} forest_path={:?} implied_nano={:?} measured_nano={}",
            cycle.closing_edge_id,
            cycle.status.as_str(),
            display_optional_u64(cycle.residual_ppm),
            cycle.forest_path_edge_ids,
            cycle.implied_displacement_nano,
            display_optional_vector(cycle.measured_displacement_nano),
        );
    }
    Ok(())
}

fn parse(arguments: &[String]) -> Result<Options, String> {
    let usage = "usage: fdgr global-pose-initialize <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> --global-pose-policy-digest <digest> --global-pose-generation <n> [pose, scale, and initialization gates] [--format text|json]";
    let Some(node_path) = arguments.first() else {
        return Err(usage.to_owned());
    };
    let Some(edge_path) = arguments.get(1) else {
        return Err(usage.to_owned());
    };
    let Some(witness_path) = arguments.get(2) else {
        return Err(usage.to_owned());
    };
    let mut pipeline = PoseScalePipelineParser::new(node_path, edge_path, witness_path);
    let mut policy_digest = None;
    let mut generation = None;
    let mut policy = GlobalPosePolicy::default();
    let mut format = OutputFormat::Text;
    let mut position = 3_usize;
    while position < arguments.len() {
        let flag = arguments
            .get(position)
            .ok_or_else(|| usage.to_owned())?;
        let value = arguments
            .get(position.saturating_add(1))
            .ok_or_else(|| format!("missing value for {flag}"))?;
        if pipeline.apply_option(flag, value)? {
            position = position.saturating_add(2);
            continue;
        }
        match flag.as_str() {
            "--global-pose-policy-digest" => {
                policy_digest = Some(parse_digest_option(value, "global-pose policy digest")?);
            }
            "--global-pose-generation" => {
                generation = Some(parse_nonzero_u64_option(value, "global-pose generation")?);
            }
            "--max-translation-cycle-residual-ppm" => {
                policy.max_translation_cycle_residual_ppm =
                    parse_u64_option(value, "maximum translation cycle residual ppm")?;
            }
            "--max-camera-center-abs-nano" => {
                policy.max_camera_center_abs_nano =
                    parse_nonzero_u64_option(value, "maximum camera-center magnitude nano")?;
            }
            "--max-global-pose-operations" => {
                policy.max_operations =
                    parse_nonzero_u64_option(value, "maximum global-pose operations")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown global-pose-initialize option {flag:?}")),
        }
        position = position.saturating_add(2);
    }
    Ok(Options {
        pipeline: pipeline.finish()?,
        policy_digest: policy_digest
            .ok_or_else(|| "missing --global-pose-policy-digest".to_owned())?,
        generation: generation.ok_or_else(|| "missing --global-pose-generation".to_owned())?,
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

fn display_optional_u64(value: Option<u64>) -> String {
    value.map_or_else(|| "none".to_owned(), |value| value.to_string())
}

fn display_optional_vector(value: Option<[i64; 3]>) -> String {
    value.map_or_else(|| "none".to_owned(), |value| format!("{value:?}"))
}

#[cfg(test)]
mod tests {
    use super::{is_command, parse};

    #[test]
    fn command_detection_is_exact() {
        assert!(is_command(&["global-pose-initialize".to_owned()]));
        assert!(!is_command(&["global-pose".to_owned()]));
        assert!(!is_command(&[]));
    }

    #[test]
    fn parser_requires_global_pose_identity() {
        let arguments = vec![
            "nodes.tsv".to_owned(),
            "edges.tsv".to_owned(),
            "witnesses.tsv".to_owned(),
        ];
        assert!(matches!(parse(&arguments), Err(ref error) if error.contains("--node-file-digest")));
    }
}
