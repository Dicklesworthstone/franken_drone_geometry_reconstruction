#![forbid(unsafe_code)]
#![allow(clippy::too_many_lines)]
//! Public exact-byte adapter for deterministic bundle-admission auditing.

use crate::args::OutputFormat;
use crate::bundle_problem_pipeline_cli::{
    BundleProblemPipelineOptions, BundleProblemPipelineParser, build_bundle_problem,
};
use crate::geometry_observation_cli::read_bound_text;
use crate::pose_graph_input_cli::{
    parse_digest, parse_digest_option, parse_nonzero_u32_option, parse_nonzero_u64_option,
};
use fdgr_bundle_admission::{
    BUNDLE_ADMISSION_SCHEMA, BundleAdmissionBasis, BundleAdmissionGeneration,
    BundleAdmissionPolicy, BundleCameraDomain, LandmarkSeedProvenance, MAX_CAMERA_DOMAINS,
    MAX_SEED_PROVENANCES, audit_bundle_admission, bundle_admission_policy_digest,
    camera_domain_table_digest, seed_provenance_table_digest,
};
use fdgr_types::EvidenceDigest;
use std::path::{Path, PathBuf};

const CAMERA_DOMAIN_HEADER: &str =
    "camera_node_id\tframe_digest\teffective_calibration_digest\timage_width\timage_height";
const SEED_PROVENANCE_HEADER: &str = "landmark_id\tsupport_observation_ids";
const MAX_CAMERA_DOMAIN_TABLE_BYTES: u64 = 64 * 1024 * 1024;
const MAX_SEED_PROVENANCE_TABLE_BYTES: u64 = 128 * 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
struct Options {
    problem: BundleProblemPipelineOptions,
    camera_domain_path: PathBuf,
    camera_domain_file_digest: EvidenceDigest,
    seed_provenance_path: PathBuf,
    seed_provenance_file_digest: EvidenceDigest,
    generation: u64,
    policy: BundleAdmissionPolicy,
    format: OutputFormat,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "bundle-admission-audit")
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr bundle-admission-audit <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> <camera-bindings.tsv> <landmark-seeds.tsv> <bundle-observations.tsv> <camera-domains.tsv> <seed-provenance.tsv> --node-file-digest <digest> --pose-edge-file-digest <digest> --scale-witness-file-digest <digest> --camera-binding-file-digest <digest> --landmark-seed-file-digest <digest> --bundle-observation-file-digest <digest> --camera-domain-file-digest <digest> --seed-provenance-file-digest <digest> --graph-selection-policy-digest <digest> --rotation-policy-digest <digest> --pose-graph-generation <n> --edge-scale-policy-digest <digest> --edge-scale-generation <n> --global-pose-policy-digest <digest> --global-pose-generation <n> --pose-refinement-policy-digest <digest> --pose-refinement-generation <n> --bundle-problem-generation <n> --bundle-admission-generation <n> [upstream, structural, and audit gates] [--format text|json]"
    );
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let problem = build_bundle_problem(&options.problem)?;
    let camera_domains = read_camera_domains(
        &options.camera_domain_path,
        &options.camera_domain_file_digest,
    )?;
    let seed_provenance = read_seed_provenance(
        &options.seed_provenance_path,
        &options.seed_provenance_file_digest,
    )?;
    let bundle_problem_digest = problem
        .digest()
        .map_err(|error| format!("bundle-problem identity failed: {error}"))?;
    let camera_domain_basis_digest = camera_domain_table_digest(&camera_domains)
        .map_err(|error| format!("camera-domain table rejected: {error}"))?;
    let seed_provenance_basis_digest = seed_provenance_table_digest(&seed_provenance)
        .map_err(|error| format!("seed-provenance table rejected: {error}"))?;
    let policy_digest = bundle_admission_policy_digest(options.policy)
        .map_err(|error| format!("bundle-admission policy rejected: {error}"))?;
    let generation = audit_bundle_admission(
        BundleAdmissionBasis {
            bundle_problem_digest,
            camera_domain_basis_digest,
            seed_provenance_basis_digest,
            policy_digest,
            generation: options.generation,
        },
        options.policy,
        problem,
        camera_domains,
        seed_provenance,
    )
    .map_err(|error| format!("bundle-admission audit rejected: {error}"))?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            generation
                .to_json()
                .map_err(|error| format!("bundle-admission JSON rendering failed: {error}"))?
        ),
        OutputFormat::Text => print_text(&generation)?,
    }
    Ok(())
}

fn print_text(generation: &BundleAdmissionGeneration) -> Result<(), String> {
    let digest = generation
        .digest()
        .map_err(|error| format!("bundle-admission identity failed: {error}"))?;
    println!("schema: {BUNDLE_ADMISSION_SCHEMA}");
    println!("bundle_admission_digest: {digest}");
    println!("authority: {}", generation.authority());
    println!(
        "bundle_problem_digest: {}",
        generation.basis.bundle_problem_digest
    );
    println!(
        "camera_domain_basis_digest: {}",
        generation.basis.camera_domain_basis_digest
    );
    println!(
        "seed_provenance_basis_digest: {}",
        generation.basis.seed_provenance_basis_digest
    );
    println!("policy_digest: {}", generation.basis.policy_digest);
    println!("generation: {}", generation.basis.generation);
    println!("camera_domains: {}", generation.camera_domains.len());
    println!("seed_provenance: {}", generation.seed_provenance.len());
    println!("observation_audits: {}", generation.observations.len());
    println!("landmark_audits: {}", generation.landmarks.len());
    println!("components: {}", generation.components.len());
    println!("operations: {}", generation.operation_count);
    for component in &generation.components {
        println!(
            "component root={} scale_root={} status={} decision={} recommendation={} invalid_image_observations={:?} unproven_seeds={:?} inactive_held_out={:?} independent_held_out={:?} independent_held_out_cameras={:?}",
            component.component_root_node_id,
            display_optional_u64(component.scale_component_root_edge_id),
            component.status.as_str(),
            component.decision.as_str(),
            component.recommendation.as_str(),
            component.invalid_image_observation_ids,
            component.unproven_seed_landmark_ids,
            component.inactive_held_out_observation_ids,
            component.independent_held_out_observation_ids,
            component.independent_held_out_camera_node_ids,
        );
    }
    Ok(())
}

fn parse(arguments: &[String]) -> Result<Options, String> {
    let usage = "usage: fdgr bundle-admission-audit <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> <camera-bindings.tsv> <landmark-seeds.tsv> <bundle-observations.tsv> <camera-domains.tsv> <seed-provenance.tsv> [exact upstream identities] --camera-domain-file-digest <digest> --seed-provenance-file-digest <digest> --bundle-admission-generation <n> [upstream, structural, and audit gates] [--format text|json]";
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
    let Some(camera_domain_path) = arguments.get(6) else {
        return Err(usage.to_owned());
    };
    let Some(seed_provenance_path) = arguments.get(7) else {
        return Err(usage.to_owned());
    };
    let mut problem = BundleProblemPipelineParser::new(
        node_path,
        edge_path,
        witness_path,
        camera_binding_path,
        landmark_seed_path,
        observation_path,
    );
    let mut camera_domain_file_digest = None;
    let mut seed_provenance_file_digest = None;
    let mut generation = None;
    let mut policy = BundleAdmissionPolicy::default();
    let mut format = OutputFormat::Text;
    let mut position = 8_usize;
    while position < arguments.len() {
        let flag = arguments
            .get(position)
            .ok_or_else(|| usage.to_owned())?;
        let value = arguments
            .get(position.saturating_add(1))
            .ok_or_else(|| format!("missing value for {flag}"))?;
        if problem.apply_option(flag, value)? {
            position = position.saturating_add(2);
            continue;
        }
        match flag.as_str() {
            "--camera-domain-file-digest" => {
                camera_domain_file_digest =
                    Some(parse_digest_option(value, "camera-domain file digest")?);
            }
            "--seed-provenance-file-digest" => {
                seed_provenance_file_digest =
                    Some(parse_digest_option(value, "seed-provenance file digest")?);
            }
            "--bundle-admission-generation" => {
                generation =
                    Some(parse_nonzero_u64_option(value, "bundle-admission generation")?);
            }
            "--max-seed-uncertainty-nano" => {
                policy.max_seed_uncertainty_nano =
                    parse_nonzero_u64_option(value, "maximum seed uncertainty nano")?;
            }
            "--min-seed-support-observations" => {
                policy.min_seed_support_observations = parse_nonzero_u32_option(
                    value,
                    "minimum seed-support observations",
                )?;
            }
            "--min-seed-support-cameras" => {
                policy.min_seed_support_cameras =
                    parse_nonzero_u32_option(value, "minimum seed-support cameras")?;
            }
            "--require-active-held-out-camera" => {
                policy.require_active_held_out_camera =
                    parse_bool_option(value, "require active held-out camera")?;
            }
            "--max-bundle-admission-operations" => {
                policy.max_operations =
                    parse_nonzero_u64_option(value, "maximum bundle-admission operations")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown bundle-admission-audit option {flag:?}")),
        }
        position = position.saturating_add(2);
    }
    Ok(Options {
        problem: problem.finish()?,
        camera_domain_path: PathBuf::from(camera_domain_path),
        camera_domain_file_digest: camera_domain_file_digest
            .ok_or_else(|| "missing --camera-domain-file-digest".to_owned())?,
        seed_provenance_path: PathBuf::from(seed_provenance_path),
        seed_provenance_file_digest: seed_provenance_file_digest
            .ok_or_else(|| "missing --seed-provenance-file-digest".to_owned())?,
        generation: generation
            .ok_or_else(|| "missing --bundle-admission-generation".to_owned())?,
        policy,
        format,
    })
}

fn read_camera_domains(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<BundleCameraDomain>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_CAMERA_DOMAIN_TABLE_BYTES,
        "bundle camera-domain",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "bundle camera-domain table is empty".to_owned())?;
    if header != CAMERA_DOMAIN_HEADER {
        return Err(format!(
            "bundle camera-domain header mismatch: expected {CAMERA_DOMAIN_HEADER:?}"
        ));
    }
    let mut domains = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        reject_empty_line(line, "bundle camera-domain", line_number)?;
        let fields = line.split('\t').collect::<Vec<_>>();
        let [camera_node_id, frame_digest, effective_calibration_digest, image_width, image_height] =
            fields.as_slice()
        else {
            return Err(format!(
                "bundle camera-domain line {line_number} must contain exactly five tab-separated fields"
            ));
        };
        domains.push(BundleCameraDomain {
            camera_node_id: parse_nonzero_u64_line(
                camera_node_id,
                "camera node id",
                line_number,
            )?,
            frame_digest: parse_digest(frame_digest, "frame digest", line_number)?,
            effective_calibration_digest: parse_digest(
                effective_calibration_digest,
                "effective calibration digest",
                line_number,
            )?,
            image_width: parse_nonzero_u32_line(image_width, "image width", line_number)?,
            image_height: parse_nonzero_u32_line(image_height, "image height", line_number)?,
        });
        if domains.len() > MAX_CAMERA_DOMAINS {
            return Err(format!(
                "bundle camera-domain table contains more than {MAX_CAMERA_DOMAINS} records"
            ));
        }
    }
    Ok(domains)
}

fn read_seed_provenance(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<LandmarkSeedProvenance>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_SEED_PROVENANCE_TABLE_BYTES,
        "bundle seed-provenance",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "bundle seed-provenance table is empty".to_owned())?;
    if header != SEED_PROVENANCE_HEADER {
        return Err(format!(
            "bundle seed-provenance header mismatch: expected {SEED_PROVENANCE_HEADER:?}"
        ));
    }
    let mut provenance = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        reject_empty_line(line, "bundle seed-provenance", line_number)?;
        let fields = line.split('\t').collect::<Vec<_>>();
        let [landmark_id, support_observation_ids] = fields.as_slice() else {
            return Err(format!(
                "bundle seed-provenance line {line_number} must contain exactly two tab-separated fields"
            ));
        };
        provenance.push(LandmarkSeedProvenance {
            landmark_id: parse_nonzero_u64_line(landmark_id, "landmark id", line_number)?,
            support_observation_ids: parse_id_list(
                support_observation_ids,
                "support observation ids",
                line_number,
            )?,
        });
        if provenance.len() > MAX_SEED_PROVENANCES {
            return Err(format!(
                "bundle seed-provenance table contains more than {MAX_SEED_PROVENANCES} records"
            ));
        }
    }
    Ok(provenance)
}

fn parse_id_list(value: &str, label: &str, line: usize) -> Result<Vec<u64>, String> {
    if value == "none" {
        return Ok(Vec::new());
    }
    if value.is_empty() {
        return Err(format!(
            "line {line}: {label} must be comma-separated nonzero integers or none"
        ));
    }
    value
        .split(',')
        .map(|entry| parse_nonzero_u64_line(entry, label, line))
        .collect()
}

fn parse_nonzero_u64_line(value: &str, label: &str, line: usize) -> Result<u64, String> {
    parse_nonzero_u64_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_nonzero_u32_line(value: &str, label: &str, line: usize) -> Result<u32, String> {
    parse_nonzero_u32_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_bool_option(value: &str, label: &str) -> Result<bool, String> {
    match value {
        "true" => Ok(true),
        "false" => Ok(false),
        _ => Err(format!(
            "invalid {label} {value:?}; expected true or false"
        )),
    }
}

fn reject_empty_line(line: &str, label: &str, line_number: usize) -> Result<(), String> {
    if line.is_empty() {
        Err(format!("{label} line {line_number} is empty"))
    } else {
        Ok(())
    }
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
    use super::{is_command, parse_bool_option, parse_id_list};

    #[test]
    fn command_detection_is_exact() {
        assert!(is_command(&["bundle-admission-audit".to_owned()]));
        assert!(!is_command(&["bundle-audit".to_owned()]));
        assert!(!is_command(&[]));
    }

    #[test]
    fn closed_bool_and_identifier_list_grammars_are_exact() {
        assert_eq!(parse_bool_option("true", "flag"), Ok(true));
        assert!(parse_bool_option("1", "flag").is_err());
        assert_eq!(parse_id_list("none", "ids", 2), Ok(Vec::new()));
        assert_eq!(parse_id_list("1,2,3", "ids", 2), Ok(vec![1, 2, 3]));
        assert!(parse_id_list("1,,3", "ids", 2).is_err());
        assert!(parse_id_list("0", "ids", 2).is_err());
    }
}
