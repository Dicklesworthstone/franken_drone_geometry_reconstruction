#![forbid(unsafe_code)]
//! CLI arguments and bounded anchor-table parsing for clock fitting.

use crate::args::OutputFormat;
use fdgr_clock::{
    ClockAnchor, ClockDomain, ClockFitOptions, MAX_CLOCK_ANCHORS,
};
use fdgr_codec::hash_bytes;
use fdgr_types::EvidenceDigest;
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path, PathBuf};

const CLOCK_ANCHOR_HEADER: &str =
    "anchor_id\tsource_tick\treference_tick\tuncertainty_ticks\tcorrelation_group";
const MAX_CLOCK_ANCHOR_TABLE_BYTES: u64 = 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct ClockFitCliOptions {
    pub(crate) anchor_path: PathBuf,
    pub(crate) basis_digest: EvidenceDigest,
    pub(crate) source_domain: ClockDomain,
    pub(crate) reference_domain: ClockDomain,
    pub(crate) source_epoch: u64,
    pub(crate) reference_epoch: u64,
    pub(crate) model_generation: u64,
    pub(crate) source_timescale: u64,
    pub(crate) reference_timescale: u64,
    pub(crate) fit: ClockFitOptions,
    pub(crate) format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct ClockAnchorTable {
    pub(crate) digest: EvidenceDigest,
    pub(crate) anchors: Vec<ClockAnchor>,
}

pub(crate) fn parse_clock_fit(arguments: &[String]) -> Result<ClockFitCliOptions, String> {
    let mut arguments = arguments.iter();
    let usage = "usage: fdgr clock-fit <anchors.tsv> --basis-digest <digest> --source-domain <id> --reference-domain <id> --source-epoch <n> --reference-epoch <n> --generation <n> --source-timescale <ticks-per-second> --reference-timescale <ticks-per-second> [--max-residual-ticks n] [--max-drift-ppm n] [--min-independent-groups n] [--format text|json]";
    let anchor_path = arguments.next().ok_or_else(|| usage.to_owned())?;
    let mut basis_digest = None;
    let mut source_domain = None;
    let mut reference_domain = None;
    let mut source_epoch = None;
    let mut reference_epoch = None;
    let mut model_generation = None;
    let mut source_timescale = None;
    let mut reference_timescale = None;
    let mut fit = ClockFitOptions::default();
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--basis-digest" => {
                basis_digest = Some(EvidenceDigest::parse(value).map_err(|error| error.to_string())?);
            }
            "--source-domain" => {
                source_domain = Some(ClockDomain::parse(value).map_err(|error| error.to_string())?);
            }
            "--reference-domain" => {
                reference_domain =
                    Some(ClockDomain::parse(value).map_err(|error| error.to_string())?);
            }
            "--source-epoch" => source_epoch = Some(parse_nonzero_u64(value, "source epoch")?),
            "--reference-epoch" => {
                reference_epoch = Some(parse_nonzero_u64(value, "reference epoch")?);
            }
            "--generation" => {
                model_generation = Some(parse_nonzero_u64(value, "model generation")?);
            }
            "--source-timescale" => {
                source_timescale = Some(parse_nonzero_u64(value, "source timescale")?);
            }
            "--reference-timescale" => {
                reference_timescale = Some(parse_nonzero_u64(value, "reference timescale")?);
            }
            "--max-residual-ticks" => {
                fit.max_residual_ticks = parse_nonzero_u64(value, "maximum residual ticks")?;
            }
            "--max-drift-ppm" => fit.max_drift_ppm = parse_u64(value, "maximum drift ppm")?,
            "--min-independent-groups" => {
                fit.min_independent_groups =
                    parse_nonzero_u16(value, "minimum independent groups")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown clock-fit option {flag:?}")),
        }
    }
    Ok(ClockFitCliOptions {
        anchor_path: PathBuf::from(anchor_path),
        basis_digest: basis_digest.ok_or_else(|| "missing --basis-digest".to_owned())?,
        source_domain: source_domain.ok_or_else(|| "missing --source-domain".to_owned())?,
        reference_domain: reference_domain.ok_or_else(|| "missing --reference-domain".to_owned())?,
        source_epoch: source_epoch.ok_or_else(|| "missing --source-epoch".to_owned())?,
        reference_epoch: reference_epoch.ok_or_else(|| "missing --reference-epoch".to_owned())?,
        model_generation: model_generation.ok_or_else(|| "missing --generation".to_owned())?,
        source_timescale: source_timescale
            .ok_or_else(|| "missing --source-timescale".to_owned())?,
        reference_timescale: reference_timescale
            .ok_or_else(|| "missing --reference-timescale".to_owned())?,
        fit,
        format,
    })
}

pub(crate) fn read_clock_anchor_table(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<ClockAnchorTable, String> {
    let metadata = fs::symlink_metadata(path)
        .map_err(|error| format!("clock anchor metadata failed: {error}"))?;
    if metadata.file_type().is_symlink() {
        return Err("clock anchor table must not be a symlink".to_owned());
    }
    if !metadata.is_file() {
        return Err("clock anchor table must be a regular file".to_owned());
    }
    let mut file = File::open(path).map_err(|error| format!("clock anchor open failed: {error}"))?;
    let mut bytes = Vec::new();
    file.by_ref()
        .take(MAX_CLOCK_ANCHOR_TABLE_BYTES.saturating_add(1))
        .read_to_end(&mut bytes)
        .map_err(|error| format!("clock anchor read failed: {error}"))?;
    if u64::try_from(bytes.len()).unwrap_or(u64::MAX) > MAX_CLOCK_ANCHOR_TABLE_BYTES {
        return Err(format!(
            "clock anchor table exceeds {MAX_CLOCK_ANCHOR_TABLE_BYTES} bytes"
        ));
    }
    let observed_digest = hash_bytes(&bytes);
    if &observed_digest != expected_digest {
        return Err(format!(
            "clock anchor basis digest mismatch: expected {expected_digest}, observed {observed_digest}"
        ));
    }
    let text = std::str::from_utf8(&bytes)
        .map_err(|error| format!("clock anchor table is not UTF-8: {error}"))?;
    if text.contains('\r') {
        return Err("clock anchor table must use LF line endings".to_owned());
    }
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "clock anchor table is empty".to_owned())?;
    if header != CLOCK_ANCHOR_HEADER {
        return Err(format!(
            "clock anchor header mismatch: expected {CLOCK_ANCHOR_HEADER:?}"
        ));
    }
    let mut anchors = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("clock anchor line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [anchor_id, source_tick, reference_tick, uncertainty_ticks, correlation_group] =
            fields.as_slice()
        else {
            return Err(format!(
                "clock anchor line {line_number} must contain exactly five tab-separated fields"
            ));
        };
        anchors.push(ClockAnchor {
            anchor_id: parse_nonzero_u64(anchor_id, "anchor id")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            source_tick: parse_i128(source_tick, "source tick")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            reference_tick: parse_i128(reference_tick, "reference tick")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            uncertainty_ticks: parse_u64(uncertainty_ticks, "uncertainty ticks")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            correlation_group: parse_nonzero_u32(correlation_group, "correlation group")
                .map_err(|error| format!("line {line_number}: {error}"))?,
        });
        if anchors.len() > MAX_CLOCK_ANCHORS {
            return Err(format!(
                "clock anchor table contains more than {MAX_CLOCK_ANCHORS} records"
            ));
        }
    }
    if anchors.is_empty() {
        return Err("clock anchor table contains no records".to_owned());
    }
    Ok(ClockAnchorTable {
        digest: observed_digest,
        anchors,
    })
}

fn parse_format(value: &str) -> Result<OutputFormat, String> {
    match value {
        "text" => Ok(OutputFormat::Text),
        "json" => Ok(OutputFormat::Json),
        _ => Err(format!("unknown output format {value:?}; expected text or json")),
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

fn parse_nonzero_u64(value: &str, label: &str) -> Result<u64, String> {
    let parsed = parse_u64(value, label)?;
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

fn parse_i128(value: &str, label: &str) -> Result<i128, String> {
    value
        .parse::<i128>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

#[cfg(test)]
mod tests {
    use super::{parse_clock_fit, read_clock_anchor_table};
    use crate::args::OutputFormat;
    use fdgr_codec::hash_bytes;
    use std::fs;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

    fn path(label: &str) -> PathBuf {
        let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
        std::env::temp_dir().join(format!("fdgr-clock-cli-{label}-{}-{id}", std::process::id()))
    }

    #[test]
    fn required_clock_fields_parse_in_any_order() {
        let digest = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        let arguments = vec![
            "anchors.tsv".to_owned(),
            "--reference-domain".to_owned(),
            "host_monotonic".to_owned(),
            "--basis-digest".to_owned(),
            digest.to_owned(),
            "--source-domain".to_owned(),
            "media_pts".to_owned(),
            "--source-epoch".to_owned(),
            "2".to_owned(),
            "--reference-epoch".to_owned(),
            "3".to_owned(),
            "--generation".to_owned(),
            "4".to_owned(),
            "--source-timescale".to_owned(),
            "1000".to_owned(),
            "--reference-timescale".to_owned(),
            "1000000000".to_owned(),
            "--min-independent-groups".to_owned(),
            "3".to_owned(),
            "--format".to_owned(),
            "json".to_owned(),
        ];
        assert!(matches!(parse_clock_fit(&arguments), Ok(ref value) if value.source_epoch == 2 && value.reference_epoch == 3 && value.model_generation == 4 && value.format == OutputFormat::Json));
    }

    #[test]
    fn anchor_table_is_digest_bound_and_exact() {
        let path = path("table");
        let bytes = b"anchor_id\tsource_tick\treference_tick\tuncertainty_ticks\tcorrelation_group\n1\t0\t5\t1\t1\n2\t10\t15\t1\t2\n";
        assert!(fs::write(&path, bytes).is_ok());
        let digest = hash_bytes(bytes);
        assert!(matches!(read_clock_anchor_table(&path, &digest), Ok(ref value) if value.digest == digest && value.anchors.len() == 2));
        let wrong = hash_bytes(b"different");
        assert!(read_clock_anchor_table(&path, &wrong).is_err());
        assert!(fs::remove_file(path).is_ok());
    }
}
