#![forbid(unsafe_code)]
#![allow(clippy::indexing_slicing, clippy::too_many_lines)]
//! Exact-byte CLI adapter for deterministic descriptor correspondence and track assembly.

use crate::args::OutputFormat;
use fdgr_codec::hash_bytes;
use fdgr_correspondence::{
    CorrespondenceBasis, Descriptor256, FeatureObservation, FramePair, MatchPolicy,
    MAX_FEATURE_OBSERVATIONS, MAX_FRAME_PAIRS, build_correspondences,
};
use fdgr_types::EvidenceDigest;
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path, PathBuf};

const OBSERVATION_HEADER: &str = "observation_id\tsample_index\tframe_digest\tfeature_id\tx_nano_pixels\ty_nano_pixels\tresponse_ppm\tuncertainty_nano_pixels\tdescriptor_hex\tdynamic_masked";
const PAIR_HEADER: &str = "pair_id\tleft_sample_index\tright_sample_index";
const MAX_OBSERVATION_TABLE_BYTES: u64 = 128 * 1024 * 1024;
const MAX_PAIR_TABLE_BYTES: u64 = 4 * 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
struct CorrespondenceCliOptions {
    observation_path: PathBuf,
    pair_path: PathBuf,
    keyframe_selection_digest: EvidenceDigest,
    calibration_digest: EvidenceDigest,
    feature_basis_digest: EvidenceDigest,
    pair_basis_digest: EvidenceDigest,
    policy_digest: EvidenceDigest,
    generation: u64,
    policy: MatchPolicy,
    format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct ObservationTable {
    digest: EvidenceDigest,
    observations: Vec<FeatureObservation>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct PairTable {
    digest: EvidenceDigest,
    pairs: Vec<FramePair>,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments
        .first()
        .is_some_and(|value| value == "correspondence-build")
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let observations = read_observation_table(
        &options.observation_path,
        &options.feature_basis_digest,
    )?;
    let pairs = read_pair_table(&options.pair_path, &options.pair_basis_digest)?;
    let generation = build_correspondences(
        CorrespondenceBasis {
            keyframe_selection_digest: options.keyframe_selection_digest,
            calibration_digest: options.calibration_digest,
            feature_basis_digest: observations.digest,
            pair_basis_digest: pairs.digest,
            policy_digest: options.policy_digest,
            generation: options.generation,
        },
        options.policy,
        observations.observations,
        pairs.pairs,
    )
    .map_err(|error| format!("correspondence build rejected: {error}"))?;
    match options.format {
        OutputFormat::Json => println!(
            "{}",
            generation
                .to_json()
                .map_err(|error| format!("correspondence JSON failed: {error}"))?
        ),
        OutputFormat::Text => {
            println!("schema\tfdgr.correspondence_generation/1");
            println!(
                "generation_digest\t{}",
                generation
                    .digest()
                    .map_err(|error| format!("correspondence identity failed: {error}"))?
            );
            println!("observation_count\t{}", generation.observations.len());
            println!("pair_count\t{}", generation.pairs.len());
            println!("accepted_match_count\t{}", generation.accepted_matches.len());
            println!("rejected_match_count\t{}", generation.rejected_matches.len());
            println!("track_count\t{}", generation.tracks.len());
            println!("distance_evaluations\t{}", generation.distance_evaluations);
            for track in &generation.tracks {
                println!(
                    "track\t{}\t{}\t{}\t{}",
                    track.track_id,
                    track.observation_ids.len(),
                    track.edge_count,
                    join_u64(&track.sample_indices)
                );
            }
            for rejection in &generation.rejected_matches {
                println!(
                    "rejected\t{}\t{}\t{}\t{}",
                    rejection.pair_id,
                    rejection.left_observation_id,
                    rejection
                        .candidate_right_observation_id
                        .map_or_else(|| "-".to_owned(), |value| value.to_string()),
                    rejection.reason.as_str()
                );
            }
        }
    }
    Ok(())
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr correspondence-build <observations.tsv> <pairs.tsv> --feature-basis-digest <digest> --pair-basis-digest <digest> --keyframe-selection-digest <digest> --calibration-digest <digest> --policy-digest <digest> --generation <n> [matching policy] [--format text|json]"
    );
}

fn parse(arguments: &[String]) -> Result<CorrespondenceCliOptions, String> {
    let usage = "usage: fdgr correspondence-build <observations.tsv> <pairs.tsv> --feature-basis-digest <digest> --pair-basis-digest <digest> --keyframe-selection-digest <digest> --calibration-digest <digest> --policy-digest <digest> --generation <n> [--max-hamming-distance n] [--ratio-threshold-ppm n] [--require-second-best true|false] [--require-mutual true|false] [--min-response-ppm n] [--max-uncertainty-nano-pixels n] [--reject-dynamic-masked true|false] [--max-distance-evaluations n] [--format text|json]";
    let mut values = arguments.iter();
    let observation_path = PathBuf::from(values.next().ok_or_else(|| usage.to_owned())?);
    let pair_path = PathBuf::from(values.next().ok_or_else(|| usage.to_owned())?);
    let mut keyframe_selection_digest = None;
    let mut calibration_digest = None;
    let mut feature_basis_digest = None;
    let mut pair_basis_digest = None;
    let mut policy_digest = None;
    let mut generation = None;
    let mut policy = MatchPolicy::default();
    let mut format = OutputFormat::Text;
    while let Some(flag) = values.next() {
        let value = values
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--keyframe-selection-digest" => {
                keyframe_selection_digest = Some(parse_digest(value, flag)?);
            }
            "--calibration-digest" => calibration_digest = Some(parse_digest(value, flag)?),
            "--feature-basis-digest" => feature_basis_digest = Some(parse_digest(value, flag)?),
            "--pair-basis-digest" => pair_basis_digest = Some(parse_digest(value, flag)?),
            "--policy-digest" => policy_digest = Some(parse_digest(value, flag)?),
            "--generation" => generation = Some(parse_nonzero_u64(value, "generation")?),
            "--max-hamming-distance" => {
                policy.max_hamming_distance = parse_u16(value, "maximum Hamming distance")?;
            }
            "--ratio-threshold-ppm" => {
                policy.ratio_threshold_ppm = parse_u32(value, "ratio threshold ppm")?;
            }
            "--require-second-best" => {
                policy.require_second_best = parse_bool(value, "require second best")?;
            }
            "--require-mutual" => policy.require_mutual = parse_bool(value, "require mutual")?,
            "--min-response-ppm" => {
                policy.min_response_ppm = parse_u32(value, "minimum response ppm")?;
            }
            "--max-uncertainty-nano-pixels" => {
                policy.max_uncertainty_nano_pixels =
                    parse_nonzero_u64(value, "maximum uncertainty nano-pixels")?;
            }
            "--reject-dynamic-masked" => {
                policy.reject_dynamic_masked = parse_bool(value, "reject dynamic masked")?;
            }
            "--max-distance-evaluations" => {
                policy.max_distance_evaluations =
                    parse_nonzero_u64(value, "maximum distance evaluations")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown correspondence-build option {flag:?}")),
        }
    }
    Ok(CorrespondenceCliOptions {
        observation_path,
        pair_path,
        keyframe_selection_digest: keyframe_selection_digest
            .ok_or_else(|| "missing --keyframe-selection-digest".to_owned())?,
        calibration_digest: calibration_digest
            .ok_or_else(|| "missing --calibration-digest".to_owned())?,
        feature_basis_digest: feature_basis_digest
            .ok_or_else(|| "missing --feature-basis-digest".to_owned())?,
        pair_basis_digest: pair_basis_digest
            .ok_or_else(|| "missing --pair-basis-digest".to_owned())?,
        policy_digest: policy_digest.ok_or_else(|| "missing --policy-digest".to_owned())?,
        generation: generation.ok_or_else(|| "missing --generation".to_owned())?,
        policy,
        format,
    })
}

fn read_observation_table(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<ObservationTable, String> {
    let bytes = read_bound_file(path, MAX_OBSERVATION_TABLE_BYTES, "feature observation")?;
    let digest = hash_bytes(&bytes);
    if &digest != expected_digest {
        return Err(format!(
            "feature observation basis digest mismatch: expected {expected_digest}, observed {digest}"
        ));
    }
    let text = parse_text(&bytes, "feature observation")?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "feature observation table is empty".to_owned())?;
    if header != OBSERVATION_HEADER {
        return Err(format!(
            "feature observation header mismatch: expected {OBSERVATION_HEADER:?}"
        ));
    }
    let mut observations = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("feature observation line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [observation_id, sample_index, frame_digest, feature_id, x, y, response, uncertainty, descriptor, dynamic_masked] = fields.as_slice() else {
            return Err(format!("feature observation line {line_number} must contain exactly ten tab-separated fields"));
        };
        observations.push(FeatureObservation {
            observation_id: parse_nonzero_u64(observation_id, "observation id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            sample_index: parse_u64(sample_index, "sample index")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            frame_digest: parse_digest(frame_digest, "frame digest")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            feature_id: parse_nonzero_u32(feature_id, "feature id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            x_nano_pixels: parse_i64(x, "x nano-pixels")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            y_nano_pixels: parse_i64(y, "y nano-pixels")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            response_ppm: parse_u32(response, "response ppm")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            uncertainty_nano_pixels: parse_u64(uncertainty, "uncertainty nano-pixels")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            descriptor: parse_descriptor(descriptor)
                .map_err(|error| format!("line {line_number}: {error}"))?,
            dynamic_masked: parse_bool(dynamic_masked, "dynamic masked")
                .map_err(|error| format!("line {line_number}: {error}"))?,
        });
        if observations.len() > MAX_FEATURE_OBSERVATIONS {
            return Err(format!(
                "feature observation table contains more than {MAX_FEATURE_OBSERVATIONS} records"
            ));
        }
    }
    if observations.is_empty() {
        return Err("feature observation table contains no records".to_owned());
    }
    Ok(ObservationTable {
        digest,
        observations,
    })
}

fn read_pair_table(path: &Path, expected_digest: &EvidenceDigest) -> Result<PairTable, String> {
    let bytes = read_bound_file(path, MAX_PAIR_TABLE_BYTES, "frame pair")?;
    let digest = hash_bytes(&bytes);
    if &digest != expected_digest {
        return Err(format!(
            "frame pair basis digest mismatch: expected {expected_digest}, observed {digest}"
        ));
    }
    let text = parse_text(&bytes, "frame pair")?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "frame pair table is empty".to_owned())?;
    if header != PAIR_HEADER {
        return Err(format!("frame pair header mismatch: expected {PAIR_HEADER:?}"));
    }
    let mut pairs = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("frame pair line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [pair_id, left_sample_index, right_sample_index] = fields.as_slice() else {
            return Err(format!("frame pair line {line_number} must contain exactly three tab-separated fields"));
        };
        pairs.push(FramePair {
            pair_id: parse_nonzero_u64(pair_id, "pair id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            left_sample_index: parse_u64(left_sample_index, "left sample index")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            right_sample_index: parse_u64(right_sample_index, "right sample index")
                .map_err(|error| format!("line {line_number}: {error}"))?,
        });
        if pairs.len() > MAX_FRAME_PAIRS {
            return Err(format!(
                "frame pair table contains more than {MAX_FRAME_PAIRS} records"
            ));
        }
    }
    if pairs.is_empty() {
        return Err("frame pair table contains no records".to_owned());
    }
    Ok(PairTable { digest, pairs })
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

fn parse_descriptor(value: &str) -> Result<Descriptor256, String> {
    if value.len() != 64
        || !value
            .bytes()
            .all(|byte| byte.is_ascii_digit() || matches!(byte, b'a'..=b'f'))
    {
        return Err("descriptor must be exactly 64 lowercase hexadecimal characters".to_owned());
    }
    let mut words = [0_u64; 4];
    for (index, slot) in words.iter_mut().enumerate() {
        let start = index.saturating_mul(16);
        let end = start.saturating_add(16);
        let segment = value
            .get(start..end)
            .ok_or_else(|| "descriptor segment is out of range".to_owned())?;
        *slot = u64::from_str_radix(segment, 16)
            .map_err(|error| format!("invalid descriptor segment: {error}"))?;
    }
    Ok(Descriptor256(words))
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

fn parse_u16(value: &str, label: &str) -> Result<u16, String> {
    value
        .parse::<u16>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn parse_i64(value: &str, label: &str) -> Result<i64, String> {
    value
        .parse::<i64>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn join_u64(values: &[u64]) -> String {
    values
        .iter()
        .map(u64::to_string)
        .collect::<Vec<_>>()
        .join(",")
}

#[cfg(test)]
mod tests {
    use super::{parse_descriptor, parse};
    use fdgr_correspondence::Descriptor256;

    #[test]
    fn descriptor_requires_canonical_lowercase_hex() {
        assert_eq!(
            parse_descriptor("0000000000000001000000000000000200000000000000030000000000000004"),
            Ok(Descriptor256([1, 2, 3, 4]))
        );
        assert!(parse_descriptor("ABCDEF").is_err());
    }

    #[test]
    fn required_basis_fields_are_not_optional() {
        assert!(parse(&["features.tsv".to_owned(), "pairs.tsv".to_owned()]).is_err());
    }
}
