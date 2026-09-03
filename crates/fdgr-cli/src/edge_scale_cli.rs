#![forbid(unsafe_code)]
//! Exact-byte public adapter for pose-graph-bound relative edge-scale reconciliation.

use crate::args::OutputFormat;
use crate::pose_scale_pipeline_cli::{
    PoseScalePipelineOptions, PoseScalePipelineParser, build_pose_and_edge_scale,
};

#[derive(Clone, Debug, Eq, PartialEq)]
struct Options {
    pipeline: PoseScalePipelineOptions,
    format: OutputFormat,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "edge-scale-resolve")
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr edge-scale-resolve <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> [pose and scale gates] [--format text|json]"
    );
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let (_, generation) = build_pose_and_edge_scale(&options.pipeline)?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            generation
                .to_json()
                .map_err(|error| format!("edge-scale JSON rendering failed: {error}"))?
        ),
        OutputFormat::Text => {
            let digest = generation
                .digest()
                .map_err(|error| format!("edge-scale identity failed: {error}"))?;
            println!("schema: fdgr.edge_scale_generation/1");
            println!("generation_digest: {digest}");
            println!(
                "pose_graph_generation_digest: {}",
                generation.basis.pose_graph_generation_digest
            );
            println!("unit: component_edge_scale_unit");
            println!("subjects: {}", generation.subjects.len());
            println!("witnesses: {}", generation.witnesses.len());
            println!("relations: {}", generation.relations.len());
            println!("components: {}", generation.components.len());
            for component in &generation.components {
                println!(
                    "component edge-root {} pose-root {}: status={} cross_validated={} edges={} conflicts={:?}",
                    component.scale_component_root_edge_id,
                    component.pose_component_root_node_id,
                    component.status.as_str(),
                    component.cross_validated,
                    component.edge_ids.len(),
                    component.conflicting_relation_ids,
                );
            }
        }
    }
    Ok(())
}

fn parse(arguments: &[String]) -> Result<Options, String> {
    let usage = "usage: fdgr edge-scale-resolve <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> [pose and scale gates] [--format text|json]";
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
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown edge-scale-resolve option {flag:?}")),
        }
        position = position.saturating_add(2);
    }
    Ok(Options {
        pipeline: pipeline.finish()?,
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
