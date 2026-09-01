#![forbid(unsafe_code)]
//! CLI arguments for authority-free media decode planning.

use crate::args::OutputFormat;
use fdgr_media_worker::DecodePixelFormat;
use fdgr_types::EvidenceDigest;
use std::collections::BTreeSet;
use std::path::PathBuf;

pub(crate) const DEFAULT_DECODE_WALL_TIME_MS: u64 = 60_000;
pub(crate) const DEFAULT_DECODE_MEMORY_BYTES: u64 = 1024 * 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct MediaDecodePlanCliOptions {
    pub(crate) store_root: PathBuf,
    pub(crate) root_manifest_digest: EvidenceDigest,
    pub(crate) track_id: u32,
    pub(crate) start_sample: u64,
    pub(crate) max_samples: u64,
    pub(crate) pixel_format: DecodePixelFormat,
    pub(crate) output_width: u32,
    pub(crate) output_height: u32,
    pub(crate) max_frames: u64,
    pub(crate) max_output_bytes: u64,
    pub(crate) max_wall_time_ms: u64,
    pub(crate) max_memory_bytes: u64,
    pub(crate) worker_executable_digest: EvidenceDigest,
    pub(crate) worker_version_digest: EvidenceDigest,
    pub(crate) profile_digest: EvidenceDigest,
    pub(crate) worker_threads: u16,
    pub(crate) format: OutputFormat,
}

pub(crate) fn parse_media_decode_plan(
    arguments: &[String],
) -> Result<MediaDecodePlanCliOptions, String> {
    let usage = "usage: fdgr media-decode-plan <store-root> <recorded-media-root-manifest-digest> --track-id id --max-samples n --pixel-format gray8|rgb24|rgba|yuv420p --width pixels --height pixels --worker-executable-digest digest --worker-version-digest digest --profile-digest digest [--start-sample n] [--max-frames n] [--max-output-bytes n] [--max-wall-time-ms n] [--max-memory-bytes n] [--worker-threads n] [--format text|json]";
    let mut arguments = arguments.iter();
    let store_root = arguments.next().ok_or_else(|| usage.to_owned())?;
    let root_manifest_digest = arguments.next().ok_or_else(|| usage.to_owned())?;
    let root_manifest_digest =
        EvidenceDigest::parse(root_manifest_digest).map_err(|error| error.to_string())?;
    let mut track_id = None;
    let mut start_sample = 0_u64;
    let mut max_samples = None;
    let mut pixel_format = None;
    let mut output_width = None;
    let mut output_height = None;
    let mut max_frames = None;
    let mut max_output_bytes = None;
    let mut max_wall_time_ms = DEFAULT_DECODE_WALL_TIME_MS;
    let mut max_memory_bytes = DEFAULT_DECODE_MEMORY_BYTES;
    let mut worker_executable_digest = None;
    let mut worker_version_digest = None;
    let mut profile_digest = None;
    let mut worker_threads = 1_u16;
    let mut format = OutputFormat::Text;
    let mut seen = BTreeSet::new();
    while let Some(flag) = arguments.next() {
        if !seen.insert(flag.as_str()) {
            return Err(format!("duplicate media-decode-plan option {flag:?}"));
        }
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--track-id" => track_id = Some(parse_u32(value, "track id")?),
            "--start-sample" => start_sample = parse_u64(value, "start sample")?,
            "--max-samples" => max_samples = Some(parse_u64(value, "maximum samples")?),
            "--pixel-format" => pixel_format = Some(parse_pixel_format(value)?),
            "--width" => output_width = Some(parse_u32(value, "output width")?),
            "--height" => output_height = Some(parse_u32(value, "output height")?),
            "--max-frames" => max_frames = Some(parse_u64(value, "maximum frames")?),
            "--max-output-bytes" => {
                max_output_bytes = Some(parse_u64(value, "maximum output bytes")?);
            }
            "--max-wall-time-ms" => {
                max_wall_time_ms = parse_u64(value, "maximum wall time")?;
            }
            "--max-memory-bytes" => {
                max_memory_bytes = parse_u64(value, "maximum memory bytes")?;
            }
            "--worker-executable-digest" => {
                worker_executable_digest = Some(parse_digest(value, "worker executable digest")?);
            }
            "--worker-version-digest" => {
                worker_version_digest = Some(parse_digest(value, "worker version digest")?);
            }
            "--profile-digest" => {
                profile_digest = Some(parse_digest(value, "worker profile digest")?);
            }
            "--worker-threads" => {
                worker_threads = parse_u16(value, "worker thread count")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown media-decode-plan option {flag:?}")),
        }
    }
    let track_id = track_id.ok_or_else(|| "missing required --track-id".to_owned())?;
    let max_samples = max_samples.ok_or_else(|| "missing required --max-samples".to_owned())?;
    let pixel_format =
        pixel_format.ok_or_else(|| "missing required --pixel-format".to_owned())?;
    let output_width = output_width.ok_or_else(|| "missing required --width".to_owned())?;
    let output_height = output_height.ok_or_else(|| "missing required --height".to_owned())?;
    let max_frames = max_frames.unwrap_or(max_samples);
    let required_output_bytes = minimum_output_bytes(
        output_width,
        output_height,
        max_frames,
        pixel_format.bytes_per_pixel_upper_bound(),
    )?;
    let max_output_bytes = max_output_bytes.unwrap_or(required_output_bytes);
    let worker_executable_digest = worker_executable_digest
        .ok_or_else(|| "missing required --worker-executable-digest".to_owned())?;
    let worker_version_digest = worker_version_digest
        .ok_or_else(|| "missing required --worker-version-digest".to_owned())?;
    let profile_digest =
        profile_digest.ok_or_else(|| "missing required --profile-digest".to_owned())?;
    Ok(MediaDecodePlanCliOptions {
        store_root: PathBuf::from(store_root),
        root_manifest_digest,
        track_id,
        start_sample,
        max_samples,
        pixel_format,
        output_width,
        output_height,
        max_frames,
        max_output_bytes,
        max_wall_time_ms,
        max_memory_bytes,
        worker_executable_digest,
        worker_version_digest,
        profile_digest,
        worker_threads,
        format,
    })
}

fn parse_pixel_format(value: &str) -> Result<DecodePixelFormat, String> {
    match value {
        "gray8" => Ok(DecodePixelFormat::Gray8),
        "rgb24" => Ok(DecodePixelFormat::Rgb24),
        "rgba" => Ok(DecodePixelFormat::Rgba),
        "yuv420p" => Ok(DecodePixelFormat::Yuv420p),
        _ => Err(format!(
            "unknown pixel format {value:?}; expected gray8, rgb24, rgba, or yuv420p"
        )),
    }
}

fn parse_format(value: &str) -> Result<OutputFormat, String> {
    match value {
        "text" => Ok(OutputFormat::Text),
        "json" => Ok(OutputFormat::Json),
        _ => Err(format!("unknown output format {value:?}; expected text or json")),
    }
}

fn parse_digest(value: &str, label: &str) -> Result<EvidenceDigest, String> {
    EvidenceDigest::parse(value).map_err(|error| format!("invalid {label}: {error}"))
}

fn parse_u16(value: &str, label: &str) -> Result<u16, String> {
    value
        .parse::<u16>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
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

fn minimum_output_bytes(
    width: u32,
    height: u32,
    frames: u64,
    bytes_per_pixel: u8,
) -> Result<u64, String> {
    u64::from(width)
        .checked_mul(u64::from(height))
        .and_then(|value| value.checked_mul(u64::from(bytes_per_pixel)))
        .and_then(|value| value.checked_mul(frames))
        .ok_or_else(|| "decode output-size budget overflows u64".to_owned())
}

#[cfg(test)]
mod tests {
    use super::parse_media_decode_plan;
    use crate::args::OutputFormat;
    use fdgr_media_worker::DecodePixelFormat;

    const DIGEST: &str =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    fn required() -> Vec<String> {
        [
            "store",
            DIGEST,
            "--track-id",
            "7",
            "--max-samples",
            "4",
            "--pixel-format",
            "rgb24",
            "--width",
            "16",
            "--height",
            "8",
            "--worker-executable-digest",
            DIGEST,
            "--worker-version-digest",
            DIGEST,
            "--profile-digest",
            DIGEST,
            "--format",
            "json",
        ]
        .into_iter()
        .map(str::to_owned)
        .collect()
    }

    #[test]
    fn parser_computes_a_conservative_default_output_budget() {
        let parsed = parse_media_decode_plan(&required());
        assert!(matches!(
            parsed,
            Ok(ref value)
                if value.track_id == 7
                    && value.max_samples == 4
                    && value.max_frames == 4
                    && value.pixel_format == DecodePixelFormat::Rgb24
                    && value.max_output_bytes == 1_536
                    && value.format == OutputFormat::Json
        ));
    }

    #[test]
    fn parser_rejects_duplicate_and_missing_identity_options() {
        let mut duplicate = required();
        duplicate.extend(["--track-id".to_owned(), "8".to_owned()]);
        assert!(parse_media_decode_plan(&duplicate).is_err());
        let missing: Vec<String> = required()
            .into_iter()
            .filter(|value| value != "--profile-digest" && value != DIGEST)
            .collect();
        assert!(parse_media_decode_plan(&missing).is_err());
    }
}
