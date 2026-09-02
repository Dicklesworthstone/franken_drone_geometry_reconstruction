#![forbid(unsafe_code)]
#![allow(clippy::indexing_slicing, clippy::too_many_lines)]
//! Exact-byte CLI adapter for deterministic calibrated epipolar verification.

use crate::args::OutputFormat;
use fdgr_codec::hash_bytes;
use fdgr_epipolar::{
    CandidateSource, EpipolarBasis, EpipolarPolicy, EssentialCandidate,
    MAX_EPIPOLAR_CANDIDATES, MAX_EPIPOLAR_OBSERVATIONS, NormalizedCorrespondence,
    verify_epipolar_candidates,
};
use fdgr_types::EvidenceDigest;
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path, PathBuf};

const OBSERVATION_HEADER: &str = "match_id\tpair_id\tleft_observation_id\tright_observation_id\tleft_x_ppm\tleft_y_ppm\tright_x_ppm\tright_y_ppm\tuncertainty_ppm\tleft_spatial_bin\tright_spatial_bin";
const CANDIDATE_HEADER: &str =
    "candidate_id\tsource\tm00\tm01\tm02\tm10\tm11\tm12\tm20\tm21\tm22";
const MAX_OBSERVATION_TABLE_BYTES: u64 = 64 * 1024 * 1024;
const MAX_CANDIDATE_TABLE_BYTES: u64 = 4 * 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
struct CliOptions {
    observation_path: PathBuf,
    candidate_path: PathBuf,
    observation_basis_digest: EvidenceDigest,
    candidate_basis_digest: EvidenceDigest,
    correspondence_generation_digest: EvidenceDigest,
    calibration_digest: EvidenceDigest,
    policy_digest: EvidenceDigest,
    pair_id: u64,
    left_sample_index: u64,
    right_sample_index: u64,
    generation: u64,
    policy: EpipolarPolicy,
    format: OutputFormat,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "epipolar-verify")
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr epipolar-verify <observations.tsv> <candidates.tsv> --observation-basis-digest <digest> --candidate-basis-digest <digest> --correspondence-generation-digest <digest> --calibration-digest <digest> --policy-digest <digest> --pair-id <id> --left-sample-index <n> --right-sample-index <n> --generation <n> [verification gates] [--format text|json]"
    );
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse_options(arguments)?;
    let observations = read_observations(
        &options.observation_path,
        &options.observation_basis_digest,
    )?;
    let candidates = read_candidates(&options.candidate_path, &options.candidate_basis_digest)?;
    let verification = verify_epipolar_candidates(
        EpipolarBasis {
            correspondence_generation_digest: options.correspondence_generation_digest,
            calibration_digest: options.calibration_digest,
            observation_basis_digest: options.observation_basis_digest,
            candidate_basis_digest: options.candidate_basis_digest,
            policy_digest: options.policy_digest,
            pair_id: options.pair_id,
            left_sample_index: options.left_sample_index,
            right_sample_index: options.right_sample_index,
            generation: options.generation,
        },
        options.policy,
        observations,
        candidates,
    )
    .map_err(|error| format!("epipolar verification rejected: {error}"))?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            verification
                .to_json()
                .map_err(|error| format!("epipolar JSON rendering failed: {error}"))?
        ),
        OutputFormat::Text => {
            let digest = verification
                .digest()
                .map_err(|error| format!("epipolar identity failed: {error}"))?;
            println!("schema: fdgr.epipolar_verification/1");
            println!("verification_digest: {digest}");
            println!("decision: {}", verification.decision.as_str());
            println!("pair_id: {}", verification.basis.pair_id);
            println!("observations: {}", verification.observations.len());
            println!("candidates: {}", verification.candidates.len());
            println!("evaluations: {}", verification.evaluation_count);
            println!(
                "best_candidate_id: {}",
                optional_u64_text(verification.best_candidate_id)
            );
            println!(
                "runner_up_candidate_id: {}",
                optional_u64_text(verification.runner_up_candidate_id)
            );
            println!(
                "admitted_candidate_id: {}",
                optional_u64_text(verification.admitted_candidate_id)
            );
            println!("inlier_margin: {}", verification.inlier_margin);
            for evaluation in &verification.evaluations {
                println!(
                    "candidate {}: pass={} inliers={} ratio_ppm={} bins={}/{} median_residual_ppm={} determinant_residual_ppm={}",
                    evaluation.candidate_id,
                    evaluation.passes_admission_gates,
                    evaluation.inlier_count,
                    evaluation.inlier_ratio_ppm,
                    evaluation.left_spatial_bin_count,
                    evaluation.right_spatial_bin_count,
                    evaluation.median_inlier_residual_ppm,
                    evaluation.determinant_residual_ppm,
                );
            }
        }
    }
    Ok(())
}

fn parse_options(arguments: &[String]) -> Result<CliOptions, String> {
    let usage = "usage: fdgr epipolar-verify <observations.tsv> <candidates.tsv> --observation-basis-digest <digest> --candidate-basis-digest <digest> --correspondence-generation-digest <digest> --calibration-digest <digest> --policy-digest <digest> --pair-id <id> --left-sample-index <n> --right-sample-index <n> --generation <n> [--max-residual-ppm n] [--min-inliers n] [--min-inlier-ratio-ppm n] [--min-spatial-bins-per-image n] [--max-determinant-residual-ppm n] [--min-inlier-margin n] [--max-evaluations n] [--format text|json]";
    let Some(observation_path) = arguments.first() else {
        return Err(usage.to_owned());
    };
    let Some(candidate_path) = arguments.get(1) else {
        return Err(usage.to_owned());
    };
    let mut observation_basis_digest = None;
    let mut candidate_basis_digest = None;
    let mut correspondence_generation_digest = None;
    let mut calibration_digest = None;
    let mut policy_digest = None;
    let mut pair_id = None;
    let mut left_sample_index = None;
    let mut right_sample_index = None;
    let mut generation = None;
    let mut policy = EpipolarPolicy::default();
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
            "--observation-basis-digest" => {
                observation_basis_digest = Some(parse_digest(value, "observation basis digest")?);
            }
            "--candidate-basis-digest" => {
                candidate_basis_digest = Some(parse_digest(value, "candidate basis digest")?);
            }
            "--correspondence-generation-digest" => {
                correspondence_generation_digest =
                    Some(parse_digest(value, "correspondence generation digest")?);
            }
            "--calibration-digest" => {
                calibration_digest = Some(parse_digest(value, "calibration digest")?);
            }
            "--policy-digest" => policy_digest = Some(parse_digest(value, "policy digest")?),
            "--pair-id" => pair_id = Some(parse_nonzero_u64(value, "pair id")?),
            "--left-sample-index" => {
                left_sample_index = Some(parse_u64(value, "left sample index")?);
            }
            "--right-sample-index" => {
                right_sample_index = Some(parse_u64(value, "right sample index")?);
            }
            "--generation" => generation = Some(parse_nonzero_u64(value, "generation")?),
            "--max-residual-ppm" => {
                policy.max_residual_ppm = parse_nonzero_u64(value, "maximum residual ppm")?;
            }
            "--min-inliers" => policy.min_inliers = parse_nonzero_u32(value, "minimum inliers")?,
            "--min-inlier-ratio-ppm" => {
                policy.min_inlier_ratio_ppm =
                    parse_nonzero_u32(value, "minimum inlier ratio ppm")?;
            }
            "--min-spatial-bins-per-image" => {
                policy.min_spatial_bins_per_image =
                    parse_nonzero_u16(value, "minimum spatial bins per image")?;
            }
            "--max-determinant-residual-ppm" => {
                policy.max_determinant_residual_ppm =
                    parse_u64(value, "maximum determinant residual ppm")?;
            }
            "--min-inlier-margin" => {
                policy.min_inlier_margin = parse_u32(value, "minimum inlier margin")?;
            }
            "--max-evaluations" => {
                policy.max_evaluations = parse_nonzero_u64(value, "maximum evaluations")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown epipolar-verify option {flag:?}")),
        }
        position = position.saturating_add(2);
    }
    Ok(CliOptions {
        observation_path: PathBuf::from(observation_path),
        candidate_path: PathBuf::from(candidate_path),
        observation_basis_digest: observation_basis_digest
            .ok_or_else(|| "missing --observation-basis-digest".to_owned())?,
        candidate_basis_digest: candidate_basis_digest
            .ok_or_else(|| "missing --candidate-basis-digest".to_owned())?,
        correspondence_generation_digest: correspondence_generation_digest
            .ok_or_else(|| "missing --correspondence-generation-digest".to_owned())?,
        calibration_digest: calibration_digest
            .ok_or_else(|| "missing --calibration-digest".to_owned())?,
        policy_digest: policy_digest.ok_or_else(|| "missing --policy-digest".to_owned())?,
        pair_id: pair_id.ok_or_else(|| "missing --pair-id".to_owned())?,
        left_sample_index: left_sample_index
            .ok_or_else(|| "missing --left-sample-index".to_owned())?,
        right_sample_index: right_sample_index
            .ok_or_else(|| "missing --right-sample-index".to_owned())?,
        generation: generation.ok_or_else(|| "missing --generation".to_owned())?,
        policy,
        format,
    })
}

fn read_observations(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<NormalizedCorrespondence>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_OBSERVATION_TABLE_BYTES,
        "epipolar observation",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "epipolar observation table is empty".to_owned())?;
    if header != OBSERVATION_HEADER {
        return Err(format!(
            "epipolar observation header mismatch: expected {OBSERVATION_HEADER:?}"
        ));
    }
    let mut observations = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("epipolar observation line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [match_id, pair_id, left_observation_id, right_observation_id, left_x_ppm, left_y_ppm, right_x_ppm, right_y_ppm, uncertainty_ppm, left_spatial_bin, right_spatial_bin] = fields.as_slice() else {
            return Err(format!(
                "epipolar observation line {line_number} must contain exactly eleven tab-separated fields"
            ));
        };
        observations.push(NormalizedCorrespondence {
            match_id: parse_nonzero_u64(match_id, "match id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            pair_id: parse_nonzero_u64(pair_id, "pair id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            left_observation_id: parse_nonzero_u64(left_observation_id, "left observation id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            right_observation_id: parse_nonzero_u64(right_observation_id, "right observation id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            left_x_ppm: parse_i64(left_x_ppm, "left x ppm")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            left_y_ppm: parse_i64(left_y_ppm, "left y ppm")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            right_x_ppm: parse_i64(right_x_ppm, "right x ppm")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            right_y_ppm: parse_i64(right_y_ppm, "right y ppm")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            uncertainty_ppm: parse_u32(uncertainty_ppm, "uncertainty ppm")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            left_spatial_bin: parse_nonzero_u16(left_spatial_bin, "left spatial bin")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            right_spatial_bin: parse_nonzero_u16(right_spatial_bin, "right spatial bin")
                .map_err(|error| format!("line {line_number}: {error}"))?,
        });
        if observations.len() > MAX_EPIPOLAR_OBSERVATIONS {
            return Err(format!(
                "epipolar observation table contains more than {MAX_EPIPOLAR_OBSERVATIONS} records"
            ));
        }
    }
    if observations.is_empty() {
        return Err("epipolar observation table contains no records".to_owned());
    }
    Ok(observations)
}

fn read_candidates(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<EssentialCandidate>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_CANDIDATE_TABLE_BYTES,
        "essential candidate",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "essential candidate table is empty".to_owned())?;
    if header != CANDIDATE_HEADER {
        return Err(format!(
            "essential candidate header mismatch: expected {CANDIDATE_HEADER:?}"
        ));
    }
    let mut candidates = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("essential candidate line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [candidate_id, source, m00, m01, m02, m10, m11, m12, m20, m21, m22] =
            fields.as_slice()
        else {
            return Err(format!(
                "essential candidate line {line_number} must contain exactly eleven tab-separated fields"
            ));
        };
        candidates.push(EssentialCandidate {
            candidate_id: parse_nonzero_u64(candidate_id, "candidate id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            source: parse_source(source)
                .map_err(|error| format!("line {line_number}: {error}"))?,
            matrix: [
                parse_i64(m00, "m00"),
                parse_i64(m01, "m01"),
                parse_i64(m02, "m02"),
                parse_i64(m10, "m10"),
                parse_i64(m11, "m11"),
                parse_i64(m12, "m12"),
                parse_i64(m20, "m20"),
                parse_i64(m21, "m21"),
                parse_i64(m22, "m22"),
            ]
            .into_iter()
            .collect::<Result<Vec<_>, _>>()
            .and_then(|values| {
                values.try_into().map_err(|_| {
                    "essential candidate matrix did not contain exactly nine coefficients".to_owned()
                })
            })
            .map_err(|error| format!("line {line_number}: {error}"))?,
        });
        if candidates.len() > MAX_EPIPOLAR_CANDIDATES {
            return Err(format!(
                "essential candidate table contains more than {MAX_EPIPOLAR_CANDIDATES} records"
            ));
        }
    }
    if candidates.is_empty() {
        return Err("essential candidate table contains no records".to_owned());
    }
    Ok(candidates)
}

fn read_bound_text(
    path: &Path,
    expected_digest: &EvidenceDigest,
    maximum_bytes: u64,
    label: &str,
) -> Result<String, String> {
    let metadata = fs::symlink_metadata(path)
        .map_err(|error| format!("{label} metadata failed: {error}"))?;
    if metadata.file_type().is_symlink() {
        return Err(format!("{label} table must not be a symlink"));
    }
    if !metadata.is_file() {
        return Err(format!("{label} table must be a regular file"));
    }
    if metadata.len() > maximum_bytes {
        return Err(format!("{label} table exceeds {maximum_bytes} bytes"));
    }
    let mut file = File::open(path).map_err(|error| format!("{label} open failed: {error}"))?;
    let mut bytes = Vec::new();
    file.by_ref()
        .take(maximum_bytes.saturating_add(1))
        .read_to_end(&mut bytes)
        .map_err(|error| format!("{label} read failed: {error}"))?;
    let byte_length = u64::try_from(bytes.len()).map_err(|_| format!("{label} length overflow"))?;
    if byte_length > maximum_bytes {
        return Err(format!("{label} table exceeds {maximum_bytes} bytes"));
    }
    let observed_digest = hash_bytes(&bytes);
    if &observed_digest != expected_digest {
        return Err(format!(
            "{label} basis digest mismatch: expected {expected_digest}, observed {observed_digest}"
        ));
    }
    if !bytes.ends_with(b"\n") {
        return Err(format!("{label} table must end with a newline"));
    }
    let text = std::str::from_utf8(&bytes)
        .map_err(|error| format!("{label} table is not UTF-8: {error}"))?;
    if text.contains('\r') {
        return Err(format!("{label} table must use LF line endings"));
    }
    Ok(text.to_owned())
}

fn parse_source(value: &str) -> Result<CandidateSource, String> {
    match value {
        "native_minimal_solver" => Ok(CandidateSource::NativeMinimalSolver),
        "native_linear_solver" => Ok(CandidateSource::NativeLinearSolver),
        "telemetry_prior" => Ok(CandidateSource::TelemetryPrior),
        "model_worker" => Ok(CandidateSource::ModelWorker),
        "external_oracle" => Ok(CandidateSource::ExternalOracle),
        _ => Err(format!(
            "unknown candidate source {value:?}; expected native_minimal_solver, native_linear_solver, telemetry_prior, model_worker, or external_oracle"
        )),
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

fn parse_digest(value: &str, label: &str) -> Result<EvidenceDigest, String> {
    EvidenceDigest::parse(value).map_err(|error| format!("invalid {label}: {error}"))
}

fn parse_nonzero_u64(value: &str, label: &str) -> Result<u64, String> {
    let parsed = parse_u64(value, label)?;
    if parsed == 0 {
        Err(format!("{label} must be nonzero"))
    } else {
        Ok(parsed)
    }
}

fn parse_nonzero_u32(value: &str, label: &str) -> Result<u32, String> {
    let parsed = parse_u32(value, label)?;
    if parsed == 0 {
        Err(format!("{label} must be nonzero"))
    } else {
        Ok(parsed)
    }
}

fn parse_nonzero_u16(value: &str, label: &str) -> Result<u16, String> {
    let parsed = value
        .parse::<u16>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))?;
    if parsed == 0 {
        Err(format!("{label} must be nonzero"))
    } else {
        Ok(parsed)
    }
}

fn parse_u64(value: &str, label: &str) -> Result<u64, String> {
    value
        .parse::<u64>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn parse_u32(value: &str, label: &str) -> Result<u32, String> {
    value
        .parse::<u32>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn parse_i64(value: &str, label: &str) -> Result<i64, String> {
    value
        .parse::<i64>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn optional_u64_text(value: Option<u64>) -> String {
    value.map_or_else(|| "none".to_owned(), |number| number.to_string())
}
