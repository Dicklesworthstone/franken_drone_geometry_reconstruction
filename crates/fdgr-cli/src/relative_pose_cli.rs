#![forbid(unsafe_code)]
#![allow(clippy::indexing_slicing, clippy::too_many_lines)]
//! Exact-byte CLI adapter for deterministic two-view relative-pose candidate verification.

use crate::args::OutputFormat;
use fdgr_codec::hash_bytes;
use fdgr_relative_pose::{
    BearingMatch, MAX_BEARING_MATCHES, MAX_POSE_CANDIDATES, PoseCandidateSource,
    RelativePoseBasis, RelativePoseCandidate, RelativePosePolicy, verify_relative_pose,
};
use fdgr_types::EvidenceDigest;
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path, PathBuf};

const BEARING_HEADER: &str = "match_id\tleft_observation_id\tright_observation_id\tleft_bx_nano\tleft_by_nano\tleft_bz_nano\tright_bx_nano\tright_by_nano\tright_bz_nano\tuncertainty_nano";
const CANDIDATE_HEADER: &str = "candidate_id\tevidence_digest\tsource\tr00\tr01\tr02\tr10\tr11\tr12\tr20\tr21\tr22\ttx\tty\ttz";
const MAX_BEARING_TABLE_BYTES: u64 = 64 * 1024 * 1024;
const MAX_CANDIDATE_TABLE_BYTES: u64 = 4 * 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
struct RelativePoseCliOptions {
    bearing_path: PathBuf,
    candidate_path: PathBuf,
    correspondence_generation_digest: EvidenceDigest,
    calibration_digest: EvidenceDigest,
    bearing_basis_digest: EvidenceDigest,
    candidate_basis_digest: EvidenceDigest,
    policy_digest: EvidenceDigest,
    left_sample_index: u64,
    right_sample_index: u64,
    generation: u64,
    policy: RelativePosePolicy,
    format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct BearingTable {
    digest: EvidenceDigest,
    matches: Vec<BearingMatch>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct CandidateTable {
    digest: EvidenceDigest,
    candidates: Vec<RelativePoseCandidate>,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "relative-pose-verify")
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let bearings = read_bearing_table(&options.bearing_path, &options.bearing_basis_digest)?;
    let candidates = read_candidate_table(
        &options.candidate_path,
        &options.candidate_basis_digest,
    )?;
    let verification = verify_relative_pose(
        RelativePoseBasis {
            correspondence_generation_digest: options.correspondence_generation_digest,
            calibration_digest: options.calibration_digest,
            bearing_basis_digest: bearings.digest,
            candidate_basis_digest: candidates.digest,
            policy_digest: options.policy_digest,
            left_sample_index: options.left_sample_index,
            right_sample_index: options.right_sample_index,
            generation: options.generation,
        },
        options.policy,
        bearings.matches,
        candidates.candidates,
    )
    .map_err(|error| format!("relative-pose verification rejected: {error}"))?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            verification
                .to_json()
                .map_err(|error| format!("relative-pose JSON failed: {error}"))?
        ),
        OutputFormat::Text => {
            println!("schema\tfdgr.relative_pose_verification/1");
            println!(
                "verification_digest\t{}",
                verification
                    .digest()
                    .map_err(|error| format!("relative-pose identity failed: {error}"))?
            );
            println!("status\t{}", verification.status.as_str());
            println!(
                "selected_candidate_id\t{}",
                verification
                    .selected_candidate_id
                    .map_or_else(|| "-".to_owned(), |value| value.to_string())
            );
            println!("match_count\t{}", verification.matches.len());
            println!("candidate_count\t{}", verification.candidates.len());
            println!("evaluation_count\t{}", verification.evaluation_count);
            for evaluation in &verification.evaluations {
                println!(
                    "candidate\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
                    evaluation.candidate_id,
                    evaluation.source.as_str(),
                    evaluation.accepted,
                    evaluation.inlier_count,
                    evaluation.inlier_ratio_ppm,
                    evaluation.positive_depth_ratio_ppm,
                    evaluation.median_inlier_residual_nano
                );
            }
        }
    }
    Ok(())
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr relative-pose-verify <bearings.tsv> <candidates.tsv> --bearing-basis-digest <digest> --candidate-basis-digest <digest> --correspondence-generation-digest <digest> --calibration-digest <digest> --policy-digest <digest> --left-sample-index <n> --right-sample-index <n> --generation <n> [verification policy] [--format text|json]"
    );
}

fn parse(arguments: &[String]) -> Result<RelativePoseCliOptions, String> {
    let usage = "usage: fdgr relative-pose-verify <bearings.tsv> <candidates.tsv> --bearing-basis-digest <digest> --candidate-basis-digest <digest> --correspondence-generation-digest <digest> --calibration-digest <digest> --policy-digest <digest> --left-sample-index <n> --right-sample-index <n> --generation <n> [--max-epipolar-residual-nano n] [--min-epipolar-normal-nano n] [--min-parallax-nano n] [--min-inlier-matches n] [--min-inlier-ratio-ppm n] [--min-positive-depth-ratio-ppm n] [--max-median-residual-nano n] [--require-cheirality true|false] [--max-evaluations n] [--format text|json]";
    let mut values = arguments.iter();
    let bearing_path = PathBuf::from(values.next().ok_or_else(|| usage.to_owned())?);
    let candidate_path = PathBuf::from(values.next().ok_or_else(|| usage.to_owned())?);
    let mut correspondence_generation_digest = None;
    let mut calibration_digest = None;
    let mut bearing_basis_digest = None;
    let mut candidate_basis_digest = None;
    let mut policy_digest = None;
    let mut left_sample_index = None;
    let mut right_sample_index = None;
    let mut generation = None;
    let mut policy = RelativePosePolicy::default();
    let mut format = OutputFormat::Text;
    while let Some(flag) = values.next() {
        let value = values
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--correspondence-generation-digest" => {
                correspondence_generation_digest = Some(parse_digest(value, flag)?);
            }
            "--calibration-digest" => calibration_digest = Some(parse_digest(value, flag)?),
            "--bearing-basis-digest" => bearing_basis_digest = Some(parse_digest(value, flag)?),
            "--candidate-basis-digest" => {
                candidate_basis_digest = Some(parse_digest(value, flag)?);
            }
            "--policy-digest" => policy_digest = Some(parse_digest(value, flag)?),
            "--left-sample-index" => {
                left_sample_index = Some(parse_u64(value, "left sample index")?);
            }
            "--right-sample-index" => {
                right_sample_index = Some(parse_u64(value, "right sample index")?);
            }
            "--generation" => generation = Some(parse_nonzero_u64(value, "generation")?),
            "--max-epipolar-residual-nano" => {
                policy.max_epipolar_residual_nano =
                    parse_u64(value, "maximum epipolar residual nano")?;
            }
            "--min-epipolar-normal-nano" => {
                policy.min_epipolar_normal_nano =
                    parse_nonzero_u64(value, "minimum epipolar normal nano")?;
            }
            "--min-parallax-nano" => {
                policy.min_parallax_nano = parse_u64(value, "minimum parallax nano")?;
            }
            "--min-inlier-matches" => {
                policy.min_inlier_matches = parse_nonzero_u32(value, "minimum inlier matches")?;
            }
            "--min-inlier-ratio-ppm" => {
                policy.min_inlier_ratio_ppm = parse_u32(value, "minimum inlier ratio ppm")?;
            }
            "--min-positive-depth-ratio-ppm" => {
                policy.min_positive_depth_ratio_ppm =
                    parse_u32(value, "minimum positive-depth ratio ppm")?;
            }
            "--max-median-residual-nano" => {
                policy.max_median_residual_nano =
                    parse_u64(value, "maximum median residual nano")?;
            }
            "--require-cheirality" => {
                policy.require_cheirality = parse_bool(value, "require cheirality")?;
            }
            "--max-evaluations" => {
                policy.max_evaluations = parse_nonzero_u64(value, "maximum evaluations")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown relative-pose-verify option {flag:?}")),
        }
    }
    Ok(RelativePoseCliOptions {
        bearing_path,
        candidate_path,
        correspondence_generation_digest: correspondence_generation_digest
            .ok_or_else(|| "missing --correspondence-generation-digest".to_owned())?,
        calibration_digest: calibration_digest
            .ok_or_else(|| "missing --calibration-digest".to_owned())?,
        bearing_basis_digest: bearing_basis_digest
            .ok_or_else(|| "missing --bearing-basis-digest".to_owned())?,
        candidate_basis_digest: candidate_basis_digest
            .ok_or_else(|| "missing --candidate-basis-digest".to_owned())?,
        policy_digest: policy_digest.ok_or_else(|| "missing --policy-digest".to_owned())?,
        left_sample_index: left_sample_index
            .ok_or_else(|| "missing --left-sample-index".to_owned())?,
        right_sample_index: right_sample_index
            .ok_or_else(|| "missing --right-sample-index".to_owned())?,
        generation: generation.ok_or_else(|| "missing --generation".to_owned())?,
        policy,
        format,
    })
}

fn read_bearing_table(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<BearingTable, String> {
    let bytes = read_bound_file(path, MAX_BEARING_TABLE_BYTES, "bearing match")?;
    let digest = hash_bytes(&bytes);
    if &digest != expected_digest {
        return Err(format!(
            "bearing basis digest mismatch: expected {expected_digest}, observed {digest}"
        ));
    }
    let text = parse_text(&bytes, "bearing match")?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "bearing table is empty".to_owned())?;
    if header != BEARING_HEADER {
        return Err(format!("bearing header mismatch: expected {BEARING_HEADER:?}"));
    }
    let mut matches = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("bearing line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [match_id, left_observation_id, right_observation_id, lbx, lby, lbz, rbx, rby, rbz, uncertainty] = fields.as_slice() else {
            return Err(format!("bearing line {line_number} must contain exactly ten tab-separated fields"));
        };
        matches.push(BearingMatch {
            match_id: parse_nonzero_u64(match_id, "match id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            left_observation_id: parse_nonzero_u64(left_observation_id, "left observation id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            right_observation_id: parse_nonzero_u64(right_observation_id, "right observation id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            left_bearing_nano: [
                parse_i64(lbx, "left bx")?,
                parse_i64(lby, "left by")?,
                parse_i64(lbz, "left bz")?,
            ],
            right_bearing_nano: [
                parse_i64(rbx, "right bx")?,
                parse_i64(rby, "right by")?,
                parse_i64(rbz, "right bz")?,
            ],
            uncertainty_nano: parse_u64(uncertainty, "uncertainty nano")?,
        });
        if matches.len() > MAX_BEARING_MATCHES {
            return Err(format!(
                "bearing table contains more than {MAX_BEARING_MATCHES} records"
            ));
        }
    }
    if matches.is_empty() {
        return Err("bearing table contains no records".to_owned());
    }
    Ok(BearingTable { digest, matches })
}

fn read_candidate_table(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<CandidateTable, String> {
    let bytes = read_bound_file(path, MAX_CANDIDATE_TABLE_BYTES, "pose candidate")?;
    let digest = hash_bytes(&bytes);
    if &digest != expected_digest {
        return Err(format!(
            "pose candidate basis digest mismatch: expected {expected_digest}, observed {digest}"
        ));
    }
    let text = parse_text(&bytes, "pose candidate")?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "pose candidate table is empty".to_owned())?;
    if header != CANDIDATE_HEADER {
        return Err(format!(
            "pose candidate header mismatch: expected {CANDIDATE_HEADER:?}"
        ));
    }
    let mut candidates = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("pose candidate line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [candidate_id, evidence_digest, source, r00, r01, r02, r10, r11, r12, r20, r21, r22, tx, ty, tz] = fields.as_slice() else {
            return Err(format!("pose candidate line {line_number} must contain exactly fifteen tab-separated fields"));
        };
        candidates.push(RelativePoseCandidate {
            candidate_id: parse_nonzero_u64(candidate_id, "candidate id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            evidence_digest: parse_digest(evidence_digest, "candidate evidence digest")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            source: parse_source(source)
                .map_err(|error| format!("line {line_number}: {error}"))?,
            rotation_left_to_right_nano: [
                parse_i64(r00, "r00")?,
                parse_i64(r01, "r01")?,
                parse_i64(r02, "r02")?,
                parse_i64(r10, "r10")?,
                parse_i64(r11, "r11")?,
                parse_i64(r12, "r12")?,
                parse_i64(r20, "r20")?,
                parse_i64(r21, "r21")?,
                parse_i64(r22, "r22")?,
            ],
            translation_left_origin_in_right_nano: [
                parse_i64(tx, "tx")?,
                parse_i64(ty, "ty")?,
                parse_i64(tz, "tz")?,
            ],
        });
        if candidates.len() > MAX_POSE_CANDIDATES {
            return Err(format!(
                "pose candidate table contains more than {MAX_POSE_CANDIDATES} records"
            ));
        }
    }
    if candidates.is_empty() {
        return Err("pose candidate table contains no records".to_owned());
    }
    Ok(CandidateTable { digest, candidates })
}

fn read_bound_file(path: &Path, maximum: u64, label: &str) -> Result<Vec<u8>, String> {
    let metadata = fs::symlink_metadata(path)
        .map_err(|error| format!("{label} metadata failed: {error}"))?;
    if metadata.file_type().is_symlink() {
        return Err(format!("{label} table must not be a symlink"));
    }
    if !metadata.is_file() {
        return Err(format!("{label} table must be a regular file"));
    }
    let mut file = File::open(path).map_err(|error| format!("{label} open failed: {error}"))?;
    let mut bytes = Vec::new();
    file.by_ref()
        .take(maximum.saturating_add(1))
        .read_to_end(&mut bytes)
        .map_err(|error| format!("{label} read failed: {error}"))?;
    if u64::try_from(bytes.len()).map_or(true, |length| length > maximum) {
        return Err(format!("{label} table exceeds {maximum} bytes"));
    }
    Ok(bytes)
}

fn parse_text<'a>(bytes: &'a [u8], label: &str) -> Result<&'a str, String> {
    let text = std::str::from_utf8(bytes)
        .map_err(|error| format!("{label} table is not UTF-8: {error}"))?;
    if text.contains('\r') {
        return Err(format!("{label} table must use LF line endings"));
    }
    Ok(text)
}

fn parse_source(value: &str) -> Result<PoseCandidateSource, String> {
    match value {
        "five_point" => Ok(PoseCandidateSource::FivePoint),
        "eight_point" => Ok(PoseCandidateSource::EightPoint),
        "telemetry_prior" => Ok(PoseCandidateSource::TelemetryPrior),
        "model_prior" => Ok(PoseCandidateSource::ModelPrior),
        "diagnostic_hypothesis" => Ok(PoseCandidateSource::DiagnosticHypothesis),
        _ => Err(format!("unknown pose candidate source {value:?}")),
    }
}

fn parse_digest(value: &str, label: &str) -> Result<EvidenceDigest, String> {
    EvidenceDigest::parse(value).map_err(|error| format!("invalid {label}: {error}"))
}

fn parse_format(value: &str) -> Result<OutputFormat, String> {
    match value {
        "text" => Ok(OutputFormat::Text),
        "json" => Ok(OutputFormat::Json),
        _ => Err(format!("unknown output format {value:?}; expected text or json")),
    }
}

fn parse_bool(value: &str, label: &str) -> Result<bool, String> {
    match value {
        "true" => Ok(true),
        "false" => Ok(false),
        _ => Err(format!("invalid {label} {value:?}; expected true or false")),
    }
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

#[cfg(test)]
mod tests {
    use super::{parse, parse_source};
    use fdgr_relative_pose::PoseCandidateSource;

    #[test]
    fn source_vocabulary_is_closed() {
        assert_eq!(
            parse_source("five_point"),
            Ok(PoseCandidateSource::FivePoint)
        );
        assert!(parse_source("trusted_vendor_pose").is_err());
    }

    #[test]
    fn required_basis_fields_are_not_optional() {
        assert!(parse(&["bearings.tsv".to_owned(), "candidates.tsv".to_owned()]).is_err());
    }
}
