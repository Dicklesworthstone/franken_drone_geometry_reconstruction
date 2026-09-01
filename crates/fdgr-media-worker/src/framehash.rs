#![forbid(unsafe_code)]
#![allow(clippy::module_name_repetitions)]
//! Bounded parser for FFmpeg framehash version 2 evidence.

use fdgr_types::{DigestError, EvidenceDigest};
use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// Hard parser limits for attacker-controlled framehash text.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FrameHashLimits {
    /// Maximum total UTF-8 bytes.
    pub max_total_bytes: usize,
    /// Maximum bytes in one line.
    pub max_line_bytes: usize,
    /// Maximum frame records.
    pub max_records: usize,
}

impl Default for FrameHashLimits {
    fn default() -> Self {
        Self {
            max_total_bytes: 64 * 1024 * 1024,
            max_line_bytes: 16 * 1024,
            max_records: 1_000_000,
        }
    }
}

/// One canonical framehash record in worker-output order.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FrameHashRecord {
    /// Zero-based record order in the evidence stream.
    pub record_index: u64,
    /// Worker stream index reported by the muxer.
    pub stream_index: u32,
    /// Decode timestamp in the stream time base.
    pub dts: i64,
    /// Presentation timestamp in the stream time base.
    pub pts: i64,
    /// Frame duration in the stream time base.
    pub duration: u64,
    /// Exact decoded frame byte length hashed by the worker.
    pub byte_length: u64,
    /// SHA-256 digest emitted by framehash.
    pub digest: EvidenceDigest,
}

/// Validated bounded framehash report.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FrameHashReport {
    /// Canonical framehash version. This parser admits only version 2.
    pub version: u32,
    /// Canonical hash name. This parser admits only `SHA256`.
    pub hash_name: &'static str,
    /// Ordered frame records.
    pub records: Vec<FrameHashRecord>,
    /// Sum of all record byte lengths.
    pub total_frame_bytes: u64,
}

/// Parses a bounded FFmpeg framehash version 2 document.
///
/// The parser ignores bounded informational comment lines but requires explicit `#version: 2` and
/// `#hash: SHA256` headers before the first data record.
///
/// # Errors
///
/// Returns a stable line-aware error for a missing/unsupported header, malformed record, invalid
/// digest, bound violation, or integer overflow.
pub fn parse_framehash_v2(
    input: &str,
    limits: FrameHashLimits,
) -> Result<FrameHashReport, FrameHashError> {
    if input.len() > limits.max_total_bytes {
        return Err(FrameHashError::TotalBytesExceeded {
            actual: input.len(),
            maximum: limits.max_total_bytes,
        });
    }
    let mut version: Option<String> = None;
    let mut hash_name: Option<String> = None;
    let mut records = Vec::new();
    let mut total_frame_bytes = 0_u64;
    for (index, raw_line) in input.lines().enumerate() {
        let line_number = index.saturating_add(1);
        if raw_line.len() > limits.max_line_bytes {
            return Err(FrameHashError::LineBytesExceeded {
                line: line_number,
                actual: raw_line.len(),
                maximum: limits.max_line_bytes,
            });
        }
        let line = raw_line.trim();
        if line.is_empty() {
            continue;
        }
        if let Some(value) = line.strip_prefix("#version:") {
            set_header(&mut version, value.trim(), "version", line_number)?;
            continue;
        }
        if let Some(value) = line.strip_prefix("#hash:") {
            set_header(&mut hash_name, value.trim(), "hash", line_number)?;
            continue;
        }
        if line.starts_with('#') {
            continue;
        }
        if version.is_none() || hash_name.is_none() {
            return Err(FrameHashError::RecordBeforeRequiredHeaders { line: line_number });
        }
        let actual_fields = line.split(',').count();
        if actual_fields != 6 {
            return Err(FrameHashError::WrongFieldCount {
                line: line_number,
                actual: actual_fields,
            });
        }
        if records.len() >= limits.max_records {
            return Err(FrameHashError::RecordLimitExceeded {
                actual: records.len().saturating_add(1),
                maximum: limits.max_records,
            });
        }
        let mut fields = line.split(',').map(str::trim);
        let stream_text = required_field(&mut fields, line_number)?;
        let dts_text = required_field(&mut fields, line_number)?;
        let pts_text = required_field(&mut fields, line_number)?;
        let duration_text = required_field(&mut fields, line_number)?;
        let size_text = required_field(&mut fields, line_number)?;
        let digest_text = required_field(&mut fields, line_number)?;
        let stream_index = parse_u32(stream_text, "stream_index", line_number)?;
        let dts = parse_i64(dts_text, "dts", line_number)?;
        let pts = parse_i64(pts_text, "pts", line_number)?;
        let duration = parse_u64(duration_text, "duration", line_number)?;
        let byte_length = parse_u64(size_text, "byte_length", line_number)?;
        let digest = EvidenceDigest::parse(digest_text)
            .map_err(|source| FrameHashError::InvalidDigest {
                line: line_number,
                source,
            })?;
        total_frame_bytes = total_frame_bytes
            .checked_add(byte_length)
            .ok_or(FrameHashError::TotalFrameBytesOverflow)?;
        let record_index =
            u64::try_from(records.len()).map_err(|_| FrameHashError::RecordIndexOverflow)?;
        records.push(FrameHashRecord {
            record_index,
            stream_index,
            dts,
            pts,
            duration,
            byte_length,
            digest,
        });
    }
    let version = version.ok_or(FrameHashError::MissingVersion)?;
    if version != "2" {
        return Err(FrameHashError::UnsupportedVersion { observed: version });
    }
    let hash_name = hash_name.ok_or(FrameHashError::MissingHash)?;
    if hash_name != "SHA256" {
        return Err(FrameHashError::UnsupportedHash {
            observed: hash_name,
        });
    }
    if records.is_empty() {
        return Err(FrameHashError::NoRecords);
    }
    Ok(FrameHashReport {
        version: 2,
        hash_name: "SHA256",
        records,
        total_frame_bytes,
    })
}

/// Stable bounded framehash parse failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum FrameHashError {
    /// The complete document exceeded its hard byte bound.
    TotalBytesExceeded {
        /// Observed bytes.
        actual: usize,
        /// Maximum admitted bytes.
        maximum: usize,
    },
    /// One line exceeded its hard byte bound.
    LineBytesExceeded {
        /// One-based line number.
        line: usize,
        /// Observed bytes.
        actual: usize,
        /// Maximum admitted bytes.
        maximum: usize,
    },
    /// A repeated header changed value.
    ConflictingHeader {
        /// Stable header name.
        header: &'static str,
        /// One-based line number of the conflict.
        line: usize,
    },
    /// A data record appeared before version and hash headers.
    RecordBeforeRequiredHeaders {
        /// One-based line number.
        line: usize,
    },
    /// A record did not contain exactly six comma-separated fields.
    WrongFieldCount {
        /// One-based line number.
        line: usize,
        /// Observed field count.
        actual: usize,
    },
    /// A required field was unexpectedly absent after field-count validation.
    MissingField {
        /// One-based line number.
        line: usize,
    },
    /// An integer field was malformed or outside its target type.
    InvalidInteger {
        /// One-based line number.
        line: usize,
        /// Stable field name.
        field: &'static str,
        /// Exact rejected text.
        value: String,
    },
    /// A frame digest was not canonical SHA-256 text.
    InvalidDigest {
        /// One-based line number.
        line: usize,
        /// Underlying digest error.
        source: DigestError,
    },
    /// The number of frame records exceeded its hard bound.
    RecordLimitExceeded {
        /// Attempted record count.
        actual: usize,
        /// Maximum admitted records.
        maximum: usize,
    },
    /// A platform record index could not fit in `u64`.
    RecordIndexOverflow,
    /// Summed decoded bytes overflowed `u64`.
    TotalFrameBytesOverflow,
    /// No version header was present.
    MissingVersion,
    /// The framehash version was not 2.
    UnsupportedVersion {
        /// Exact observed version text.
        observed: String,
    },
    /// No hash header was present.
    MissingHash,
    /// The hash algorithm was not SHA256.
    UnsupportedHash {
        /// Exact observed hash text.
        observed: String,
    },
    /// The document contained no frame records.
    NoRecords,
}

impl Display for FrameHashError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::TotalBytesExceeded { actual, maximum } => write!(
                formatter,
                "framehash document is {actual} bytes; maximum is {maximum}"
            ),
            Self::LineBytesExceeded {
                line,
                actual,
                maximum,
            } => write!(
                formatter,
                "framehash line {line} is {actual} bytes; maximum is {maximum}"
            ),
            Self::ConflictingHeader { header, line } => {
                write!(formatter, "framehash {header} header conflicts at line {line}")
            }
            Self::RecordBeforeRequiredHeaders { line } => write!(
                formatter,
                "framehash record at line {line} precedes required version/hash headers"
            ),
            Self::WrongFieldCount { line, actual } => write!(
                formatter,
                "framehash record at line {line} has {actual} fields; expected 6"
            ),
            Self::MissingField { line } => {
                write!(formatter, "framehash record at line {line} is missing a field")
            }
            Self::InvalidInteger { line, field, value } => write!(
                formatter,
                "framehash field {field} at line {line} is not a canonical integer: {value:?}"
            ),
            Self::InvalidDigest { line, source } => {
                write!(formatter, "framehash digest at line {line} is invalid: {source}")
            }
            Self::RecordLimitExceeded { actual, maximum } => write!(
                formatter,
                "framehash contains at least {actual} records; maximum is {maximum}"
            ),
            Self::RecordIndexOverflow => {
                formatter.write_str("framehash record index cannot fit in u64")
            }
            Self::TotalFrameBytesOverflow => {
                formatter.write_str("framehash total decoded bytes overflow u64")
            }
            Self::MissingVersion => formatter.write_str("framehash is missing #version"),
            Self::UnsupportedVersion { observed } => write!(
                formatter,
                "framehash version {observed:?} is unsupported; expected \"2\""
            ),
            Self::MissingHash => formatter.write_str("framehash is missing #hash"),
            Self::UnsupportedHash { observed } => write!(
                formatter,
                "framehash hash {observed:?} is unsupported; expected \"SHA256\""
            ),
            Self::NoRecords => formatter.write_str("framehash contains no records"),
        }
    }
}

impl Error for FrameHashError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::InvalidDigest { source, .. } => Some(source),
            _ => None,
        }
    }
}

fn set_header(
    target: &mut Option<String>,
    value: &str,
    header: &'static str,
    line: usize,
) -> Result<(), FrameHashError> {
    if let Some(existing) = target {
        if existing != value {
            return Err(FrameHashError::ConflictingHeader { header, line });
        }
    } else {
        *target = Some(value.to_owned());
    }
    Ok(())
}

fn required_field<'a>(
    fields: &mut impl Iterator<Item = &'a str>,
    line: usize,
) -> Result<&'a str, FrameHashError> {
    fields.next().ok_or(FrameHashError::MissingField { line })
}

fn parse_u32(value: &str, field: &'static str, line: usize) -> Result<u32, FrameHashError> {
    value
        .parse::<u32>()
        .map_err(|_| FrameHashError::InvalidInteger {
            line,
            field,
            value: value.to_owned(),
        })
}

fn parse_u64(value: &str, field: &'static str, line: usize) -> Result<u64, FrameHashError> {
    value
        .parse::<u64>()
        .map_err(|_| FrameHashError::InvalidInteger {
            line,
            field,
            value: value.to_owned(),
        })
}

fn parse_i64(value: &str, field: &'static str, line: usize) -> Result<i64, FrameHashError> {
    value
        .parse::<i64>()
        .map_err(|_| FrameHashError::InvalidInteger {
            line,
            field,
            value: value.to_owned(),
        })
}

#[cfg(test)]
mod tests {
    use super::{FrameHashError, FrameHashLimits, parse_framehash_v2};

    fn digest(pair: &str) -> String {
        pair.repeat(32)
    }

    fn valid() -> String {
        format!(
            "#format: frame checksums\n#version: 2\n#hash: SHA256\n#software: fdgr-test\n0, 0, 0, 1, 12, {}\n0, 1, 2, 1, 15, {}\n",
            digest("ab"),
            digest("cd")
        )
    }

    #[test]
    fn parser_accepts_bounded_sha256_v2_records() {
        let report = parse_framehash_v2(&valid(), FrameHashLimits::default());
        assert!(matches!(
            report,
            Ok(ref value)
                if value.version == 2
                    && value.hash_name == "SHA256"
                    && value.records.len() == 2
                    && value.total_frame_bytes == 27
                    && value.records.first().is_some_and(|record| record.pts == 0)
                    && value.records.get(1).is_some_and(|record| record.pts == 2)
        ));
    }

    #[test]
    fn parser_refuses_wrong_hash_and_record_overflow() {
        let wrong_hash = valid().replace("#hash: SHA256", "#hash: MD5");
        assert!(matches!(
            parse_framehash_v2(&wrong_hash, FrameHashLimits::default()),
            Err(FrameHashError::UnsupportedHash { .. })
        ));
        let limits = FrameHashLimits {
            max_records: 1,
            ..FrameHashLimits::default()
        };
        assert!(matches!(
            parse_framehash_v2(&valid(), limits),
            Err(FrameHashError::RecordLimitExceeded { .. })
        ));
    }

    #[test]
    fn parser_requires_headers_before_records() {
        let input = format!("0, 0, 0, 1, 12, {}\n#version: 2\n#hash: SHA256\n", digest("ab"));
        assert!(matches!(
            parse_framehash_v2(&input, FrameHashLimits::default()),
            Err(FrameHashError::RecordBeforeRequiredHeaders { line: 1 })
        ));
    }
}
