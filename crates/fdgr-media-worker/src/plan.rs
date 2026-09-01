#![forbid(unsafe_code)]
#![allow(clippy::module_name_repetitions, clippy::struct_excessive_bools)]
//! Deterministic media decode plans with no ambient paths or authority.

use crate::MEDIA_DECODE_PLAN_SCHEMA;
use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// Maximum admitted output width or height in pixels.
pub const MAX_DECODE_DIMENSION: u32 = 65_535;
/// Maximum admitted samples or decoded frames in one bounded operation.
pub const MAX_DECODE_FRAMES: u64 = 1_000_000;
/// Maximum admitted worker thread count.
pub const MAX_WORKER_THREADS: u16 = 1_024;
/// Maximum admitted wall-time budget for one operation.
pub const MAX_WALL_TIME_MS: u64 = 86_400_000;

/// Exact decoded pixel representation expected from the worker profile.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum DecodePixelFormat {
    /// One unsigned luminance byte per pixel.
    Gray8,
    /// Interleaved red, green, and blue bytes.
    Rgb24,
    /// Interleaved red, green, blue, and alpha bytes.
    Rgba,
    /// Planar 4:2:0 luma/chroma samples.
    Yuv420p,
}

impl DecodePixelFormat {
    /// Returns the canonical lower-snake-case wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Gray8 => "gray8",
            Self::Rgb24 => "rgb24",
            Self::Rgba => "rgba",
            Self::Yuv420p => "yuv420p",
        }
    }

    /// Returns a conservative integral byte upper bound used for output budgeting.
    #[must_use]
    pub const fn bytes_per_pixel_upper_bound(self) -> u8 {
        match self {
            Self::Gray8 => 1,
            Self::Rgb24 => 3,
            Self::Rgba => 4,
            Self::Yuv420p => 2,
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Gray8 => 1,
            Self::Rgb24 => 2,
            Self::Rgba => 3,
            Self::Yuv420p => 4,
        }
    }
}

impl Display for DecodePixelFormat {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        formatter.write_str(self.as_str())
    }
}

/// Construction input for one immutable decode plan.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MediaDecodePlanInput {
    /// Published recorded-media root manifest anchoring the request.
    pub source_root_manifest_digest: EvidenceDigest,
    /// Published original-media manifest selected from the verified root.
    pub source_manifest_digest: EvidenceDigest,
    /// Logical digest of the exact encoded source bytes.
    pub source_object_digest: EvidenceDigest,
    /// Exact encoded source length.
    pub source_object_length: u64,
    /// ISO BMFF track identity, not an ambient stream-array index.
    pub track_id: u32,
    /// First encoded sample in the requested range.
    pub start_sample: u64,
    /// Maximum encoded samples admitted by the request.
    pub max_samples: u64,
    /// Required decoded pixel representation.
    pub pixel_format: DecodePixelFormat,
    /// Exact decoded output width.
    pub output_width: u32,
    /// Exact decoded output height.
    pub output_height: u32,
    /// Hard maximum decoded frame count.
    pub max_frames: u64,
    /// Hard maximum bytes across all decoded frame objects.
    pub max_output_bytes: u64,
    /// Hard worker wall-time budget.
    pub max_wall_time_ms: u64,
    /// Hard worker resident-memory budget.
    pub max_memory_bytes: u64,
    /// Digest of the exact worker executable bytes.
    pub worker_executable_digest: EvidenceDigest,
    /// Digest of the exact bounded worker version/capability probe.
    pub worker_version_digest: EvidenceDigest,
    /// Digest of the admitted argument/environment/output profile.
    pub profile_digest: EvidenceDigest,
    /// Explicit worker thread limit.
    pub worker_threads: u16,
    /// Network posture; production decode plans require `false`.
    pub network_allowed: bool,
    /// Deterministic-profile posture; production reference plans require `true`.
    pub deterministic: bool,
}

/// Immutable, validated decode plan. It contains no filesystem path, credential, or dispatch token.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MediaDecodePlan {
    input: MediaDecodePlanInput,
}

impl MediaDecodePlan {
    /// Validates and constructs one plan.
    ///
    /// # Errors
    ///
    /// Returns a stable semantic error for an invalid identity, range, output, budget, or worker
    /// posture.
    pub fn new(input: MediaDecodePlanInput) -> Result<Self, DecodePlanError> {
        let plan = Self { input };
        plan.validate()?;
        Ok(plan)
    }

    /// Returns the exact validated input fields.
    #[must_use]
    pub const fn input(&self) -> &MediaDecodePlanInput {
        &self.input
    }

    /// Revalidates every invariant before encoding, dispatch, or receipt comparison.
    ///
    /// # Errors
    ///
    /// Returns a stable semantic error naming the first invalid field in canonical validation
    /// order.
    pub fn validate(&self) -> Result<(), DecodePlanError> {
        let input = &self.input;
        reject_zero_digest(
            "source_root_manifest_digest",
            &input.source_root_manifest_digest,
        )?;
        reject_zero_digest("source_manifest_digest", &input.source_manifest_digest)?;
        reject_zero_digest("source_object_digest", &input.source_object_digest)?;
        reject_zero_digest(
            "worker_executable_digest",
            &input.worker_executable_digest,
        )?;
        reject_zero_digest("worker_version_digest", &input.worker_version_digest)?;
        reject_zero_digest("profile_digest", &input.profile_digest)?;
        if input.source_object_length == 0 {
            return Err(DecodePlanError::ZeroValue {
                field: "source_object_length",
            });
        }
        if input.track_id == 0 {
            return Err(DecodePlanError::ZeroValue { field: "track_id" });
        }
        if input.max_samples == 0 {
            return Err(DecodePlanError::ZeroValue {
                field: "max_samples",
            });
        }
        if input.max_samples > MAX_DECODE_FRAMES {
            return Err(DecodePlanError::BoundExceeded {
                field: "max_samples",
                actual: input.max_samples,
                maximum: MAX_DECODE_FRAMES,
            });
        }
        input
            .start_sample
            .checked_add(input.max_samples)
            .ok_or(DecodePlanError::SampleRangeOverflow)?;
        validate_dimension("output_width", input.output_width)?;
        validate_dimension("output_height", input.output_height)?;
        if input.max_frames < input.max_samples {
            return Err(DecodePlanError::FrameBudgetBelowSampleRange {
                max_frames: input.max_frames,
                max_samples: input.max_samples,
            });
        }
        if input.max_frames > MAX_DECODE_FRAMES {
            return Err(DecodePlanError::BoundExceeded {
                field: "max_frames",
                actual: input.max_frames,
                maximum: MAX_DECODE_FRAMES,
            });
        }
        if input.max_wall_time_ms == 0 {
            return Err(DecodePlanError::ZeroValue {
                field: "max_wall_time_ms",
            });
        }
        if input.max_wall_time_ms > MAX_WALL_TIME_MS {
            return Err(DecodePlanError::BoundExceeded {
                field: "max_wall_time_ms",
                actual: input.max_wall_time_ms,
                maximum: MAX_WALL_TIME_MS,
            });
        }
        if input.max_memory_bytes == 0 {
            return Err(DecodePlanError::ZeroValue {
                field: "max_memory_bytes",
            });
        }
        if input.worker_threads == 0 {
            return Err(DecodePlanError::ZeroValue {
                field: "worker_threads",
            });
        }
        if input.worker_threads > MAX_WORKER_THREADS {
            return Err(DecodePlanError::BoundExceeded {
                field: "worker_threads",
                actual: u64::from(input.worker_threads),
                maximum: u64::from(MAX_WORKER_THREADS),
            });
        }
        if input.network_allowed {
            return Err(DecodePlanError::NetworkMustBeDisabled);
        }
        if !input.deterministic {
            return Err(DecodePlanError::DeterministicProfileRequired);
        }
        let required_output_bytes = required_output_bytes(input)?;
        if input.max_output_bytes < required_output_bytes {
            return Err(DecodePlanError::OutputBudgetTooSmall {
                required: required_output_bytes,
                admitted: input.max_output_bytes,
            });
        }
        Ok(())
    }

    /// Returns the deterministic path-free canonical binary form.
    ///
    /// # Errors
    ///
    /// Returns a semantic validation error or a canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, DecodePlanError> {
        self.validate()?;
        let input = &self.input;
        let mut encoder = Encoder::with_capacity(512);
        encoder.put_str(MEDIA_DECODE_PLAN_SCHEMA)?;
        encoder.put_digest(&input.source_root_manifest_digest);
        encoder.put_digest(&input.source_manifest_digest);
        encoder.put_digest(&input.source_object_digest);
        encoder.put_u64(input.source_object_length);
        encoder.put_u32(input.track_id);
        encoder.put_u64(input.start_sample);
        encoder.put_u64(input.max_samples);
        encoder.put_u8(input.pixel_format.code());
        encoder.put_u32(input.output_width);
        encoder.put_u32(input.output_height);
        encoder.put_u64(input.max_frames);
        encoder.put_u64(input.max_output_bytes);
        encoder.put_u64(input.max_wall_time_ms);
        encoder.put_u64(input.max_memory_bytes);
        encoder.put_digest(&input.worker_executable_digest);
        encoder.put_digest(&input.worker_version_digest);
        encoder.put_digest(&input.profile_digest);
        encoder.put_u16(input.worker_threads);
        encoder.put_bool(input.network_allowed);
        encoder.put_bool(input.deterministic);
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated plan identity.
    ///
    /// # Errors
    ///
    /// Returns a semantic validation, domain, or canonical hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, DecodePlanError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(MEDIA_DECODE_PLAN_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Returns deterministic JSON for presentation and process-manifest materialization.
    ///
    /// # Errors
    ///
    /// Returns the same validation or hashing errors as [`Self::digest`].
    pub fn to_json(&self) -> Result<String, DecodePlanError> {
        let digest = self.digest()?;
        let input = &self.input;
        Ok(format!(
            "{{\"schema\":\"{MEDIA_DECODE_PLAN_SCHEMA}\",\"plan_digest\":\"{digest}\",\"source_root_manifest_digest\":\"{}\",\"source_manifest_digest\":\"{}\",\"source_object_digest\":\"{}\",\"source_object_length\":{},\"track_id\":{},\"start_sample\":{},\"max_samples\":{},\"pixel_format\":\"{}\",\"output_width\":{},\"output_height\":{},\"max_frames\":{},\"max_output_bytes\":{},\"max_wall_time_ms\":{},\"max_memory_bytes\":{},\"worker_executable_digest\":\"{}\",\"worker_version_digest\":\"{}\",\"profile_digest\":\"{}\",\"worker_threads\":{},\"network_allowed\":{},\"deterministic\":{}}}",
            input.source_root_manifest_digest,
            input.source_manifest_digest,
            input.source_object_digest,
            input.source_object_length,
            input.track_id,
            input.start_sample,
            input.max_samples,
            input.pixel_format,
            input.output_width,
            input.output_height,
            input.max_frames,
            input.max_output_bytes,
            input.max_wall_time_ms,
            input.max_memory_bytes,
            input.worker_executable_digest,
            input.worker_version_digest,
            input.profile_digest,
            input.worker_threads,
            input.network_allowed,
            input.deterministic,
        ))
    }
}

/// Stable plan-construction and canonicalization failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DecodePlanError {
    /// A mandatory numeric field was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
    },
    /// A mandatory identity used the all-zero sentinel.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A numeric field exceeded its constitutional hard bound.
    BoundExceeded {
        /// Stable field name.
        field: &'static str,
        /// Observed value.
        actual: u64,
        /// Maximum admitted value.
        maximum: u64,
    },
    /// The sample range overflowed `u64`.
    SampleRangeOverflow,
    /// The frame budget could not cover the admitted sample range.
    FrameBudgetBelowSampleRange {
        /// Admitted decoded frame count.
        max_frames: u64,
        /// Requested encoded sample count.
        max_samples: u64,
    },
    /// The output byte budget cannot hold the worst-case admitted raw frames.
    OutputBudgetTooSmall {
        /// Minimum required bytes.
        required: u64,
        /// Admitted bytes.
        admitted: u64,
    },
    /// Required output-size arithmetic overflowed.
    OutputSizeOverflow,
    /// A production decode plan attempted to grant network access.
    NetworkMustBeDisabled,
    /// The selected worker profile was not deterministic.
    DeterministicProfileRequired,
    /// Canonical encoding or hashing failed.
    Codec(CodecError),
    /// The plan identity domain was invalid.
    Domain(DomainError),
}

impl Display for DecodePlanError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroValue { field } => write!(formatter, "decode-plan field {field} must be nonzero"),
            Self::ZeroIdentity { field } => write!(formatter, "decode-plan identity {field} must not be all zero"),
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(formatter, "decode-plan field {field} is {actual}; maximum is {maximum}"),
            Self::SampleRangeOverflow => formatter.write_str("decode-plan sample range overflows u64"),
            Self::FrameBudgetBelowSampleRange {
                max_frames,
                max_samples,
            } => write!(
                formatter,
                "decode-plan frame budget {max_frames} cannot cover {max_samples} admitted samples"
            ),
            Self::OutputBudgetTooSmall { required, admitted } => write!(
                formatter,
                "decode-plan output budget is {admitted} bytes; at least {required} are required"
            ),
            Self::OutputSizeOverflow => formatter.write_str("decode-plan worst-case output size overflows u64"),
            Self::NetworkMustBeDisabled => formatter.write_str("decode-plan worker network access must be disabled"),
            Self::DeterministicProfileRequired => formatter.write_str("decode-plan worker profile must be deterministic"),
            Self::Codec(error) => write!(formatter, "decode-plan codec error: {error}"),
            Self::Domain(error) => write!(formatter, "decode-plan identity-domain error: {error}"),
        }
    }
}

impl Error for DecodePlanError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for DecodePlanError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for DecodePlanError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}

fn validate_dimension(field: &'static str, value: u32) -> Result<(), DecodePlanError> {
    if value == 0 {
        return Err(DecodePlanError::ZeroValue { field });
    }
    if value > MAX_DECODE_DIMENSION {
        return Err(DecodePlanError::BoundExceeded {
            field,
            actual: u64::from(value),
            maximum: u64::from(MAX_DECODE_DIMENSION),
        });
    }
    Ok(())
}

fn reject_zero_digest(
    field: &'static str,
    value: &EvidenceDigest,
) -> Result<(), DecodePlanError> {
    if value.as_str().bytes().all(|byte| byte == b'0') {
        Err(DecodePlanError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn required_output_bytes(input: &MediaDecodePlanInput) -> Result<u64, DecodePlanError> {
    u64::from(input.output_width)
        .checked_mul(u64::from(input.output_height))
        .and_then(|value| {
            value.checked_mul(u64::from(
                input.pixel_format.bytes_per_pixel_upper_bound(),
            ))
        })
        .and_then(|value| value.checked_mul(input.max_frames))
        .ok_or(DecodePlanError::OutputSizeOverflow)
}

#[cfg(test)]
mod tests {
    use super::{
        DecodePixelFormat, DecodePlanError, MediaDecodePlan, MediaDecodePlanInput,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn input() -> MediaDecodePlanInput {
        MediaDecodePlanInput {
            source_root_manifest_digest: digest(1),
            source_manifest_digest: digest(2),
            source_object_digest: digest(3),
            source_object_length: 1_000_000,
            track_id: 7,
            start_sample: 10,
            max_samples: 4,
            pixel_format: DecodePixelFormat::Rgb24,
            output_width: 16,
            output_height: 8,
            max_frames: 4,
            max_output_bytes: 1_536,
            max_wall_time_ms: 5_000,
            max_memory_bytes: 64 * 1024 * 1024,
            worker_executable_digest: digest(4),
            worker_version_digest: digest(5),
            profile_digest: digest(6),
            worker_threads: 1,
            network_allowed: false,
            deterministic: true,
        }
    }

    #[test]
    fn plan_identity_is_deterministic_and_basis_sensitive() {
        let first = MediaDecodePlan::new(input());
        let second = MediaDecodePlan::new(input());
        assert!(matches!((&first, &second), (Ok(left), Ok(right)) if left.digest() == right.digest()));
        let mut changed = input();
        changed.start_sample = 11;
        let changed = MediaDecodePlan::new(changed);
        assert!(matches!((first, changed), (Ok(left), Ok(right)) if left.digest() != right.digest()));
    }

    #[test]
    fn plan_refuses_network_and_undersized_output_budget() {
        let mut network = input();
        network.network_allowed = true;
        assert!(matches!(
            MediaDecodePlan::new(network),
            Err(DecodePlanError::NetworkMustBeDisabled)
        ));
        let mut small = input();
        small.max_output_bytes = 1_535;
        assert!(matches!(
            MediaDecodePlan::new(small),
            Err(DecodePlanError::OutputBudgetTooSmall { .. })
        ));
    }

    #[test]
    fn json_contains_no_path_or_dispatch_authority() {
        let plan = MediaDecodePlan::new(input());
        assert!(matches!(
            plan.and_then(|value| value.to_json()),
            Ok(ref json)
                if json.contains("\"network_allowed\":false")
                    && !json.contains("input_path")
                    && !json.contains("output_path")
                    && !json.contains("dispatch")
        ));
    }
}
