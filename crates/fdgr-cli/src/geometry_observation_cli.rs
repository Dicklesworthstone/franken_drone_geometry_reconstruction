#![forbid(unsafe_code)]
#![allow(clippy::indexing_slicing, clippy::too_many_lines)]
//! Shared exact-byte parser for calibrated normalized correspondence evidence.

use fdgr_codec::hash_bytes;
use fdgr_epipolar::{MAX_EPIPOLAR_OBSERVATIONS, NormalizedCorrespondence};
use fdgr_types::EvidenceDigest;
use std::fs::{self, File};
use std::io::Read;
use std::path::Path;

const OBSERVATION_HEADER: &str = "match_id\tpair_id\tleft_observation_id\tright_observation_id\tleft_x_ppm\tleft_y_ppm\tright_x_ppm\tright_y_ppm\tuncertainty_ppm\tleft_spatial_bin\tright_spatial_bin";
const MAX_OBSERVATION_TABLE_BYTES: u64 = 64 * 1024 * 1024;

pub(crate) fn read_normalized_observations(
    path: &Path,
    expected_digest: &EvidenceDigest,
    label: &str,
) -> Result<Vec<NormalizedCorrespondence>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_OBSERVATION_TABLE_BYTES,
        label,
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| format!("{label} table is empty"))?;
    if header != OBSERVATION_HEADER {
        return Err(format!(
            "{label} header mismatch: expected {OBSERVATION_HEADER:?}"
        ));
    }
    let mut observations = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("{label} line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [match_id, pair_id, left_observation_id, right_observation_id, left_x_ppm, left_y_ppm, right_x_ppm, right_y_ppm, uncertainty_ppm, left_spatial_bin, right_spatial_bin] = fields.as_slice() else {
            return Err(format!(
                "{label} line {line_number} must contain exactly eleven tab-separated fields"
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
                "{label} table contains more than {MAX_EPIPOLAR_OBSERVATIONS} records"
            ));
        }
    }
    if observations.is_empty() {
        return Err(format!("{label} table contains no records"));
    }
    Ok(observations)
}

pub(crate) fn read_bound_text(
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

fn parse_nonzero_u64(value: &str, label: &str) -> Result<u64, String> {
    let parsed = value
        .parse::<u64>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))?;
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
