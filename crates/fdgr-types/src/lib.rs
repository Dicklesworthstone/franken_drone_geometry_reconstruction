#![forbid(unsafe_code)]
//! Canonical identities and state-machine types for FDGR.

use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// A domain-separated `SHA-256` digest rendered as 64 lowercase hexadecimal characters.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct EvidenceDigest(String);

impl EvidenceDigest {
    /// Parses and validates a canonical digest string.
    ///
    /// # Errors
    ///
    /// Returns [`DigestError::WrongLength`] when the text is not exactly 64 bytes and
    /// [`DigestError::NonCanonicalHex`] when it contains anything other than lowercase
    /// hexadecimal digits.
    pub fn parse(value: impl AsRef<str>) -> Result<Self, DigestError> {
        let value = value.as_ref();
        if value.len() != 64 {
            return Err(DigestError::WrongLength {
                actual: value.len(),
            });
        }
        if !value
            .bytes()
            .all(|byte| matches!(byte, b'0'..=b'9' | b'a'..=b'f'))
        {
            return Err(DigestError::NonCanonicalHex);
        }
        Ok(Self(value.to_owned()))
    }

    /// Returns the canonical lowercase hexadecimal representation.
    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl Display for EvidenceDigest {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.0)
    }
}

/// Validation errors for canonical digest text.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DigestError {
    /// The text does not contain exactly 64 hexadecimal bytes.
    WrongLength {
        /// Actual byte length supplied by the caller.
        actual: usize,
    },
    /// The text contains uppercase or non-hexadecimal characters.
    NonCanonicalHex,
}

impl Display for DigestError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::WrongLength { actual } => {
                write!(formatter, "digest must be 64 bytes; received {actual}")
            }
            Self::NonCanonicalHex => formatter
                .write_str("digest must contain only canonical lowercase hexadecimal characters"),
        }
    }
}

impl Error for DigestError {}

/// Monotonic capture lineage epoch. A discontinuity creates a new epoch instead of rewriting time.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct CaptureEpoch(
    /// Raw monotonic epoch value.
    pub u64,
);

/// Monotonic frame publication sequence inside one capture epoch.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct FrameSequence(
    /// Raw monotonic frame sequence value.
    pub u64,
);

/// Monotonic calibration generation.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct CalibrationEpoch(
    /// Raw monotonic calibration generation value.
    pub u64,
);

/// Monotonic clock-model generation.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ClockEpoch(
    /// Raw monotonic clock-model generation value.
    pub u64,
);

/// A complete immutable basis for one reconstruction request or publication.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ReconstructionAnchor {
    /// Stable capture lineage identity.
    pub capture_root: EvidenceDigest,
    /// Capture epoch containing the referenced observations.
    pub capture_epoch: CaptureEpoch,
    /// Highest consecutively published frame included in the basis.
    pub frame_high_water: FrameSequence,
    /// Calibration generation used for projection and unprojection.
    pub calibration_epoch: CalibrationEpoch,
    /// Clock model used to align video and telemetry.
    pub clock_epoch: ClockEpoch,
    /// Exact policy registry root.
    pub policy_root: EvidenceDigest,
    /// Exact admitted model-registry root.
    pub model_registry_root: EvidenceDigest,
    /// Root of the immutable source evidence graph.
    pub evidence_root: EvidenceDigest,
}

/// How strongly the coordinate system is anchored to metric units.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ScaleStatus {
    /// Geometry is projective or relative; no metric claim is permitted.
    RelativeOnly,
    /// A model or weak prior proposes scale, but independent evidence has not verified it.
    Estimated,
    /// One or more registered scale witnesses establish a measured metric transform.
    Witnessed,
    /// A survey-grade reference process establishes the metric transform for a named domain.
    Surveyed,
}

impl ScaleStatus {
    /// Returns whether dimensions may be rendered in metric units without an explicit warning.
    #[must_use]
    pub const fn permits_metric_claim(self) -> bool {
        matches!(self, Self::Witnessed | Self::Surveyed)
    }
}

/// Claim maturity in the canonical evidence graph.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ClaimDisposition {
    /// A sensor, model, or human produced a raw observation.
    Observation,
    /// The system has formed a testable interpretation from observations.
    Hypothesis,
    /// Registered evidence gates have resolved the hypothesis for the named scope.
    Resolved,
    /// Evidence contradicted or invalidated the claim.
    Rejected,
    /// The system cannot decide from available evidence.
    Indeterminate,
}

/// Multi-artifact publication lifecycle.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PublicationStage {
    /// Identity, inputs, authority, and budget are reserved.
    Reserved,
    /// Children are being written below an unpublished root.
    Materializing,
    /// Every child has passed its structural verification.
    Verified,
    /// The root is atomically visible to readers.
    Published,
    /// Work ended before publication and remains unreachable.
    Aborted,
    /// Publication outcome cannot yet be established.
    Indeterminate,
}

/// A typed evidence class used to prevent semantic outputs from masquerading as geometry proof.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum EvidenceClass {
    /// Original encoded bytes and acquisition metadata.
    RawCapture,
    /// Decoded image, timing, telemetry, or calibration observation.
    SensorObservation,
    /// Geometric correspondence, pose, depth, fusion, or scale evidence.
    Geometry,
    /// Object, region, material, or utility interpretation.
    Semantic,
    /// Human measurement, annotation, or confirmation.
    Human,
    /// Test, benchmark, recovery, or qualification receipt.
    Qualification,
}

#[cfg(test)]
mod tests {
    use super::{DigestError, EvidenceDigest, ScaleStatus};

    #[test]
    fn digest_accepts_canonical_sha256_text() {
        let text = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        let parsed = EvidenceDigest::parse(text);
        assert!(matches!(parsed, Ok(ref value) if value.as_str() == text));
    }

    #[test]
    fn digest_rejects_uppercase() {
        let text = "A123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        assert!(matches!(
            EvidenceDigest::parse(text),
            Err(DigestError::NonCanonicalHex)
        ));
    }

    #[test]
    fn relative_scale_never_permits_metric_claims() {
        assert!(!ScaleStatus::RelativeOnly.permits_metric_claim());
        assert!(!ScaleStatus::Estimated.permits_metric_claim());
        assert!(ScaleStatus::Witnessed.permits_metric_claim());
        assert!(ScaleStatus::Surveyed.permits_metric_claim());
    }
}
