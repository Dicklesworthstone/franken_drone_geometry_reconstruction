#![forbid(unsafe_code)]
//! Public deterministic translation-only pose refinement.

use crate::args::OutputFormat;
use crate::global_pose_pipeline_cli::{
    GlobalPosePipelineOptions, GlobalPosePipelineParser, build_global_pose,
};
use crate::pose_graph_input_cli::{
    parse_digest_option, parse_nonzero_u32_option, parse_nonzero_u64_option, parse_u64_option,
};
use fdgr_pose_refinement::{
    POSE_REFINEMENT_AUTHORITY, POSE_REFINEMENT_SCHEMA, PoseRefinementBasis,
    PoseRefinementGeneration, PoseRefinementPolicy, refine_pose_translation,
};
use fdgr_types::EvidenceDigest;

#[derive(Clone, Debug, Eq, PartialEq)]
struct Options {
    pipeline: GlobalPosePipelineOptions,
    policy_digest: EvidenceDigest,
    generation: u64,
    policy: PoseRefinementPolicy,
    format: OutputFormat,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "pose-refine")
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr pose-refine <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> --global-pose-policy-digest <digest> --global-pose-generation <n> --pose-refinement-policy-digest <digest> --pose-refinement-generation <n> [pose, scale, initialization, and refinement gates] [--format text|json]"
    );
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let initialization = build_global_pose(&options.pipeline)?;
    let initialization_digest = initialization
        .digest()
        .map_err(|error| format!("global-pose identity failed: {error}"))?;
    let refinement = refine_pose_translation(
        PoseRefinementBasis {
            initialization_digest,
            policy_digest: options.policy_digest,
            generation: options.generation,
        },
        options.policy,
        initialization,
    )
    .map_err(|error| format!("pose refinement rejected: {error}"))?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            refinement
                .to_json()
                .map_err(|error| format!("pose-refinement JSON rendering failed: {error}"))?
        ),
        OutputFormat::Text => print_text(&refinement)?,
    }
    Ok(())
}

fn print_text(refinement: &PoseRefinementGeneration) -> Result<(), String> {
    let digest = refinement
        .digest()
        .map_err(|error| format!("pose-refinement identity failed: {error}"))?;
    println!("schema: {POSE_REFINEMENT_SCHEMA}");
    println!("refinement_digest: {digest}");
    println!("authority: {POSE_REFINEMENT_AUTHORITY}");
    println!("unit: component_edge_scale_unit_nano");
    println!(
        "initialization_digest: {}",
        refinement.basis.initialization_digest
    );
    println!("poses: {}", refinement.poses.len());
    println!("factors: {}", refinement.factors.len());
    println!("components: {}", refinement.components.len());
    println!("iterations: {}", refinement.iteration_count);
    println!("operations: {}", refinement.operation_count);
    for component in &refinement.components {
        println!(
            "component root={} scale_root={} status={} decision={} reason={} iterations={} initial_rms_nano={} final_rms_nano={} improvement_nano={} max_adjustment_nano={} do_nothing_dominates={} active_factors={:?} downweighted={:?}",
            component.component_root_node_id,
            display_optional_u64(component.scale_component_root_edge_id),
            component.status.as_str(),
            component.decision.as_str(),
            component.reason.as_str(),
            component.iterations,
            display_optional_u64(component.initial_rms_residual_nano),
            display_optional_u64(component.final_rms_residual_nano),
            display_optional_u64(component.rms_improvement_nano),
            component.max_adjustment_nano,
            component.do_nothing_dominates,
            component.active_factor_edge_ids,
            component.downweighted_edge_ids,
        );
    }
    for pose in &refinement.poses {
        println!(
            "pose node={} sample={} component_root={} scale_root={} initial_center_nano={:?} refined_center_nano={:?} adjustment_nano={:?} root_pinned={}",
            pose.node_id,
            pose.sample_index,
            pose.component_root_node_id,
            display_optional_u64(pose.scale_component_root_edge_id),
            pose.initial_camera_center_from_root_nano,
            pose.refined_camera_center_from_root_nano,
            pose.adjustment_nano,
            pose.root_pinned,
        );
    }
    for factor in &refinement.factors {
        println!(
            "factor edge={} component_root={} endpoints=({}, {}) disposition={} base_weight={} robust_weight={} initial_residual_nano={} final_residual_nano={} measured_nano={}",
            factor.edge_id,
            factor.component_root_node_id,
            factor.left_node_id,
            factor.right_node_id,
            factor.disposition.as_str(),
            factor.base_weight,
            factor.robust_weight,
            display_optional_u64(factor.initial_residual_nano),
            display_optional_u64(factor.final_residual_nano),
            display_optional_vector(factor.measured_displacement_nano),
        );
    }
    Ok(())
}

fn parse(arguments: &[String]) -> Result<Options, String> {
    let usage = "usage: fdgr pose-refine <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> --global-pose-policy-digest <digest> --global-pose-generation <n> --pose-refinement-policy-digest <digest> --pose-refinement-generation <n> [pose, scale, initialization, and refinement gates] [--format text|json]";
    let Some(node_path) = arguments.first() else {
        return Err(usage.to_owned());
    };
    let Some(edge_path) = arguments.get(1) else {
        return Err(usage.to_owned());
    };
    let Some(witness_path) = arguments.get(2) else {
        return Err(usage.to_owned());
    };
    let mut pipeline = GlobalPosePipelineParser::new(node_path, edge_path, witness_path);
    let mut policy_digest = None;
    let mut generation = None;
    let mut policy = PoseRefinementPolicy::default();
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
            "--pose-refinement-policy-digest" => {
                policy_digest = Some(parse_digest_option(value, "pose-refinement policy digest")?);
            }
            "--pose-refinement-generation" => {
                generation = Some(parse_nonzero_u64_option(value, "pose-refinement generation")?);
            }
            "--max-refinement-iterations" => {
                policy.max_iterations =
                    parse_nonzero_u32_option(value, "maximum refinement iterations")?;
            }
            "--refinement-convergence-delta-nano" => {
                policy.convergence_delta_nano =
                    parse_u64_option(value, "refinement convergence delta nano")?;
            }
            "--refinement-huber-delta-nano" => {
                policy.huber_delta_nano =
                    parse_nonzero_u64_option(value, "refinement Huber delta nano")?;
            }
            "--refinement-damping-weight" => {
                policy.damping_weight =
                    parse_nonzero_u32_option(value, "refinement damping weight")?;
            }
            "--max-refinement-factor-weight" => {
                policy.max_factor_weight =
                    parse_nonzero_u32_option(value, "maximum refinement factor weight")?;
            }
            "--max-refinement-camera-center-abs-nano" => {
                policy.max_camera_center_abs_nano = parse_nonzero_u64_option(
                    value,
                    "maximum refined camera-center magnitude nano",
                )?;
            }
            "--max-refinement-operations" => {
                policy.max_operations =
                    parse_nonzero_u64_option(value, "maximum refinement operations")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown pose-refine option {flag:?}")),
        }
        position = position.saturating_add(2);
    }
    Ok(Options {
        pipeline: pipeline.finish()?,
        policy_digest: policy_digest
            .ok_or_else(|| "missing --pose-refinement-policy-digest".to_owned())?,
        generation: generation
            .ok_or_else(|| "missing --pose-refinement-generation".to_owned())?,
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
        assert!(is_command(&["pose-refine".to_owned()]));
        assert!(!is_command(&["pose-refinement".to_owned()]));
        assert!(!is_command(&[]));
    }

    #[test]
    fn parser_requires_exact_upstream_identity() {
        let arguments = vec![
            "nodes.tsv".to_owned(),
            "edges.tsv".to_owned(),
            "witnesses.tsv".to_owned(),
        ];
        assert!(matches!(parse(&arguments), Err(ref error) if error.contains("--node-file-digest")));
    }
}
