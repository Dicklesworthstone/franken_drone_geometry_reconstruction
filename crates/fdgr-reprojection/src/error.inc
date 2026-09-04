/// Stable reprojection-evaluation failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ReprojectionError {
    /// A mandatory identity was all zero.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A mandatory numeric value was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
    },
    /// A semantic minimum was not met.
    BelowMinimum {
        /// Stable field name.
        field: &'static str,
        /// Observed value.
        actual: u128,
        /// Required minimum.
        minimum: u128,
    },
    /// A constitutional bound was exceeded.
    BoundExceeded {
        /// Stable field name.
        field: &'static str,
        /// Observed magnitude.
        actual: u128,
        /// Maximum admitted magnitude.
        maximum: u128,
    },
    /// One exact basis identity disagreed with independently recomputed evidence.
    BasisMismatch {
        /// Stable basis field.
        field: &'static str,
        /// Required identity.
        expected: EvidenceDigest,
        /// Independently observed identity.
        observed: EvidenceDigest,
    },
    /// A camera calibration row was duplicated.
    DuplicateCalibration {
        /// Duplicate camera node.
        camera_node_id: u64,
    },
    /// An audited camera lacked a materialized calibration.
    MissingCalibration {
        /// Camera node.
        camera_node_id: u64,
    },
    /// A calibration row named no audited camera.
    ExtraCalibration {
        /// Camera node.
        camera_node_id: u64,
    },
    /// Calibration frame identity disagreed with the audited camera binding.
    CalibrationFrameMismatch {
        /// Camera node.
        camera_node_id: u64,
    },
    /// Calibration identity disagreed with the audited camera binding or materialized object.
    CalibrationDigestMismatch {
        /// Camera node.
        camera_node_id: u64,
    },
    /// Materialized calibration dimensions disagreed with the audited image domain.
    CalibrationDimensionMismatch {
        /// Camera node.
        camera_node_id: u64,
        /// Expected width.
        expected_width: u32,
        /// Expected height.
        expected_height: u32,
        /// Observed width.
        observed_width: u32,
        /// Observed height.
        observed_height: u32,
    },
    /// A required upstream record was absent after upstream validation.
    InternalLookup {
        /// Stable lookup name.
        field: &'static str,
    },
    /// Deterministic work exceeded its explicit ceiling.
    OperationBudgetExceeded {
        /// Attempted operation count.
        attempted: u64,
        /// Admitted maximum.
        maximum: u64,
    },
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation name.
        field: &'static str,
    },
    /// Deterministic replay disagreed with retained output.
    DerivedMismatch {
        /// Stable derived field.
        field: &'static str,
    },
    /// Audited bundle-admission validation failed.
    BundleAdmission(BundleAdmissionError),
    /// A materialized calibration failed validation.
    Calibration(CalibrationError),
    /// Canonical codec failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for ReprojectionError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => {
                write!(formatter, "reprojection identity {field} must not be all zero")
            }
            Self::ZeroValue { field } => {
                write!(formatter, "reprojection field {field} must be nonzero")
            }
            Self::BelowMinimum {
                field,
                actual,
                minimum,
            } => write!(
                formatter,
                "reprojection field {field} is {actual}; minimum is {minimum}"
            ),
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "reprojection field {field} is {actual}; maximum is {maximum}"
            ),
            Self::BasisMismatch {
                field,
                expected,
                observed,
            } => write!(
                formatter,
                "reprojection basis {field} expected {expected}, observed {observed}"
            ),
            Self::DuplicateCalibration { camera_node_id } => write!(
                formatter,
                "reprojection calibration for camera {camera_node_id} is duplicated"
            ),
            Self::MissingCalibration { camera_node_id } => write!(
                formatter,
                "audited camera {camera_node_id} lacks a materialized calibration"
            ),
            Self::ExtraCalibration { camera_node_id } => write!(
                formatter,
                "reprojection calibration names unknown camera {camera_node_id}"
            ),
            Self::CalibrationFrameMismatch { camera_node_id } => write!(
                formatter,
                "reprojection calibration for camera {camera_node_id} uses the wrong frame identity"
            ),
            Self::CalibrationDigestMismatch { camera_node_id } => write!(
                formatter,
                "reprojection calibration for camera {camera_node_id} uses the wrong effective calibration identity"
            ),
            Self::CalibrationDimensionMismatch {
                camera_node_id,
                expected_width,
                expected_height,
                observed_width,
                observed_height,
            } => write!(
                formatter,
                "reprojection calibration for camera {camera_node_id} has dimensions {observed_width}x{observed_height}; expected {expected_width}x{expected_height}"
            ),
            Self::InternalLookup { field } => write!(
                formatter,
                "reprojection internal lookup failed for {field}"
            ),
            Self::OperationBudgetExceeded { attempted, maximum } => write!(
                formatter,
                "reprojection evaluation attempted operation {attempted}; maximum is {maximum}"
            ),
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "reprojection arithmetic overflowed while computing {field}"
            ),
            Self::DerivedMismatch { field } => write!(
                formatter,
                "reprojection derived field {field} disagrees with deterministic replay"
            ),
            Self::BundleAdmission(error) => {
                write!(formatter, "reprojection bundle-admission error: {error}")
            }
            Self::Calibration(error) => {
                write!(formatter, "reprojection calibration error: {error}")
            }
            Self::Codec(error) => write!(formatter, "reprojection codec error: {error}"),
            Self::Domain(error) => {
                write!(formatter, "reprojection identity-domain error: {error}")
            }
            Self::JsonRendering(error) => {
                write!(formatter, "reprojection JSON rendering failed: {error}")
            }
        }
    }
}

impl Error for ReprojectionError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::BundleAdmission(error) => Some(error),
            Self::Calibration(error) => Some(error),
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<BundleAdmissionError> for ReprojectionError {
    fn from(error: BundleAdmissionError) -> Self {
        Self::BundleAdmission(error)
    }
}

impl From<CalibrationError> for ReprojectionError {
    fn from(error: CalibrationError) -> Self {
        Self::Calibration(error)
    }
}

impl From<CodecError> for ReprojectionError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for ReprojectionError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}
