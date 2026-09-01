#![forbid(unsafe_code)]
//! CLI arguments for complete recorded-media publication and independent verification.

use crate::args::OutputFormat;
use fdgr_media::ParseLimits;
use fdgr_recorded_media::{
    DEFAULT_DERIVED_CHUNK_SIZE, DEFAULT_SOURCE_CHUNK_SIZE, RecordedMediaIngestOptions,
};
use fdgr_types::EvidenceDigest;
use std::path::PathBuf;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct RecordedMediaIngestCliOptions {
    pub(crate) store_root: PathBuf,
    pub(crate) source_path: PathBuf,
    pub(crate) ingest: RecordedMediaIngestOptions,
    pub(crate) format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct RecordedMediaVerifyCliOptions {
    pub(crate) store_root: PathBuf,
    pub(crate) root_manifest_digest: EvidenceDigest,
    pub(crate) format: OutputFormat,
}

pub(crate) fn parse_recorded_media_ingest(
    arguments: &[String],
) -> Result<RecordedMediaIngestCliOptions, String> {
    let mut arguments = arguments.iter();
    let usage = "usage: fdgr recorded-media-ingest <store-root> <source-path> [--source-chunk-size bytes] [--derived-chunk-size bytes] [bounded parser options] [--format text|json]";
    let store_root = arguments.next().ok_or_else(|| usage.to_owned())?;
    let source_path = arguments.next().ok_or_else(|| usage.to_owned())?;
    let mut ingest = RecordedMediaIngestOptions {
        source_chunk_size: DEFAULT_SOURCE_CHUNK_SIZE,
        derived_chunk_size: DEFAULT_DERIVED_CHUNK_SIZE,
        parse_limits: ParseLimits::default(),
    };
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = next_value(&mut arguments, flag)?;
        if set_parse_limit(&mut ingest.parse_limits, flag, value)? {
            continue;
        }
        match flag.as_str() {
            "--source-chunk-size" => {
                ingest.source_chunk_size = parse_nonzero_u32(value, "source chunk size")?;
            }
            "--derived-chunk-size" => {
                ingest.derived_chunk_size = parse_nonzero_u32(value, "derived chunk size")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown recorded-media-ingest option {flag:?}")),
        }
    }
    Ok(RecordedMediaIngestCliOptions {
        store_root: PathBuf::from(store_root),
        source_path: PathBuf::from(source_path),
        ingest,
        format,
    })
}

pub(crate) fn parse_recorded_media_verify(
    arguments: &[String],
) -> Result<RecordedMediaVerifyCliOptions, String> {
    let mut arguments = arguments.iter();
    let usage = "usage: fdgr recorded-media-verify <store-root> <root-manifest-digest> [--format text|json]";
    let store_root = arguments.next().ok_or_else(|| usage.to_owned())?;
    let root_manifest_digest = arguments.next().ok_or_else(|| usage.to_owned())?;
    let root_manifest_digest =
        EvidenceDigest::parse(root_manifest_digest).map_err(|error| error.to_string())?;
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = next_value(&mut arguments, flag)?;
        match flag.as_str() {
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown recorded-media-verify option {flag:?}")),
        }
    }
    Ok(RecordedMediaVerifyCliOptions {
        store_root: PathBuf::from(store_root),
        root_manifest_digest,
        format,
    })
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

fn parse_nonzero_u32(value: &str, label: &str) -> Result<u32, String> {
    let parsed = value
        .parse::<u32>()
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

fn parse_usize(value: &str, label: &str) -> Result<usize, String> {
    value
        .parse::<usize>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

#[cfg(test)]
mod tests {
    use super::{parse_recorded_media_ingest, parse_recorded_media_verify};
    use crate::args::OutputFormat;

    const DIGEST: &str =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    #[test]
    fn ingest_parses_independent_chunk_and_parser_bounds() {
        let arguments = vec![
            "store".to_owned(),
            "flight.mp4".to_owned(),
            "--source-chunk-size".to_owned(),
            "4096".to_owned(),
            "--derived-chunk-size".to_owned(),
            "512".to_owned(),
            "--max-boxes".to_owned(),
            "99".to_owned(),
            "--max-tracks".to_owned(),
            "4".to_owned(),
            "--format".to_owned(),
            "json".to_owned(),
        ];
        assert!(matches!(
            parse_recorded_media_ingest(&arguments),
            Ok(ref value)
                if value.store_root.to_string_lossy() == "store"
                    && value.source_path.to_string_lossy() == "flight.mp4"
                    && value.ingest.source_chunk_size == 4096
                    && value.ingest.derived_chunk_size == 512
                    && value.ingest.parse_limits.max_boxes == 99
                    && value.ingest.parse_limits.max_tracks == 4
                    && value.format == OutputFormat::Json
        ));
    }

    #[test]
    fn ingest_rejects_zero_chunk_sizes() {
        let source_zero = vec![
            "store".to_owned(),
            "flight.mp4".to_owned(),
            "--source-chunk-size".to_owned(),
            "0".to_owned(),
        ];
        let derived_zero = vec![
            "store".to_owned(),
            "flight.mp4".to_owned(),
            "--derived-chunk-size".to_owned(),
            "0".to_owned(),
        ];
        assert!(parse_recorded_media_ingest(&source_zero).is_err());
        assert!(parse_recorded_media_ingest(&derived_zero).is_err());
    }

    #[test]
    fn verify_requires_a_canonical_root_manifest_identity() {
        let valid = vec![
            "store".to_owned(),
            DIGEST.to_owned(),
            "--format".to_owned(),
            "json".to_owned(),
        ];
        assert!(matches!(
            parse_recorded_media_verify(&valid),
            Ok(ref value)
                if value.root_manifest_digest.as_str() == DIGEST
                    && value.format == OutputFormat::Json
        ));
        let invalid = vec!["store".to_owned(), "not-a-digest".to_owned()];
        assert!(parse_recorded_media_verify(&invalid).is_err());
    }
}
