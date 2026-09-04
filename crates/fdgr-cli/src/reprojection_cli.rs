#![forbid(unsafe_code)]
#![allow(
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::struct_excessive_bools
)]
//! Public exact-byte adapter for deterministic calibrated reprojection evidence.

use crate::args::OutputFormat;
use crate::bundle_problem_pipeline_cli::{
    BundleProblemPipelineOptions, BundleProblemPipelineParser, build_bundle_problem,
};
use crate::geometry_observation_cli::read_bound_text;
use crate::pose_graph_input_cli::{
    parse_digest, parse_digest_option, parse_nonzero_u32_option, parse_nonzero_u64_option,
    parse_u64_option,
};
use fdgr_bundle_admission::{
    BundleAdmissionBasis, BundleAdmissionGeneration, BundleAdmissionPolicy, BundleCameraDomain,
    LandmarkSeedProvenance, MAX_CAMERA_DOMAINS, MAX_SEED_PROVENANCES,
    audit_bundle_admission, bundle_admission_policy_digest, camera_domain_table_digest,
    seed_provenance_table_digest,
};
use fdgr_calibration::{
    DerivedCalibration, DerivedReadout, DistortionModel, ImageDerivation, PinholeIntrinsics,
    PixelConvention, ReadoutDirection, RigidTransform,
};
use fdgr_reprojection::{
    MAX_REPROJECTION_CALIBRATIONS, REPROJECTION_EVALUATION_SCHEMA, ReprojectionBasis,
    ReprojectionCalibration, ReprojectionEvaluation, ReprojectionPolicy, evaluate_reprojection,
    reprojection_algorithm_digest, reprojection_calibration_table_digest,
    reprojection_policy_digest,
};
use fdgr_types::EvidenceDigest;
use std::path::{Path, PathBuf};

const CAMERA_DOMAIN_HEADER: &str =
    "camera_node_id\tframe_digest\teffective_calibration_digest\timage_width\timage_height";
const SEED_PROVENANCE_HEADER: &str = "landmark_id\tsupport_observation_ids";
const CALIBRATION_HEADER: &str = "camera_node_id\tframe_digest\teffective_calibration_digest\tsource_calibration_digest\tsource_width\tsource_height\tcrop_x\tcrop_y\tcrop_width\tcrop_height\toutput_width\toutput_height\tfx_nano_pixels\tfy_nano_pixels\tcx_nano_pixels\tcy_nano_pixels\tdistortion_model\tk1_nano\tk2_nano\tp1_nano\tp2_nano\tk3_nano\trolling\treadout_direction\tfirst_observed_line_offset_ns\tobserved_readout_time_ns\treference_phase_nano\tr00\tr01\tr02\tr10\tr11\tr12\tr20\tr21\tr22\ttx_micrometers\tty_micrometers\ttz_micrometers\tdeclared_uncertainty_nano_pixels";
const MAX_CAMERA_DOMAIN_TABLE_BYTES: u64 = 64 * 1024 * 1024;
const MAX_SEED_PROVENANCE_TABLE_BYTES: u64 = 128 * 1024 * 1024;
const MAX_REPROJECTION_CALIBRATION_TABLE_BYTES: u64 = 256 * 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
struct Options {
    problem: BundleProblemPipelineOptions,
    camera_domain_path: PathBuf,
    camera_domain_file_digest: EvidenceDigest,
    seed_provenance_path: PathBuf,
    seed_provenance_file_digest: EvidenceDigest,
    calibration_path: PathBuf,
    calibration_file_digest: EvidenceDigest,
    admission_generation: u64,
    admission_policy: BundleAdmissionPolicy,
    reprojection_generation: u64,
    reprojection_policy: ReprojectionPolicy,
    format: OutputFormat,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "reprojection-evaluate")
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr reprojection-evaluate <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> <camera-bindings.tsv> <landmark-seeds.tsv> <bundle-observations.tsv> <camera-domains.tsv> <seed-provenance.tsv> <effective-calibrations.tsv> [exact upstream identities and gates] --camera-domain-file-digest <digest> --seed-provenance-file-digest <digest> --reprojection-calibration-file-digest <digest> --bundle-admission-generation <n> --reprojection-generation <n> [audit and reprojection gates] [--format text|json]"
    );
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let admission = build_bundle_admission(&options)?;
    let calibrations = read_calibrations(
        &options.calibration_path,
        &options.calibration_file_digest,
    )?;
    let bundle_admission_digest = admission
        .digest()
        .map_err(|error| format!("bundle-admission identity failed: {error}"))?;
    let calibration_table_digest = reprojection_calibration_table_digest(&calibrations)
        .map_err(|error| format!("reprojection calibration table rejected: {error}"))?;
    let algorithm_digest = reprojection_algorithm_digest()
        .map_err(|error| format!("reprojection algorithm identity failed: {error}"))?;
    let policy_digest = reprojection_policy_digest(options.reprojection_policy)
        .map_err(|error| format!("reprojection policy rejected: {error}"))?;
    let evaluation = evaluate_reprojection(
        ReprojectionBasis {
            bundle_admission_digest,
            calibration_table_digest,
            algorithm_digest,
            policy_digest,
            generation: options.reprojection_generation,
        },
        options.reprojection_policy,
        admission,
        calibrations,
    )
    .map_err(|error| format!("reprojection evaluation rejected: {error}"))?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            evaluation
                .to_json()
                .map_err(|error| format!("reprojection JSON rendering failed: {error}"))?
        ),
        OutputFormat::Text => print_text(&evaluation)?,
    }
    Ok(())
}

fn build_bundle_admission(options: &Options) -> Result<BundleAdmissionGeneration, String> {
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
    let policy_digest = bundle_admission_policy_digest(options.admission_policy)
        .map_err(|error| format!("bundle-admission policy rejected: {error}"))?;
    audit_bundle_admission(
        BundleAdmissionBasis {
            bundle_problem_digest,
            camera_domain_basis_digest,
            seed_provenance_basis_digest,
            policy_digest,
            generation: options.admission_generation,
        },
        options.admission_policy,
        problem,
        camera_domains,
        seed_provenance,
    )
    .map_err(|error| format!("bundle-admission audit rejected: {error}"))
}

include!("reprojection_cli_input.inc");
include!("reprojection_cli_render.inc");
include!("reprojection_cli_tests.inc");
