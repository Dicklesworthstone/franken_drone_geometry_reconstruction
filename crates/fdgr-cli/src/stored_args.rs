#![forbid(unsafe_code)]
//! CLI arguments for custody-bound media inspection.

use crate::args::OutputFormat;
use fdgr_media::{ParseLimits, SampleWindowLimits, SampleWindowRequest};
use fdgr_types::EvidenceDigest;
use std::path::PathBuf;

const DEFAULT_STORED_SAMPLE_LIMIT: usize = 128;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct StoredMediaInspectOptions {
    pub(crate) store_root: PathBuf,
    pub(crate) manifest_digest: EvidenceDigest,
    pub(crate) limits: ParseLimits,
    pub(crate) format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct StoredMediaSamplesOptions {
    pub(crate) store_root: PathBuf,
    pub(crate) manifest_digest: EvidenceDigest,
    pub(crate) request: SampleWindowRequest,
    pub(crate) parse_limits: ParseLimits,
    pub(crate) window_limits: SampleWindowLimits,
    pub(crate) format: OutputFormat,
}

pub(crate) fn parse_stored_media_inspect(
    arguments: &[String],
) -> Result<StoredMediaInspectOptions, String> {
    let mut arguments = arguments.iter();
    let store_root = arguments.next().ok_or_else(|| {
        "usage: fdgr stored-media-inspect <store-root> <manifest-digest> [options]".to_owned()
    })?;
    let manifest_digest = parse_manifest_digest(arguments.next())?;
    let mut limits = ParseLimits::default();
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = next_value(&mut arguments, flag)?;
        if set_parse_limit(&mut limits, flag, value)? {
            continue;
        }
        match flag.as_str() {
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown stored-media-inspect option {flag:?}")),
        }
    }
    Ok(StoredMediaInspectOptions {
        store_root: PathBuf::from(store_root),
        manifest_digest,
        limits,
        format,
    })
}

pub(crate) fn parse_stored_media_samples(
    arguments: &[String],
) -> Result<StoredMediaSamplesOptions, String> {
    let mut arguments = arguments.iter();
    let store_root = arguments.next().ok_or_else(|| {
        "usage: fdgr stored-media-samples <store-root> <manifest-digest> --track-id n [options]"
            .to_owned()
    })?;
    let manifest_digest = parse_manifest_digest(arguments.next())?;
    let mut track_id = None;
    let mut start_sample = 0_u64;
    let mut sample_limit = DEFAULT_STORED_SAMPLE_LIMIT;
    let mut parse_limits = ParseLimits::default();
    let mut window_limits = SampleWindowLimits::default();
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = next_value(&mut arguments, flag)?;
        if set_parse_limit(&mut parse_limits, flag, value)? {
            continue;
        }
        match flag.as_str() {
            "--track-id" => track_id = Some(parse_u32(value, "track id")?),
            "--start-sample" => start_sample = parse_u64(value, "start sample")?,
            "--sample-limit" => sample_limit = parse_usize(value, "sample limit")?,
            "--max-window-records" => {
                window_limits.max_records = parse_usize(value, "maximum window records")?;
            }
            "--max-index-entries-scanned" => {
                window_limits.max_index_entries_scanned =
                    parse_u64(value, "maximum index entries scanned")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown stored-media-samples option {flag:?}")),
        }
    }
    let track_id = track_id.ok_or_else(|| "missing --track-id".to_owned())?;
    if track_id == 0 {
        return Err("track id must be nonzero".to_owned());
    }
    Ok(StoredMediaSamplesOptions {
        store_root: PathBuf::from(store_root),
        manifest_digest,
        request: SampleWindowRequest {
            track_id,
            start_sample,
            max_samples: sample_limit,
        },
        parse_limits,
        window_limits,
        format,
    })
}

fn parse_manifest_digest(value: Option<&String>) -> Result<EvidenceDigest, String> {
    let value = value.ok_or_else(|| "missing manifest digest".to_owned())?;
    EvidenceDigest::parse(value).map_err(|error| error.to_string())
}

fn next_value<'a>(
    arguments: &mut impl Iterator<Item = &'a String>,
    flag: &str,
) -> Result<&'a str, String> {
    arguments
        .next()
        .map(String::as_str)
        .ok_or_else(|| format!("missing value for {flag}"))
}

fn set_parse_limit(
    limits: &mut ParseLimits,
    flag: &str,
    value: &str,
) -> Result<bool, String> {
    match flag {
        "--max-boxes" => limits.max_boxes = parse_u64(value, "maximum box count")?,
        "--max-depth" => limits.max_depth = parse_usize(value, "maximum box depth")?,
        "--max-tracks" => limits.max_tracks = parse_usize(value, "maximum track count")?,
        "--max-table-entries" => {
            limits.max_table_entries = parse_u64(value, "maximum table entry count")?;
        }
        "--max-table-bytes" => {
            limits.max_table_bytes = parse_u64(value, "maximum table byte count")?;
        }
        "--max-compatible-brands" => {
            limits.max_compatible_brands = parse_usize(value, "maximum compatible brand count")?;
        }
        "--max-sample-descriptions" => {
            limits.max_sample_descriptions =
                parse_u64(value, "maximum sample description count")?;
        }
        _ => return Ok(false),
    }
    Ok(true)
}

fn parse_format(value: &str) -> Result<OutputFormat, String> {
    match value {
        "text" => Ok(OutputFormat::Text),
        "json" => Ok(OutputFormat::Json),
        _ => Err(format!("unknown output format {value:?}; expected text or json")),
    }
}

fn parse_u32(value: &str, label: &str) -> Result<u32, String> {
    value
        .parse::<u32>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn parse_u64(value: &str, label: &str) -> Result<u64, String> {
    value
        .parse::<u64>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn parse_usize(value: &str, label: &str) -> Result<usize, String> {
    value
        .parse::<usize>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

#[cfg(test)]
mod tests {
    use super::{parse_stored_media_inspect, parse_stored_media_samples};
    use crate::args::OutputFormat;

    const DIGEST: &str =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    #[test]
    fn stored_inspection_parses_identity_and_bounds() {
        let arguments = vec![
            "store".to_owned(),
            DIGEST.to_owned(),
            "--max-boxes".to_owned(),
            "44".to_owned(),
            "--format".to_owned(),
            "json".to_owned(),
        ];
        assert!(matches!(
            parse_stored_media_inspect(&arguments),
            Ok(ref value)
                if value.store_root.to_string_lossy() == "store"
                    && value.manifest_digest.as_str() == DIGEST
                    && value.limits.max_boxes == 44
                    && value.format == OutputFormat::Json
        ));
    }

    #[test]
    fn stored_samples_require_nonzero_track() {
        let missing = vec!["store".to_owned(), DIGEST.to_owned()];
        assert!(parse_stored_media_samples(&missing).is_err());
        let zero = vec![
            "store".to_owned(),
            DIGEST.to_owned(),
            "--track-id".to_owned(),
            "0".to_owned(),
        ];
        assert!(parse_stored_media_samples(&zero).is_err());
    }
}
