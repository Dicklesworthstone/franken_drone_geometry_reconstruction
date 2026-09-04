/// Maximum absolute component-relative camera-point coordinate.
pub const MAX_CAMERA_POINT_ABS_NANO: u64 = 9_000_000_000_000_000;
/// Maximum absolute projected coordinate admitted by the scalar reference kernel.
pub const MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS: u64 = 9_000_000_000_000_000;

/// Semantic visibility and numeric bounds for one rectified projection.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct RectifiedProjectionPolicy {
    /// Smallest positive camera-frame depth accepted by the projection.
    pub minimum_positive_depth_nano: u64,
    /// Maximum absolute projected coordinate before the result is refused.
    pub max_projected_coordinate_abs_nano_pixels: u64,
}

impl Default for RectifiedProjectionPolicy {
    fn default() -> Self {
        Self {
            minimum_positive_depth_nano: 1_000,
            max_projected_coordinate_abs_nano_pixels:
                MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS,
        }
    }
}

/// One deterministic image projection retaining visibility and depth evidence.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ProjectedPixel {
    /// Horizontal top-left image coordinate in nano-pixels.
    pub x_nano_pixels: i64,
    /// Vertical top-left image coordinate in nano-pixels.
    pub y_nano_pixels: i64,
    /// Positive camera-frame depth in component-relative nano units.
    pub depth_nano: i64,
    /// Whether the coordinate lies in the exact half-open calibrated image domain.
    pub within_image_domain: bool,
}

/// Stable failures for the deliberately narrow rectified projection kernel.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ProjectionError {
    /// The exact derived calibration failed its own invariants.
    Calibration(CalibrationError),
    /// Distorted coordinates require a separately admitted distortion kernel.
    UnsupportedDistortion {
        /// Canonical calibration model name.
        model: &'static str,
    },
    /// Rolling-shutter projection requires a time-varying pose model.
    RollingShutterRequiresMotionModel,
    /// A semantic policy value was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
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
    /// Camera-frame depth was nonpositive or too close to the projection singularity.
    InsufficientPositiveDepth {
        /// Observed signed depth.
        depth_nano: i64,
        /// Required positive minimum.
        minimum_positive_depth_nano: u64,
    },
    /// Checked fixed-point arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation name.
        field: &'static str,
    },
}

impl Display for ProjectionError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Calibration(error) => write!(formatter, "projection calibration error: {error}"),
            Self::UnsupportedDistortion { model } => write!(
                formatter,
                "projection model {model} is unsupported; an exact rectified domain is required"
            ),
            Self::RollingShutterRequiresMotionModel => formatter.write_str(
                "rolling-shutter projection requires an admitted time-varying pose model",
            ),
            Self::ZeroValue { field } => {
                write!(formatter, "projection field {field} must be nonzero")
            }
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "projection field {field} is {actual}; maximum is {maximum}"
            ),
            Self::InsufficientPositiveDepth {
                depth_nano,
                minimum_positive_depth_nano,
            } => write!(
                formatter,
                "camera depth {depth_nano} is below required positive minimum {minimum_positive_depth_nano}"
            ),
            Self::ArithmeticOverflow { field } => write!(
                formatter,
                "projection arithmetic overflowed while computing {field}"
            ),
        }
    }
}

impl Error for ProjectionError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Calibration(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CalibrationError> for ProjectionError {
    fn from(error: CalibrationError) -> Self {
        Self::Calibration(error)
    }
}
