#![forbid(unsafe_code)]
//! Canonical identities and state-machine types for FDGR.

use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// A canonical `SHA-256` digest rendered as 64 lowercase hexadecimal characters.
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

    /// Constructs the canonical textual digest from its 32 raw bytes.
    #[must_use]
    pub fn from_bytes(bytes: [u8; 32]) -> Self {
        let mut text = String::with_capacity(64);
        for byte in bytes {
            text.push(hex_digit(byte >> 4));
            text.push(hex_digit(byte & 0x0f));
        }
        Self(text)
    }

    /// Returns the canonical lowercase hexadecimal representation.
    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }

    /// Returns the raw 32-byte digest.
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 32] {
        let mut bytes = [0_u8; 32];
        for (slot, pair) in bytes.iter_mut().zip(self.0.as_bytes().chunks_exact(2)) {
            let [high, low] = pair else {
                continue;
            };
            *slot = (hex_value(*high) << 4) | hex_value(*low);
        }
        bytes
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

/// A canonical, versioned domain separator for typed FDGR identities.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct DigestDomain(String);

impl DigestDomain {
    /// Maximum accepted byte length for one domain separator.
    pub const MAX_BYTES: usize = 96;

    /// Parses a domain such as `fdgr.evidence_chunk/1`.
    ///
    /// # Errors
    ///
    /// Returns a typed error when the domain is empty, too long, lacks the `fdgr.` prefix,
    /// lacks an explicit slash version, or contains a noncanonical byte.
    pub fn parse(value: impl AsRef<str>) -> Result<Self, DomainError> {
        let value = value.as_ref();
        if value.is_empty() {
            return Err(DomainError::Empty);
        }
        if value.len() > Self::MAX_BYTES {
            return Err(DomainError::TooLong {
                actual: value.len(),
                maximum: Self::MAX_BYTES,
            });
        }
        if !value.starts_with("fdgr.") {
            return Err(DomainError::MissingPrefix);
        }
        let Some((name, version)) = value.rsplit_once('/') else {
            return Err(DomainError::MissingVersion);
        };
        if name.len() <= "fdgr.".len()
            || version.is_empty()
            || !version.bytes().all(|byte| byte.is_ascii_digit())
        {
            return Err(DomainError::MissingVersion);
        }
        if !value.bytes().all(|byte| {
            matches!(
                byte,
                b'a'..=b'z' | b'0'..=b'9' | b'.' | b'_' | b'-' | b'/'
            )
        }) {
            return Err(DomainError::NonCanonicalByte);
        }
        Ok(Self(value.to_owned()))
    }

    /// Returns the canonical domain text.
    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl Display for DigestDomain {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.0)
    }
}

/// Validation errors for digest-domain text.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DomainError {
    /// The supplied domain was empty.
    Empty,
    /// The supplied domain exceeded the hard byte bound.
    TooLong {
        /// Actual byte length.
        actual: usize,
        /// Maximum accepted byte length.
        maximum: usize,
    },
    /// The domain does not begin with `fdgr.`.
    MissingPrefix,
    /// The domain has no explicit numeric slash version.
    MissingVersion,
    /// The domain contains an uppercase, whitespace, or otherwise unsupported byte.
    NonCanonicalByte,
}

impl Display for DomainError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Empty => formatter.write_str("digest domain must not be empty"),
            Self::TooLong { actual, maximum } => write!(
                formatter,
                "digest domain is {actual} bytes; maximum is {maximum}"
            ),
            Self::MissingPrefix => formatter.write_str("digest domain must begin with `fdgr.`"),
            Self::MissingVersion => {
                formatter.write_str("digest domain must end with an explicit numeric slash version")
            }
            Self::NonCanonicalByte => formatter.write_str(
                "digest domain may contain only lowercase ASCII letters, digits, '.', '_', '-', and '/'",
            ),
        }
    }
}

impl Error for DomainError {}

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

const fn hex_digit(value: u8) -> char {
    match value {
        0 => '0',
        1 => '1',
        2 => '2',
        3 => '3',
        4 => '4',
        5 => '5',
        6 => '6',
        7 => '7',
        8 => '8',
        9 => '9',
        10 => 'a',
        11 => 'b',
        12 => 'c',
        13 => 'd',
        14 => 'e',
        15 => 'f',
        _ => '\u{fffd}',
    }
}

const fn hex_value(value: u8) -> u8 {
    match value {
        b'0'..=b'9' => value - b'0',
        b'a'..=b'f' => value - b'a' + 10,
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::{DigestDomain, DigestError, DomainError, EvidenceDigest, ScaleStatus};

    #[test]
    fn digest_accepts_canonical_sha256_text() {
        let text = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        let parsed = EvidenceDigest::parse(text);
        assert!(matches!(parsed, Ok(ref value) if value.as_str() == text));
    }

    #[test]
    fn digest_raw_bytes_round_trip() {
        let bytes = [0xab; 32];
        let digest = EvidenceDigest::from_bytes(bytes);
        assert_eq!(digest.as_str(), "abababababababababababababababababababababababababababababababab");
        assert_eq!(digest.to_bytes(), bytes);
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
    fn domains_require_canonical_namespace_and_version() {
        assert!(matches!(
            DigestDomain::parse("fdgr.evidence_chunk/1"),
            Ok(ref domain) if domain.as_str() == "fdgr.evidence_chunk/1"
        ));
        assert!(matches!(
            DigestDomain::parse("evidence_chunk/1"),
            Err(DomainError::MissingPrefix)
        ));
        assert!(matches!(
            DigestDomain::parse("fdgr.evidence_chunk"),
            Err(DomainError::MissingVersion)
        ));
        assert!(matches!(
            DigestDomain::parse("fdgr.Evidence/1"),
            Err(DomainError::NonCanonicalByte)
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
