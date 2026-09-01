#![forbid(unsafe_code)]
//! Deterministic CLI argument parsing without a third-party command framework.

use fdgr_evidence::DEFAULT_CHUNK_SIZE;
use fdgr_media::ParseLimits;
use fdgr_types::EvidenceDigest;
use std::path::PathBuf;

pub(crate) const DEFAULT_CHUNK_VIEW_LIMIT: usize = 32;
pub(crate) const MAX_CHUNK_VIEW_LIMIT: usize = 4096;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub(crate) enum OutputFormat {
    Text,
    Json,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct ManifestViewOptions {
    pub(crate) path: PathBuf,
    pub(crate) chunk_size: u32,
    pub(crate) chunk_offset: usize,
    pub(crate) chunk_limit: usize,
    pub(crate) format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct VerifyOptions {
    pub(crate) path: PathBuf,
    pub(crate) chunk_size: u32,
    pub(crate) object_digest: EvidenceDigest,
    pub(crate) manifest_digest: EvidenceDigest,
    pub(crate) format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct ImportOptions {
    pub(crate) store_root: PathBuf,
    pub(crate) path: PathBuf,
    pub(crate) chunk_size: u32,
    pub(crate) format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct StoreVerifyOptions {
    pub(crate) store_root: PathBuf,
    pub(crate) manifest_digest: EvidenceDigest,
    pub(crate) format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct MediaInspectOptions {
    pub(crate) path: PathBuf,
    pub(crate) limits: ParseLimits,
    pub(crate) format: OutputFormat,
}

pub(crate) fn parse_format(arguments: &[String]) -> Result<OutputFormat, String> {
    match arguments {
        [] => Ok(OutputFormat::Text),
        [flag, value] if flag == "--format" => parse_format_value(value),
        _ => Err("expected no arguments or `--format text|json`".to_owned()),
    }
}

pub(crate) fn parse_manifest_view(
    arguments: &[String],
) -> Result<ManifestViewOptions, String> {
    let mut arguments = arguments.iter();
    let path = arguments
        .next()
        .ok_or_else(|| "usage: fdgr file-manifest <path> [options]".to_owned())?;
    let mut options = ManifestViewOptions {
        path: PathBuf::from(path),
        chunk_size: DEFAULT_CHUNK_SIZE,
        chunk_offset: 0,
        chunk_limit: DEFAULT_CHUNK_VIEW_LIMIT,
        format: OutputFormat::Text,
    };
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--chunk-size" => options.chunk_size = parse_u32(value, "chunk size")?,
            "--chunk-offset" => options.chunk_offset = parse_usize(value, "chunk offset")?,
            "--chunk-limit" => {
                options.chunk_limit = parse_usize(value, "chunk limit")?;
                if options.chunk_limit > MAX_CHUNK_VIEW_LIMIT {
                    return Err(format!(
                        "chunk limit {} exceeds maximum {MAX_CHUNK_VIEW_LIMIT}",
                        options.chunk_limit
                    ));
                }
            }
            "--format" => options.format = parse_format_value(value)?,
            _ => return Err(format!("unknown file-manifest option {flag:?}")),
        }
    }
    Ok(options)
}

pub(crate) fn parse_verify(arguments: &[String]) -> Result<VerifyOptions, String> {
    let mut arguments = arguments.iter();
    let path = arguments.next().ok_or_else(|| {
        "usage: fdgr verify-file <path> --object-digest <digest> --manifest-digest <digest> [options]"
            .to_owned()
    })?;
    let mut chunk_size = DEFAULT_CHUNK_SIZE;
    let mut object_digest = None;
    let mut manifest_digest = None;
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--chunk-size" => chunk_size = parse_u32(value, "chunk size")?,
            "--object-digest" => {
                object_digest = Some(
                    EvidenceDigest::parse(value).map_err(|error| error.to_string())?,
                );
            }
            "--manifest-digest" => {
                manifest_digest = Some(
                    EvidenceDigest::parse(value).map_err(|error| error.to_string())?,
                );
            }
            "--format" => format = parse_format_value(value)?,
            _ => return Err(format!("unknown verify-file option {flag:?}")),
        }
    }
    Ok(VerifyOptions {
        path: PathBuf::from(path),
        chunk_size,
        object_digest: object_digest.ok_or_else(|| "missing --object-digest".to_owned())?,
        manifest_digest: manifest_digest.ok_or_else(|| "missing --manifest-digest".to_owned())?,
        format,
    })
}

pub(crate) fn parse_import(arguments: &[String]) -> Result<ImportOptions, String> {
    let mut arguments = arguments.iter();
    let store_root = arguments.next().ok_or_else(|| {
        "usage: fdgr import-file <store-root> <path> [--chunk-size bytes] [--format text|json]"
            .to_owned()
    })?;
    let path = arguments.next().ok_or_else(|| {
        "usage: fdgr import-file <store-root> <path> [--chunk-size bytes] [--format text|json]"
            .to_owned()
    })?;
    let mut chunk_size = DEFAULT_CHUNK_SIZE;
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--chunk-size" => chunk_size = parse_u32(value, "chunk size")?,
            "--format" => format = parse_format_value(value)?,
            _ => return Err(format!("unknown import-file option {flag:?}")),
        }
    }
    Ok(ImportOptions {
        store_root: PathBuf::from(store_root),
        path: PathBuf::from(path),
        chunk_size,
        format,
    })
}

pub(crate) fn parse_store_verify(
    arguments: &[String],
) -> Result<StoreVerifyOptions, String> {
    let mut arguments = arguments.iter();
    let store_root = arguments.next().ok_or_else(|| {
        "usage: fdgr verify-store <store-root> <manifest-digest> [--format text|json]"
            .to_owned()
    })?;
    let manifest_digest = arguments.next().ok_or_else(|| {
        "usage: fdgr verify-store <store-root> <manifest-digest> [--format text|json]"
            .to_owned()
    })?;
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--format" => format = parse_format_value(value)?,
            _ => return Err(format!("unknown verify-store option {flag:?}")),
        }
    }
    Ok(StoreVerifyOptions {
        store_root: PathBuf::from(store_root),
        manifest_digest: EvidenceDigest::parse(manifest_digest)
            .map_err(|error| error.to_string())?,
        format,
    })
}

pub(crate) fn parse_media_inspect(
    arguments: &[String],
) -> Result<MediaInspectOptions, String> {
    let mut arguments = arguments.iter();
    let path = arguments.next().ok_or_else(|| {
        "usage: fdgr media-inspect <path> [bounded options] [--format text|json]".to_owned()
    })?;
    let mut limits = ParseLimits::default();
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
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
                limits.max_compatible_brands =
                    parse_usize(value, "maximum compatible brand count")?;
            }
            "--max-sample-descriptions" => {
                limits.max_sample_descriptions =
                    parse_u64(value, "maximum sample description count")?;
            }
            "--format" => format = parse_format_value(value)?,
            _ => return Err(format!("unknown media-inspect option {flag:?}")),
        }
    }
    Ok(MediaInspectOptions {
        path: PathBuf::from(path),
        limits,
        format,
    })
}

fn parse_format_value(value: &str) -> Result<OutputFormat, String> {
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
    use super::{
        OutputFormat, parse_import, parse_manifest_view, parse_media_inspect, parse_store_verify,
        parse_verify,
    };

    #[test]
    fn manifest_options_are_order_independent() {
        let arguments = vec![
            "file.mp4".to_owned(),
            "--format".to_owned(),
            "json".to_owned(),
            "--chunk-limit".to_owned(),
            "7".to_owned(),
            "--chunk-offset".to_owned(),
            "3".to_owned(),
            "--chunk-size".to_owned(),
            "1024".to_owned(),
        ];
        assert!(matches!(
            parse_manifest_view(&arguments),
            Ok(ref value)
                if value.path.to_string_lossy() == "file.mp4"
                    && value.format == OutputFormat::Json
                    && value.chunk_limit == 7
                    && value.chunk_offset == 3
                    && value.chunk_size == 1024
        ));
    }

    #[test]
    fn verify_requires_both_identities() {
        let arguments = vec!["file.mp4".to_owned()];
        assert!(parse_verify(&arguments).is_err());
    }

    #[test]
    fn import_requires_store_and_source() {
        assert!(parse_import(&[]).is_err());
        assert!(parse_import(&["store".to_owned()]).is_err());
    }

    #[test]
    fn store_verify_rejects_noncanonical_digest() {
        let arguments = vec!["store".to_owned(), "not-a-digest".to_owned()];
        assert!(parse_store_verify(&arguments).is_err());
    }

    #[test]
    fn media_inspect_exposes_hard_bounds() {
        let arguments = vec![
            "flight.mp4".to_owned(),
            "--max-boxes".to_owned(),
            "99".to_owned(),
            "--max-depth".to_owned(),
            "7".to_owned(),
            "--max-tracks".to_owned(),
            "5".to_owned(),
            "--max-table-entries".to_owned(),
            "1234".to_owned(),
            "--max-table-bytes".to_owned(),
            "4096".to_owned(),
            "--max-compatible-brands".to_owned(),
            "8".to_owned(),
            "--max-sample-descriptions".to_owned(),
            "4".to_owned(),
            "--format".to_owned(),
            "json".to_owned(),
        ];
        assert!(matches!(
            parse_media_inspect(&arguments),
            Ok(ref value)
                if value.path.to_string_lossy() == "flight.mp4"
                    && value.format == OutputFormat::Json
                    && value.limits.max_boxes == 99
                    && value.limits.max_depth == 7
                    && value.limits.max_tracks == 5
                    && value.limits.max_table_entries == 1234
                    && value.limits.max_table_bytes == 4096
                    && value.limits.max_compatible_brands == 8
                    && value.limits.max_sample_descriptions == 4
        ));
    }
}
