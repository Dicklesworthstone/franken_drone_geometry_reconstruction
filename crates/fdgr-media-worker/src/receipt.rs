#![forbid(unsafe_code)]
#![allow(clippy::module_name_repetitions, clippy::struct_excessive_bools)]
//! Decode worker termination and receipt semantics.

use crate::framehash::{FrameHashRecord, FrameHashReport};
use crate::plan::{DecodePlanError, MediaDecodePlan};
use crate::MEDIA_DECODE_RECEIPT_SCHEMA;
use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// Typed terminal observation from the process supervisor.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum DecodeTermination {
    /// Process exited successfully and all declared outputs were observed.
    Succeeded,
    /// Process exited with a known nonzero status.
    Failed {
        /// Exact process exit code.
        exit_code: i32,
    },
    /// Deadline expired after dispatch; descendants and partial outputs require reconciliation.
    TimedOut,
    /// Cancellation was observed before any process dispatch.
    CancelledBeforeDispatch,
    /// Cancellation was requested after dispatch; drain/reconciliation remains required.
    CancelledDuringExecution,
    /// The supervisor escalated termination after the graceful drain interval.
    KilledAfterGrace,
    /// Process creation failed before a child was observed.
    SpawnFailed,
    /// The supervisor cannot distinguish materially different external outcomes.
    Indeterminate,
}

impl DecodeTermination {
    /// Returns the canonical lower-snake-case value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Succeeded => "succeeded",
            Self::Failed { .. } => "failed",
            Self::TimedOut => "timed_out",
            Self::CancelledBeforeDispatch => "cancelled_before_dispatch",
            Self::CancelledDuringExecution => "cancelled_during_execution",
            Self::KilledAfterGrace => "killed_after_grace",
            Self::SpawnFailed => "spawn_failed",
            Self::Indeterminate => "indeterminate",
        }
    }

    /// Returns whether the receipt may satisfy decode completion after all other checks pass.
    #[must_use]
    pub const fn is_success(self) -> bool {
        matches!(self, Self::Succeeded)
    }

    /// Returns whether operation lookup/drain/reconciliation must dominate blind retry.
    #[must_use]
    pub const fn is_indeterminate(self) -> bool {
        matches!(
            self,
            Self::TimedOut
                | Self::CancelledDuringExecution
                | Self::KilledAfterGrace
                | Self::Indeterminate
        )
    }

    const fn code(self) -> u8 {
        match self {
            Self::Succeeded => 1,
            Self::Failed { .. } => 2,
            Self::TimedOut => 3,
            Self::CancelledBeforeDispatch => 4,
            Self::CancelledDuringExecution => 5,
            Self::KilledAfterGrace => 6,
            Self::SpawnFailed => 7,
            Self::Indeterminate => 8,
        }
    }

    const fn exit_code(self) -> Option<i32> {
        match self {
            Self::Failed { exit_code } => Some(exit_code),
            _ => None,
        }
    }
}

impl Display for DecodeTermination {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        formatter.write_str(self.as_str())
    }
}

/// Construction input for one immutable worker receipt.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MediaDecodeReceiptInput {
    /// Exact decode-plan identity claimed by the worker.
    pub plan_digest: EvidenceDigest,
    /// Exact executable identity observed by the supervisor.
    pub worker_executable_digest: EvidenceDigest,
    /// Exact version/capability-probe identity observed by the supervisor.
    pub worker_version_digest: EvidenceDigest,
    /// Exact admitted worker profile identity.
    pub profile_digest: EvidenceDigest,
    /// Typed process termination observation.
    pub termination: DecodeTermination,
    /// Immutable object containing the raw bounded framehash document, when observed.
    pub framehash_object_digest: Option<EvidenceDigest>,
    /// Parsed bounded framehash evidence, when observed.
    pub framehash: Option<FrameHashReport>,
    /// Published root manifest for complete decoded frame objects, when successful.
    pub output_root_manifest_digest: Option<EvidenceDigest>,
    /// Published logical root object for complete decoded frame objects, when successful.
    pub output_root_object_digest: Option<EvidenceDigest>,
    /// Observed worker wall time.
    pub wall_time_ms: u64,
    /// Observed peak resident memory.
    pub peak_memory_bytes: u64,
    /// Immutable digest of bounded stderr evidence.
    pub stderr_digest: EvidenceDigest,
}

/// Immutable decode-worker receipt. It has no authority until validated against its exact plan.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MediaDecodeReceipt {
    input: MediaDecodeReceiptInput,
}

impl MediaDecodeReceipt {
    /// Constructs a receipt without treating source presence as validation.
    #[must_use]
    pub const fn new(input: MediaDecodeReceiptInput) -> Self {
        Self { input }
    }

    /// Returns the exact observed receipt fields.
    #[must_use]
    pub const fn input(&self) -> &MediaDecodeReceiptInput {
        &self.input
    }

    /// Validates identity, termination, resource, frame, and publication semantics against a plan.
    ///
    /// # Errors
    ///
    /// Returns a stable refusal for any mismatched identity, false-success claim, noncanonical
    /// frame sequence, resource overrun, or partial-root publication.
    pub fn validate_against(&self, plan: &MediaDecodePlan) -> Result<(), DecodeReceiptError> {
        plan.validate().map_err(DecodeReceiptError::Plan)?;
        let expected_plan_digest = plan.digest().map_err(DecodeReceiptError::Plan)?;
        let receipt = &self.input;
        if receipt.plan_digest != expected_plan_digest {
            return Err(DecodeReceiptError::PlanDigestMismatch {
                expected: expected_plan_digest,
                observed: receipt.plan_digest.clone(),
            });
        }
        compare_identity(
            "worker_executable_digest",
            &plan.input().worker_executable_digest,
            &receipt.worker_executable_digest,
        )?;
        compare_identity(
            "worker_version_digest",
            &plan.input().worker_version_digest,
            &receipt.worker_version_digest,
        )?;
        compare_identity(
            "profile_digest",
            &plan.input().profile_digest,
            &receipt.profile_digest,
        )?;
        reject_zero_digest("stderr_digest", &receipt.stderr_digest)?;
        validate_optional_digest(
            "framehash_object_digest",
            receipt.framehash_object_digest.as_ref(),
        )?;
        validate_optional_digest(
            "output_root_manifest_digest",
            receipt.output_root_manifest_digest.as_ref(),
        )?;
        validate_optional_digest(
            "output_root_object_digest",
            receipt.output_root_object_digest.as_ref(),
        )?;
        if matches!(
            receipt.termination,
            DecodeTermination::Failed { exit_code: 0 }
        ) {
            return Err(DecodeReceiptError::FailedWithZeroExitCode);
        }
        if receipt.framehash.is_some() != receipt.framehash_object_digest.is_some() {
            return Err(DecodeReceiptError::FrameHashEvidenceMismatch);
        }
        if let Some(report) = &receipt.framehash {
            validate_framehash(report, plan)?;
        }
        if receipt.termination.is_success() {
            let Some(report) = &receipt.framehash else {
                return Err(DecodeReceiptError::SuccessMissingFrameHash);
            };
            if report.records.is_empty() {
                return Err(DecodeReceiptError::SuccessHasNoFrames);
            }
            if receipt.output_root_manifest_digest.is_none()
                || receipt.output_root_object_digest.is_none()
            {
                return Err(DecodeReceiptError::SuccessMissingOutputRoot);
            }
            if receipt.wall_time_ms > plan.input().max_wall_time_ms {
                return Err(DecodeReceiptError::SuccessfulResourceOverrun {
                    field: "wall_time_ms",
                    observed: receipt.wall_time_ms,
                    maximum: plan.input().max_wall_time_ms,
                });
            }
            if receipt.peak_memory_bytes > plan.input().max_memory_bytes {
                return Err(DecodeReceiptError::SuccessfulResourceOverrun {
                    field: "peak_memory_bytes",
                    observed: receipt.peak_memory_bytes,
                    maximum: plan.input().max_memory_bytes,
                });
            }
        } else if receipt.output_root_manifest_digest.is_some()
            || receipt.output_root_object_digest.is_some()
        {
            return Err(DecodeReceiptError::NonSuccessPublishedOutputRoot);
        }
        Ok(())
    }

    /// Returns whether this receipt may satisfy semantic decode completion.
    #[must_use]
    pub fn semantic_completion(&self, plan: &MediaDecodePlan) -> bool {
        self.input.termination.is_success() && self.validate_against(plan).is_ok()
    }

    /// Returns the deterministic canonical receipt bytes after plan validation.
    ///
    /// # Errors
    ///
    /// Returns a receipt-validation, domain, or canonical encoding error.
    pub fn to_canonical_bytes(
        &self,
        plan: &MediaDecodePlan,
    ) -> Result<Vec<u8>, DecodeReceiptError> {
        self.validate_against(plan)?;
        let receipt = &self.input;
        let mut encoder = Encoder::with_capacity(1024);
        encoder.put_str(MEDIA_DECODE_RECEIPT_SCHEMA)?;
        encoder.put_digest(&receipt.plan_digest);
        encoder.put_digest(&receipt.worker_executable_digest);
        encoder.put_digest(&receipt.worker_version_digest);
        encoder.put_digest(&receipt.profile_digest);
        encoder.put_u8(receipt.termination.code());
        encode_optional_i32(&mut encoder, receipt.termination.exit_code());
        encode_optional_digest(&mut encoder, receipt.framehash_object_digest.as_ref());
        encode_optional_digest(&mut encoder, receipt.output_root_manifest_digest.as_ref());
        encode_optional_digest(&mut encoder, receipt.output_root_object_digest.as_ref());
        encoder.put_u64(receipt.wall_time_ms);
        encoder.put_u64(receipt.peak_memory_bytes);
        encoder.put_digest(&receipt.stderr_digest);
        encode_framehash(&mut encoder, receipt.framehash.as_ref())?;
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated receipt identity after full plan validation.
    ///
    /// # Errors
    ///
    /// Returns a receipt-validation, domain, or canonical hashing error.
    pub fn digest(&self, plan: &MediaDecodePlan) -> Result<EvidenceDigest, DecodeReceiptError> {
        let bytes = self.to_canonical_bytes(plan)?;
        let domain = DigestDomain::parse(MEDIA_DECODE_RECEIPT_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }
}

/// Stable decode-receipt validation failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DecodeReceiptError {
    /// The referenced decode plan is invalid.
    Plan(DecodePlanError),
    /// Receipt and plan identities differ.
    PlanDigestMismatch {
        /// Expected plan digest.
        expected: EvidenceDigest,
        /// Observed receipt digest.
        observed: EvidenceDigest,
    },
    /// A worker or profile identity differs from the plan.
    IdentityMismatch {
        /// Stable field name.
        field: &'static str,
        /// Expected identity.
        expected: EvidenceDigest,
        /// Observed identity.
        observed: EvidenceDigest,
    },
    /// A receipt identity used the all-zero sentinel.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A failed termination carried exit code zero.
    FailedWithZeroExitCode,
    /// Raw framehash identity and parsed framehash presence disagree.
    FrameHashEvidenceMismatch,
    /// The framehash version/hash contract was not canonical.
    FrameHashContractMismatch {
        /// Observed version.
        version: u32,
        /// Observed hash name.
        hash_name: String,
    },
    /// Declared total frame bytes do not equal the record sum.
    FrameHashByteTotalMismatch {
        /// Total declared by the report.
        declared: u64,
        /// Total reconstructed from records.
        observed: u64,
    },
    /// Summing frame-record bytes overflowed `u64`.
    FrameHashByteTotalOverflow,
    /// A framehash report exceeded the plan's frame count.
    FrameBudgetExceeded {
        /// Observed records.
        observed: u64,
        /// Maximum admitted frames.
        maximum: u64,
    },
    /// A framehash report exceeded the plan's sample count.
    SampleRangeExceeded {
        /// Observed records.
        observed: u64,
        /// Maximum admitted samples.
        maximum: u64,
    },
    /// A framehash report exceeded the plan's byte budget.
    OutputBudgetExceeded {
        /// Observed decoded bytes.
        observed: u64,
        /// Maximum admitted bytes.
        maximum: u64,
    },
    /// Frame record identities were not the canonical contiguous sequence.
    NonCanonicalRecordIndex {
        /// Expected index.
        expected: u64,
        /// Observed index.
        observed: u64,
    },
    /// A frame count could not fit in `u64`.
    FrameCountOverflow,
    /// A successful receipt omitted raw/parsed framehash evidence.
    SuccessMissingFrameHash,
    /// A successful receipt contained no decoded frames.
    SuccessHasNoFrames,
    /// A successful receipt omitted a complete published output root.
    SuccessMissingOutputRoot,
    /// A non-success receipt attempted to publish a semantic output root.
    NonSuccessPublishedOutputRoot,
    /// A successful receipt exceeded a hard resource budget.
    SuccessfulResourceOverrun {
        /// Stable resource field.
        field: &'static str,
        /// Observed value.
        observed: u64,
        /// Maximum admitted value.
        maximum: u64,
    },
    /// A frame-record count could not be represented canonically.
    RecordCountOverflow,
    /// Canonical encoding or hashing failed.
    Codec(CodecError),
    /// Receipt identity-domain construction failed.
    Domain(DomainError),
}

impl Display for DecodeReceiptError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Plan(error) => write!(formatter, "decode receipt references an invalid plan: {error}"),
            Self::PlanDigestMismatch { expected, observed } => write!(
                formatter,
                "decode receipt plan digest mismatch: expected {expected}, observed {observed}"
            ),
            Self::IdentityMismatch {
                field,
                expected,
                observed,
            } => write!(
                formatter,
                "decode receipt identity {field} mismatch: expected {expected}, observed {observed}"
            ),
            Self::ZeroIdentity { field } => write!(formatter, "decode receipt identity {field} must not be all zero"),
            Self::FailedWithZeroExitCode => formatter.write_str("failed decode termination must carry a nonzero exit code"),
            Self::FrameHashEvidenceMismatch => formatter.write_str("decode receipt framehash object and parsed evidence presence disagree"),
            Self::FrameHashContractMismatch { version, hash_name } => write!(
                formatter,
                "decode receipt framehash contract is version {version} with hash {hash_name:?}; expected version 2 and SHA256"
            ),
            Self::FrameHashByteTotalMismatch { declared, observed } => write!(
                formatter,
                "decode receipt framehash declares {declared} bytes but records sum to {observed}"
            ),
            Self::FrameHashByteTotalOverflow => formatter.write_str("decode receipt framehash record bytes overflow u64"),
            Self::FrameBudgetExceeded { observed, maximum } => write!(formatter, "decode receipt contains {observed} frames; maximum is {maximum}"),
            Self::SampleRangeExceeded { observed, maximum } => write!(formatter, "decode receipt contains {observed} frames for at most {maximum} samples"),
            Self::OutputBudgetExceeded { observed, maximum } => write!(formatter, "decode receipt contains {observed} decoded bytes; maximum is {maximum}"),
            Self::NonCanonicalRecordIndex { expected, observed } => write!(formatter, "decode receipt frame index is {observed}; expected {expected}"),
            Self::FrameCountOverflow => formatter.write_str("decode receipt frame count cannot fit in u64"),
            Self::SuccessMissingFrameHash => formatter.write_str("successful decode receipt is missing framehash evidence"),
            Self::SuccessHasNoFrames => formatter.write_str("successful decode receipt contains no decoded frames"),
            Self::SuccessMissingOutputRoot => formatter.write_str("successful decode receipt is missing a complete output root"),
            Self::NonSuccessPublishedOutputRoot => formatter.write_str("non-success decode receipt must not publish an output root"),
            Self::SuccessfulResourceOverrun {
                field,
                observed,
                maximum,
            } => write!(formatter, "successful decode receipt exceeded {field}: {observed} > {maximum}"),
            Self::RecordCountOverflow => formatter.write_str("decode receipt record count cannot fit in u64"),
            Self::Codec(error) => write!(formatter, "decode receipt codec error: {error}"),
            Self::Domain(error) => write!(formatter, "decode receipt identity-domain error: {error}"),
        }
    }
}

impl Error for DecodeReceiptError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Plan(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for DecodeReceiptError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for DecodeReceiptError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}

fn compare_identity(
    field: &'static str,
    expected: &EvidenceDigest,
    observed: &EvidenceDigest,
) -> Result<(), DecodeReceiptError> {
    reject_zero_digest(field, observed)?;
    if expected == observed {
        Ok(())
    } else {
        Err(DecodeReceiptError::IdentityMismatch {
            field,
            expected: expected.clone(),
            observed: observed.clone(),
        })
    }
}

fn validate_optional_digest(
    field: &'static str,
    value: Option<&EvidenceDigest>,
) -> Result<(), DecodeReceiptError> {
    if let Some(value) = value {
        reject_zero_digest(field, value)?;
    }
    Ok(())
}

fn reject_zero_digest(
    field: &'static str,
    value: &EvidenceDigest,
) -> Result<(), DecodeReceiptError> {
    if value.as_str().bytes().all(|byte| byte == b'0') {
        Err(DecodeReceiptError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn validate_framehash(
    report: &FrameHashReport,
    plan: &MediaDecodePlan,
) -> Result<(), DecodeReceiptError> {
    if report.version != 2 || report.hash_name != "SHA256" {
        return Err(DecodeReceiptError::FrameHashContractMismatch {
            version: report.version,
            hash_name: report.hash_name.to_owned(),
        });
    }
    let count = u64::try_from(report.records.len())
        .map_err(|_| DecodeReceiptError::FrameCountOverflow)?;
    if count > plan.input().max_frames {
        return Err(DecodeReceiptError::FrameBudgetExceeded {
            observed: count,
            maximum: plan.input().max_frames,
        });
    }
    if count > plan.input().max_samples {
        return Err(DecodeReceiptError::SampleRangeExceeded {
            observed: count,
            maximum: plan.input().max_samples,
        });
    }
    let observed_total = report.records.iter().try_fold(0_u64, |total, record| {
        total
            .checked_add(record.byte_length)
            .ok_or(DecodeReceiptError::FrameHashByteTotalOverflow)
    })?;
    if report.total_frame_bytes != observed_total {
        return Err(DecodeReceiptError::FrameHashByteTotalMismatch {
            declared: report.total_frame_bytes,
            observed: observed_total,
        });
    }
    if report.total_frame_bytes > plan.input().max_output_bytes {
        return Err(DecodeReceiptError::OutputBudgetExceeded {
            observed: report.total_frame_bytes,
            maximum: plan.input().max_output_bytes,
        });
    }
    for (expected, record) in report.records.iter().enumerate() {
        let expected =
            u64::try_from(expected).map_err(|_| DecodeReceiptError::FrameCountOverflow)?;
        if record.record_index != expected {
            return Err(DecodeReceiptError::NonCanonicalRecordIndex {
                expected,
                observed: record.record_index,
            });
        }
    }
    Ok(())
}

fn encode_optional_i32(encoder: &mut Encoder, value: Option<i32>) {
    encoder.put_bool(value.is_some());
    encoder.put_i64(value.map_or(0, i64::from));
}

fn encode_optional_digest(encoder: &mut Encoder, value: Option<&EvidenceDigest>) {
    encoder.put_bool(value.is_some());
    if let Some(value) = value {
        encoder.put_digest(value);
    }
}

fn encode_framehash(
    encoder: &mut Encoder,
    report: Option<&FrameHashReport>,
) -> Result<(), DecodeReceiptError> {
    encoder.put_bool(report.is_some());
    let Some(report) = report else {
        return Ok(());
    };
    encoder.put_u32(report.version);
    encoder.put_str(report.hash_name)?;
    encoder.put_u64(report.total_frame_bytes);
    let count = u64::try_from(report.records.len())
        .map_err(|_| DecodeReceiptError::RecordCountOverflow)?;
    encoder.put_u64(count);
    for record in &report.records {
        encode_record(encoder, record);
    }
    Ok(())
}

fn encode_record(encoder: &mut Encoder, record: &FrameHashRecord) {
    encoder.put_u64(record.record_index);
    encoder.put_u32(record.stream_index);
    encoder.put_i64(record.dts);
    encoder.put_i64(record.pts);
    encoder.put_u64(record.duration);
    encoder.put_u64(record.byte_length);
    encoder.put_digest(&record.digest);
}

#[cfg(test)]
mod tests {
    use super::{
        DecodeReceiptError, DecodeTermination, MediaDecodeReceipt, MediaDecodeReceiptInput,
    };
    use crate::{
        DecodePixelFormat, FrameHashLimits, MediaDecodePlan, MediaDecodePlanInput,
        parse_framehash_v2,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn plan() -> Option<MediaDecodePlan> {
        MediaDecodePlan::new(MediaDecodePlanInput {
            source_root_manifest_digest: digest(1),
            source_manifest_digest: digest(2),
            source_object_digest: digest(3),
            source_object_length: 1_000,
            track_id: 1,
            start_sample: 0,
            max_samples: 2,
            pixel_format: DecodePixelFormat::Gray8,
            output_width: 2,
            output_height: 2,
            max_frames: 2,
            max_output_bytes: 8,
            max_wall_time_ms: 1_000,
            max_memory_bytes: 1_000_000,
            worker_executable_digest: digest(4),
            worker_version_digest: digest(5),
            profile_digest: digest(6),
            worker_threads: 1,
            network_allowed: false,
            deterministic: true,
        })
        .ok()
    }

    fn report() -> Option<crate::FrameHashReport> {
        let a = "ab".repeat(32);
        let b = "cd".repeat(32);
        parse_framehash_v2(
            &format!(
                "#version: 2\n#hash: SHA256\n0, 0, 0, 1, 4, {a}\n0, 1, 1, 1, 4, {b}\n"
            ),
            FrameHashLimits::default(),
        )
        .ok()
    }

    fn successful(plan: &MediaDecodePlan) -> Option<MediaDecodeReceipt> {
        let plan_digest = plan.digest().ok()?;
        Some(MediaDecodeReceipt::new(MediaDecodeReceiptInput {
            plan_digest,
            worker_executable_digest: plan.input().worker_executable_digest.clone(),
            worker_version_digest: plan.input().worker_version_digest.clone(),
            profile_digest: plan.input().profile_digest.clone(),
            termination: DecodeTermination::Succeeded,
            framehash_object_digest: Some(digest(7)),
            framehash: report(),
            output_root_manifest_digest: Some(digest(8)),
            output_root_object_digest: Some(digest(9)),
            wall_time_ms: 500,
            peak_memory_bytes: 500_000,
            stderr_digest: digest(10),
        }))
    }

    #[test]
    fn successful_receipt_is_deterministic_and_semantically_complete() {
        let plan = plan();
        assert!(plan.is_some());
        if let Some(plan) = plan {
            let first = successful(&plan);
            let second = successful(&plan);
            assert!(matches!(
                (&first, &second),
                (Some(left), Some(right))
                    if left.semantic_completion(&plan)
                        && left.digest(&plan) == right.digest(&plan)
            ));
        }
    }

    #[test]
    fn killed_after_grace_is_indeterminate_and_cannot_publish_success() {
        assert!(DecodeTermination::KilledAfterGrace.is_indeterminate());
        let plan = plan();
        assert!(plan.is_some());
        if let Some(plan) = plan {
            let plan_digest = plan.digest();
            assert!(plan_digest.is_ok());
            if let Ok(plan_digest) = plan_digest {
                let receipt = MediaDecodeReceipt::new(MediaDecodeReceiptInput {
                    plan_digest,
                    worker_executable_digest: plan.input().worker_executable_digest.clone(),
                    worker_version_digest: plan.input().worker_version_digest.clone(),
                    profile_digest: plan.input().profile_digest.clone(),
                    termination: DecodeTermination::KilledAfterGrace,
                    framehash_object_digest: None,
                    framehash: None,
                    output_root_manifest_digest: Some(digest(8)),
                    output_root_object_digest: Some(digest(9)),
                    wall_time_ms: 1_001,
                    peak_memory_bytes: 0,
                    stderr_digest: digest(10),
                });
                assert!(matches!(
                    receipt.validate_against(&plan),
                    Err(DecodeReceiptError::NonSuccessPublishedOutputRoot)
                ));
            }
        }
    }

    #[test]
    fn mismatched_plan_identity_is_refused() {
        let plan = plan();
        assert!(plan.is_some());
        if let Some(plan) = plan {
            let receipt = successful(&plan);
            assert!(receipt.is_some());
            if let Some(receipt) = receipt {
                let mut other_input = plan.input().clone();
                other_input.start_sample = 1;
                let other = MediaDecodePlan::new(other_input);
                assert!(other.is_ok());
                if let Ok(other) = other {
                    assert!(matches!(
                        receipt.validate_against(&other),
                        Err(DecodeReceiptError::PlanDigestMismatch { .. })
                    ));
                }
            }
        }
    }
}
