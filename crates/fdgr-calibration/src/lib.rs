#![forbid(unsafe_code)]
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::similar_names,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_lines
)]
//! Deterministic camera calibration and image-derivation semantics for FDGR.
//!
//! The reference format uses fixed integer units, exact evidence identities, explicit coordinate
//! conventions, lens-state scope, distortion, rolling shutter, and rigid extrinsics. It does not
//! estimate calibration or infer any missing vendor transform.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

/// Public schema identity for one admitted calibration proposal.
pub const CALIBRATION_MODEL_SCHEMA: &str = "fdgr.calibration_model/1";
/// Public schema identity for one exact image-geometry derivation.
pub const DERIVED_CALIBRATION_SCHEMA: &str = "fdgr.derived_calibration/1";
/// Fixed scale for nano-pixel and dimensionless nano-unit values.
pub const NANO_SCALE: i64 = 1_000_000_000;
/// Maximum image width or height admitted by the reference model.
pub const MAX_IMAGE_DIMENSION: u32 = 65_535;
/// Maximum absolute intrinsic value in nano-pixels.
pub const MAX_INTRINSIC_NANO_PIXELS: i64 = 1_000_000_000_000_000;
/// Maximum absolute Brown-Conrady coefficient in nano-units.
pub const MAX_DISTORTION_NANO: i64 = 1_000_000_000_000;
/// Maximum rolling-shutter readout time.
pub const MAX_READOUT_TIME_NS: u64 = 10_000_000_000;
/// Maximum absolute translation component in micrometers.
pub const MAX_TRANSLATION_MICROMETERS: i64 = 1_000_000_000;
/// Rotation orthonormality tolerance in squared nano-units.
const ROTATION_DOT_TOLERANCE: i128 = 1_000_000_000_000;
/// Rotation determinant tolerance in cubed nano-units.
const ROTATION_DETERMINANT_TOLERANCE: i128 = 1_000_000_000_000_000_000_000;

/// Pixel coordinates use top-left origin and half-pixel centers.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PixelConvention {
    /// Pixel `(0, 0)` occupies `[0, 1) × [0, 1)` and has center `(0.5, 0.5)`.
    TopLeftHalfPixel,
}

impl PixelConvention {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::TopLeftHalfPixel => "top_left_half_pixel",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::TopLeftHalfPixel => 1,
        }
    }
}

/// Pinhole intrinsics in signed nano-pixel units.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PinholeIntrinsics {
    /// Horizontal focal length multiplied by [`NANO_SCALE`].
    pub fx_nano_pixels: i64,
    /// Vertical focal length multiplied by [`NANO_SCALE`].
    pub fy_nano_pixels: i64,
    /// Principal point x multiplied by [`NANO_SCALE`].
    pub cx_nano_pixels: i64,
    /// Principal point y multiplied by [`NANO_SCALE`].
    pub cy_nano_pixels: i64,
    /// Pixel-coordinate convention.
    pub pixel_convention: PixelConvention,
}

/// Explicit lens-distortion model.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum DistortionModel {
    /// No distortion is asserted for the admitted image domain.
    None,
    /// Brown-Conrady radial/tangential coefficients in nano-units.
    BrownConrady {
        /// First radial coefficient.
        k1_nano: i64,
        /// Second radial coefficient.
        k2_nano: i64,
        /// First tangential coefficient.
        p1_nano: i64,
        /// Second tangential coefficient.
        p2_nano: i64,
        /// Third radial coefficient.
        k3_nano: i64,
    },
}

impl DistortionModel {
    /// Canonical model name.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::None => "none",
            Self::BrownConrady { .. } => "brown_conrady",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::None => 0,
            Self::BrownConrady { .. } => 1,
        }
    }
}

/// Physical readout direction for a rolling shutter.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ReadoutDirection {
    /// Earliest rows are at the top.
    TopToBottom,
    /// Earliest rows are at the bottom.
    BottomToTop,
    /// Earliest columns are at the left.
    LeftToRight,
    /// Earliest columns are at the right.
    RightToLeft,
}

impl ReadoutDirection {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::TopToBottom => "top_to_bottom",
            Self::BottomToTop => "bottom_to_top",
            Self::LeftToRight => "left_to_right",
            Self::RightToLeft => "right_to_left",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::TopToBottom => 1,
            Self::BottomToTop => 2,
            Self::LeftToRight => 3,
            Self::RightToLeft => 4,
        }
    }
}

/// Explicit global- or rolling-shutter semantics.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ShutterModel {
    /// All pixels share one reference exposure time for this model.
    Global,
    /// Exposure reference varies along one image axis.
    Rolling {
        /// Physical sensor readout direction.
        direction: ReadoutDirection,
        /// Full source-frame first-to-last-line readout time.
        readout_time_ns: u64,
        /// Reference exposure phase in nano-units from 0 to [`NANO_SCALE`].
        reference_phase_nano: u32,
    },
}

impl ShutterModel {
    /// Canonical model name.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Global => "global",
            Self::Rolling { .. } => "rolling",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::Global => 0,
            Self::Rolling { .. } => 1,
        }
    }
}

/// Rigid camera-from-body transform.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct RigidTransform {
    /// Row-major 3×3 rotation matrix in nano-units.
    pub rotation_nano: [i64; 9],
    /// Camera-frame translation of the body origin in micrometers.
    pub translation_micrometers: [i64; 3],
}

impl RigidTransform {
    /// Exact identity transform.
    pub const IDENTITY: Self = Self {
        rotation_nano: [
            NANO_SCALE,
            0,
            0,
            0,
            NANO_SCALE,
            0,
            0,
            0,
            NANO_SCALE,
        ],
        translation_micrometers: [0, 0, 0],
    };
}

/// Scope in which a calibration may be applied.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CalibrationScope {
    /// Exact camera/firmware/profile identity.
    pub camera_profile_digest: EvidenceDigest,
    /// Exact lens/focus/zoom/stabilization-state identity.
    pub lens_state_digest: EvidenceDigest,
    /// Inclusive minimum admitted temperature in milli-Celsius.
    pub minimum_temperature_millicelsius: i32,
    /// Inclusive maximum admitted temperature in milli-Celsius.
    pub maximum_temperature_millicelsius: i32,
}

/// Capture state checked against [`CalibrationScope`].
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CaptureCalibrationState {
    /// Exact camera/firmware/profile identity.
    pub camera_profile_digest: EvidenceDigest,
    /// Exact lens/focus/zoom/stabilization-state identity.
    pub lens_state_digest: EvidenceDigest,
    /// Encoded source width.
    pub source_width: u32,
    /// Encoded source height.
    pub source_height: u32,
    /// Observed camera temperature in milli-Celsius.
    pub temperature_millicelsius: i32,
}

/// Reprojection evidence summary; values are nano-pixels.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CalibrationResiduals {
    /// Number of observations used by the admitted proposal.
    pub observation_count: u64,
    /// Median absolute reprojection residual.
    pub median_reprojection_nano_pixels: u64,
    /// Maximum absolute reprojection residual.
    pub maximum_reprojection_nano_pixels: u64,
    /// Declared symmetric calibration uncertainty.
    pub declared_uncertainty_nano_pixels: u64,
}

/// Immutable calibration proposal bound to exact evidence and scope.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CalibrationModel {
    /// Digest of the exact calibration evidence object.
    pub evidence_digest: EvidenceDigest,
    /// Monotonic calibration epoch.
    pub calibration_epoch: u64,
    /// Immutable model generation inside the epoch.
    pub model_generation: u64,
    /// Source image width.
    pub source_width: u32,
    /// Source image height.
    pub source_height: u32,
    /// Pinhole intrinsics.
    pub intrinsics: PinholeIntrinsics,
    /// Explicit distortion model.
    pub distortion: DistortionModel,
    /// Explicit shutter model.
    pub shutter: ShutterModel,
    /// Camera-from-body rigid transform.
    pub camera_from_body: RigidTransform,
    /// Applicability scope.
    pub scope: CalibrationScope,
    /// Reprojection and uncertainty evidence.
    pub residuals: CalibrationResiduals,
}

/// Exact axis-aligned crop and resize applied to the calibrated source image.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ImageDerivation {
    /// Expected calibrated source width.
    pub source_width: u32,
    /// Expected calibrated source height.
    pub source_height: u32,
    /// Crop left edge in source pixels.
    pub crop_x: u32,
    /// Crop top edge in source pixels.
    pub crop_y: u32,
    /// Crop width in source pixels.
    pub crop_width: u32,
    /// Crop height in source pixels.
    pub crop_height: u32,
    /// Output width after resize.
    pub output_width: u32,
    /// Output height after resize.
    pub output_height: u32,
}

/// Rolling-shutter timing restricted to the observed crop.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct DerivedReadout {
    /// Whether the source model is rolling.
    pub rolling: bool,
    /// Direction when rolling.
    pub direction: Option<ReadoutDirection>,
    /// Time from the source-frame earliest line to the crop's earliest line.
    pub first_observed_line_offset_ns: u64,
    /// First-to-last-line time represented by the crop.
    pub observed_readout_time_ns: u64,
    /// Source reference phase.
    pub reference_phase_nano: u32,
}

/// Calibration transformed into one exact derived image domain.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DerivedCalibration {
    /// Exact source calibration identity.
    pub source_calibration_digest: EvidenceDigest,
    /// Exact image derivation.
    pub derivation: ImageDerivation,
    /// Intrinsics transformed into output pixels.
    pub intrinsics: PinholeIntrinsics,
    /// Distortion remains defined in normalized camera coordinates.
    pub distortion: DistortionModel,
    /// Crop-restricted readout timing.
    pub readout: DerivedReadout,
    /// Unchanged physical camera-from-body transform.
    pub camera_from_body: RigidTransform,
    /// Source calibration uncertainty, before any downstream interpolation uncertainty.
    pub declared_uncertainty_nano_pixels: u64,
}

/// Stable calibration validation and derivation failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CalibrationError {
    /// A mandatory identity was all zero.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A mandatory numeric field was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
    },
    /// A bounded numeric field was invalid.
    BoundExceeded {
        /// Stable field name.
        field: &'static str,
        /// Observed magnitude.
        actual: u128,
        /// Maximum magnitude.
        maximum: u128,
    },
    /// A signed interval was invalid.
    InvalidInterval {
        /// Stable interval name.
        field: &'static str,
    },
    /// Residual summary was internally inconsistent.
    InvalidResidualSummary,
    /// Rotation row/column orthonormality failed.
    RotationNotOrthonormal,
    /// Rotation determinant was not approximately positive one.
    RotationDeterminantInvalid,
    /// Capture profile, lens state, dimensions, or temperature were outside scope.
    CaptureOutsideScope {
        /// Stable reason.
        reason: &'static str,
    },
    /// Derivation source dimensions did not match the calibrated domain.
    SourceDimensionMismatch,
    /// Crop rectangle exceeded the source domain.
    CropOutsideSource,
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation.
        field: &'static str,
    },
    /// Canonical encoding or hashing failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for CalibrationError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => write!(formatter, "calibration identity {field} must not be all zero"),
            Self::ZeroValue { field } => write!(formatter, "calibration field {field} must be nonzero"),
            Self::BoundExceeded { field, actual, maximum } => write!(formatter, "calibration field {field} is {actual}; maximum is {maximum}"),
            Self::InvalidInterval { field } => write!(formatter, "calibration interval {field} is invalid"),
            Self::InvalidResidualSummary => formatter.write_str("calibration residual summary is inconsistent"),
            Self::RotationNotOrthonormal => formatter.write_str("camera-from-body rotation is not orthonormal within tolerance"),
            Self::RotationDeterminantInvalid => formatter.write_str("camera-from-body rotation determinant is not positive one within tolerance"),
            Self::CaptureOutsideScope { reason } => write!(formatter, "capture is outside calibration scope: {reason}"),
            Self::SourceDimensionMismatch => formatter.write_str("image derivation source dimensions do not match calibration"),
            Self::CropOutsideSource => formatter.write_str("image derivation crop is outside the calibrated source image"),
            Self::ArithmeticOverflow { field } => write!(formatter, "calibration arithmetic overflowed while computing {field}"),
            Self::Codec(error) => write!(formatter, "calibration codec error: {error}"),
            Self::Domain(error) => write!(formatter, "calibration identity-domain error: {error}"),
            Self::JsonRendering(error) => write!(formatter, "calibration JSON rendering failed: {error}"),
        }
    }
}

impl Error for CalibrationError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for CalibrationError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for CalibrationError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}

impl CalibrationModel {
    /// Validates every calibration invariant.
    ///
    /// # Errors
    ///
    /// Returns a stable identity, numeric, geometry, residual, or scope error.
    pub fn validate(&self) -> Result<(), CalibrationError> {
        reject_zero_digest("evidence_digest", &self.evidence_digest)?;
        reject_zero_digest("camera_profile_digest", &self.scope.camera_profile_digest)?;
        reject_zero_digest("lens_state_digest", &self.scope.lens_state_digest)?;
        validate_nonzero("calibration_epoch", self.calibration_epoch)?;
        validate_nonzero("model_generation", self.model_generation)?;
        validate_dimensions(self.source_width, self.source_height)?;
        validate_intrinsics(self.intrinsics, self.source_width, self.source_height)?;
        validate_distortion(self.distortion)?;
        validate_shutter(self.shutter)?;
        validate_rigid_transform(self.camera_from_body)?;
        validate_scope(&self.scope)?;
        validate_residuals(self.residuals)?;
        Ok(())
    }

    /// Checks whether one exact capture state lies inside the admitted scope.
    ///
    /// # Errors
    ///
    /// Returns a model-validation or scope-mismatch error.
    pub fn admit_capture(
        &self,
        capture: &CaptureCalibrationState,
    ) -> Result<(), CalibrationError> {
        self.validate()?;
        if capture.camera_profile_digest != self.scope.camera_profile_digest {
            return Err(CalibrationError::CaptureOutsideScope {
                reason: "camera_profile_digest",
            });
        }
        if capture.lens_state_digest != self.scope.lens_state_digest {
            return Err(CalibrationError::CaptureOutsideScope {
                reason: "lens_state_digest",
            });
        }
        if capture.source_width != self.source_width || capture.source_height != self.source_height {
            return Err(CalibrationError::CaptureOutsideScope {
                reason: "source_dimensions",
            });
        }
        if capture.temperature_millicelsius < self.scope.minimum_temperature_millicelsius
            || capture.temperature_millicelsius > self.scope.maximum_temperature_millicelsius
        {
            return Err(CalibrationError::CaptureOutsideScope {
                reason: "temperature",
            });
        }
        Ok(())
    }

    /// Returns deterministic canonical model bytes.
    ///
    /// # Errors
    ///
    /// Returns a validation or codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, CalibrationError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(512);
        encoder.put_str(CALIBRATION_MODEL_SCHEMA)?;
        encoder.put_digest(&self.evidence_digest);
        encoder.put_u64(self.calibration_epoch);
        encoder.put_u64(self.model_generation);
        encoder.put_u32(self.source_width);
        encoder.put_u32(self.source_height);
        encode_intrinsics(&mut encoder, self.intrinsics)?;
        encode_distortion(&mut encoder, self.distortion)?;
        encode_shutter(&mut encoder, self.shutter)?;
        encode_rigid_transform(&mut encoder, self.camera_from_body)?;
        encoder.put_digest(&self.scope.camera_profile_digest);
        encoder.put_digest(&self.scope.lens_state_digest);
        encoder.put_i64(i64::from(self.scope.minimum_temperature_millicelsius));
        encoder.put_i64(i64::from(self.scope.maximum_temperature_millicelsius));
        encoder.put_u64(self.residuals.observation_count);
        encoder.put_u64(self.residuals.median_reprojection_nano_pixels);
        encoder.put_u64(self.residuals.maximum_reprojection_nano_pixels);
        encoder.put_u64(self.residuals.declared_uncertainty_nano_pixels);
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated calibration identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, CalibrationError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(CALIBRATION_MODEL_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Renders deterministic field-ordered JSON.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, CalibrationError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{CALIBRATION_MODEL_SCHEMA}\",\"calibration_digest\":\"{digest}\",\"evidence_digest\":\"{}\",\"calibration_epoch\":{},\"model_generation\":{},\"source_width\":{},\"source_height\":{},\"pixel_convention\":\"{}\",\"fx_nano_pixels\":{},\"fy_nano_pixels\":{},\"cx_nano_pixels\":{},\"cy_nano_pixels\":{},\"distortion_model\":\"{}\",\"distortion_coefficients_nano\":[",
            self.evidence_digest,
            self.calibration_epoch,
            self.model_generation,
            self.source_width,
            self.source_height,
            self.intrinsics.pixel_convention.as_str(),
            self.intrinsics.fx_nano_pixels,
            self.intrinsics.fy_nano_pixels,
            self.intrinsics.cx_nano_pixels,
            self.intrinsics.cy_nano_pixels,
            self.distortion.as_str(),
        );
        let coefficients = distortion_coefficients(self.distortion);
        for (position, coefficient) in coefficients.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(output, "{coefficient}").map_err(json_rendering)?;
        }
        output.push_str("],\"shutter_model\":\"");
        output.push_str(self.shutter.as_str());
        output.push_str("\",\"readout_direction\":");
        match self.shutter {
            ShutterModel::Global => output.push_str("null"),
            ShutterModel::Rolling { direction, .. } => {
                write!(output, "\"{}\"", direction.as_str()).map_err(json_rendering)?;
            }
        }
        let (readout_time_ns, reference_phase_nano) = shutter_values(self.shutter);
        write!(
            output,
            ",\"readout_time_ns\":{readout_time_ns},\"reference_phase_nano\":{reference_phase_nano},\"rotation_nano\":["
        )
        .map_err(json_rendering)?;
        push_i64_array(&mut output, &self.camera_from_body.rotation_nano)?;
        output.push_str("],\"translation_micrometers\":[");
        push_i64_array(&mut output, &self.camera_from_body.translation_micrometers)?;
        write!(
            output,
            "],\"camera_profile_digest\":\"{}\",\"lens_state_digest\":\"{}\",\"minimum_temperature_millicelsius\":{},\"maximum_temperature_millicelsius\":{},\"observation_count\":{},\"median_reprojection_nano_pixels\":{},\"maximum_reprojection_nano_pixels\":{},\"declared_uncertainty_nano_pixels\":{}}}",
            self.scope.camera_profile_digest,
            self.scope.lens_state_digest,
            self.scope.minimum_temperature_millicelsius,
            self.scope.maximum_temperature_millicelsius,
            self.residuals.observation_count,
            self.residuals.median_reprojection_nano_pixels,
            self.residuals.maximum_reprojection_nano_pixels,
            self.residuals.declared_uncertainty_nano_pixels,
        )
        .map_err(json_rendering)?;
        Ok(output)
    }
}

/// Derives calibration for one exact crop and resize.
///
/// # Errors
///
/// Returns a source-model, crop, dimension, or arithmetic error.
pub fn derive_calibration(
    source: &CalibrationModel,
    derivation: ImageDerivation,
) -> Result<DerivedCalibration, CalibrationError> {
    source.validate()?;
    validate_derivation(source, derivation)?;
    let intrinsics = PinholeIntrinsics {
        fx_nano_pixels: scale_fixed(
            source.intrinsics.fx_nano_pixels,
            derivation.output_width,
            derivation.crop_width,
            "fx",
        )?,
        fy_nano_pixels: scale_fixed(
            source.intrinsics.fy_nano_pixels,
            derivation.output_height,
            derivation.crop_height,
            "fy",
        )?,
        cx_nano_pixels: scale_fixed(
            source
                .intrinsics
                .cx_nano_pixels
                .checked_sub(pixel_edge_nano(derivation.crop_x)?)
                .ok_or(CalibrationError::ArithmeticOverflow { field: "crop_cx" })?,
            derivation.output_width,
            derivation.crop_width,
            "cx",
        )?,
        cy_nano_pixels: scale_fixed(
            source
                .intrinsics
                .cy_nano_pixels
                .checked_sub(pixel_edge_nano(derivation.crop_y)?)
                .ok_or(CalibrationError::ArithmeticOverflow { field: "crop_cy" })?,
            derivation.output_height,
            derivation.crop_height,
            "cy",
        )?,
        pixel_convention: source.intrinsics.pixel_convention,
    };
    validate_intrinsics(intrinsics, derivation.output_width, derivation.output_height)?;
    Ok(DerivedCalibration {
        source_calibration_digest: source.digest()?,
        derivation,
        intrinsics,
        distortion: source.distortion,
        readout: derive_readout(source.shutter, derivation)?,
        camera_from_body: source.camera_from_body,
        declared_uncertainty_nano_pixels: source.residuals.declared_uncertainty_nano_pixels,
    })
}

impl DerivedCalibration {
    /// Validates local derived-calibration invariants.
    ///
    /// # Errors
    ///
    /// Returns a stable identity, dimension, intrinsic, readout, or rigid-transform error.
    pub fn validate(&self) -> Result<(), CalibrationError> {
        reject_zero_digest("source_calibration_digest", &self.source_calibration_digest)?;
        validate_dimensions(self.derivation.source_width, self.derivation.source_height)?;
        validate_dimensions(self.derivation.output_width, self.derivation.output_height)?;
        validate_crop(self.derivation)?;
        validate_intrinsics(
            self.intrinsics,
            self.derivation.output_width,
            self.derivation.output_height,
        )?;
        validate_distortion(self.distortion)?;
        validate_rigid_transform(self.camera_from_body)?;
        if self.readout.reference_phase_nano > u32::try_from(NANO_SCALE).map_err(|_| CalibrationError::ArithmeticOverflow { field: "nano_scale" })? {
            return Err(CalibrationError::BoundExceeded {
                field: "reference_phase_nano",
                actual: u128::from(self.readout.reference_phase_nano),
                maximum: u128::try_from(NANO_SCALE).map_err(|_| CalibrationError::ArithmeticOverflow { field: "nano_scale" })?,
            });
        }
        if !self.readout.rolling
            && (self.readout.direction.is_some()
                || self.readout.first_observed_line_offset_ns != 0
                || self.readout.observed_readout_time_ns != 0)
        {
            return Err(CalibrationError::InvalidInterval {
                field: "global_shutter_readout",
            });
        }
        Ok(())
    }

    /// Returns canonical derived-calibration bytes.
    ///
    /// # Errors
    ///
    /// Returns a validation or codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, CalibrationError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(384);
        encoder.put_str(DERIVED_CALIBRATION_SCHEMA)?;
        encoder.put_digest(&self.source_calibration_digest);
        encode_derivation(&mut encoder, self.derivation);
        encode_intrinsics(&mut encoder, self.intrinsics)?;
        encode_distortion(&mut encoder, self.distortion)?;
        encoder.put_bool(self.readout.rolling);
        encoder.put_u8(self.readout.direction.map_or(0, ReadoutDirection::code));
        encoder.put_u64(self.readout.first_observed_line_offset_ns);
        encoder.put_u64(self.readout.observed_readout_time_ns);
        encoder.put_u32(self.readout.reference_phase_nano);
        encode_rigid_transform(&mut encoder, self.camera_from_body)?;
        encoder.put_u64(self.declared_uncertainty_nano_pixels);
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated derived identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, CalibrationError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(DERIVED_CALIBRATION_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }
}

fn validate_dimensions(width: u32, height: u32) -> Result<(), CalibrationError> {
    for (field, value) in [("image_width", width), ("image_height", height)] {
        if value == 0 {
            return Err(CalibrationError::ZeroValue { field });
        }
        if value > MAX_IMAGE_DIMENSION {
            return Err(CalibrationError::BoundExceeded {
                field,
                actual: u128::from(value),
                maximum: u128::from(MAX_IMAGE_DIMENSION),
            });
        }
    }
    Ok(())
}

fn validate_intrinsics(
    intrinsics: PinholeIntrinsics,
    width: u32,
    height: u32,
) -> Result<(), CalibrationError> {
    for (field, value) in [
        ("fx_nano_pixels", intrinsics.fx_nano_pixels),
        ("fy_nano_pixels", intrinsics.fy_nano_pixels),
    ] {
        if value <= 0 {
            return Err(CalibrationError::ZeroValue { field });
        }
        if value > MAX_INTRINSIC_NANO_PIXELS {
            return Err(CalibrationError::BoundExceeded {
                field,
                actual: value.unsigned_abs().into(),
                maximum: MAX_INTRINSIC_NANO_PIXELS.unsigned_abs().into(),
            });
        }
    }
    let width_nano = pixel_edge_nano(width)?;
    let height_nano = pixel_edge_nano(height)?;
    let minimum_x = width_nano.checked_neg().ok_or(CalibrationError::ArithmeticOverflow { field: "principal_x_min" })?;
    let maximum_x = width_nano.checked_mul(2).ok_or(CalibrationError::ArithmeticOverflow { field: "principal_x_max" })?;
    let minimum_y = height_nano.checked_neg().ok_or(CalibrationError::ArithmeticOverflow { field: "principal_y_min" })?;
    let maximum_y = height_nano.checked_mul(2).ok_or(CalibrationError::ArithmeticOverflow { field: "principal_y_max" })?;
    if intrinsics.cx_nano_pixels < minimum_x || intrinsics.cx_nano_pixels > maximum_x {
        return Err(CalibrationError::InvalidInterval {
            field: "principal_point_x",
        });
    }
    if intrinsics.cy_nano_pixels < minimum_y || intrinsics.cy_nano_pixels > maximum_y {
        return Err(CalibrationError::InvalidInterval {
            field: "principal_point_y",
        });
    }
    Ok(())
}

fn validate_distortion(distortion: DistortionModel) -> Result<(), CalibrationError> {
    for coefficient in distortion_coefficients(distortion) {
        if coefficient.unsigned_abs() > MAX_DISTORTION_NANO.unsigned_abs() {
            return Err(CalibrationError::BoundExceeded {
                field: "distortion_coefficient_nano",
                actual: coefficient.unsigned_abs().into(),
                maximum: MAX_DISTORTION_NANO.unsigned_abs().into(),
            });
        }
    }
    Ok(())
}

fn validate_shutter(shutter: ShutterModel) -> Result<(), CalibrationError> {
    if let ShutterModel::Rolling {
        readout_time_ns,
        reference_phase_nano,
        ..
    } = shutter
    {
        if readout_time_ns == 0 {
            return Err(CalibrationError::ZeroValue {
                field: "readout_time_ns",
            });
        }
        if readout_time_ns > MAX_READOUT_TIME_NS {
            return Err(CalibrationError::BoundExceeded {
                field: "readout_time_ns",
                actual: u128::from(readout_time_ns),
                maximum: u128::from(MAX_READOUT_TIME_NS),
            });
        }
        let maximum_phase = u32::try_from(NANO_SCALE).map_err(|_| CalibrationError::ArithmeticOverflow { field: "nano_scale" })?;
        if reference_phase_nano > maximum_phase {
            return Err(CalibrationError::BoundExceeded {
                field: "reference_phase_nano",
                actual: u128::from(reference_phase_nano),
                maximum: u128::from(maximum_phase),
            });
        }
    }
    Ok(())
}

fn validate_rigid_transform(transform: RigidTransform) -> Result<(), CalibrationError> {
    for value in transform.translation_micrometers {
        if value.unsigned_abs() > MAX_TRANSLATION_MICROMETERS.unsigned_abs() {
            return Err(CalibrationError::BoundExceeded {
                field: "translation_micrometers",
                actual: value.unsigned_abs().into(),
                maximum: MAX_TRANSLATION_MICROMETERS.unsigned_abs().into(),
            });
        }
    }
    let scale = i128::from(NANO_SCALE);
    let target_dot = scale.checked_mul(scale).ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_scale_squared" })?;
    for row in 0..3 {
        let row_dot = dot_row(&transform.rotation_nano, row, row)?;
        if absolute_i128(row_dot.checked_sub(target_dot).ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_row_norm" })?) > ROTATION_DOT_TOLERANCE {
            return Err(CalibrationError::RotationNotOrthonormal);
        }
        for other in row.saturating_add(1)..3 {
            if absolute_i128(dot_row(&transform.rotation_nano, row, other)?) > ROTATION_DOT_TOLERANCE {
                return Err(CalibrationError::RotationNotOrthonormal);
            }
        }
    }
    let determinant = determinant(&transform.rotation_nano)?;
    let target_determinant = target_dot.checked_mul(scale).ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_scale_cubed" })?;
    let determinant_error = determinant.checked_sub(target_determinant).ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_determinant_error" })?;
    if absolute_i128(determinant_error) > ROTATION_DETERMINANT_TOLERANCE {
        return Err(CalibrationError::RotationDeterminantInvalid);
    }
    Ok(())
}

fn validate_scope(scope: &CalibrationScope) -> Result<(), CalibrationError> {
    if scope.minimum_temperature_millicelsius > scope.maximum_temperature_millicelsius {
        return Err(CalibrationError::InvalidInterval {
            field: "temperature",
        });
    }
    if scope.minimum_temperature_millicelsius < -100_000
        || scope.maximum_temperature_millicelsius > 200_000
    {
        return Err(CalibrationError::BoundExceeded {
            field: "temperature_millicelsius",
            actual: u128::from(
                scope
                    .minimum_temperature_millicelsius
                    .unsigned_abs()
                    .max(scope.maximum_temperature_millicelsius.unsigned_abs()),
            ),
            maximum: 200_000,
        });
    }
    Ok(())
}

fn validate_residuals(residuals: CalibrationResiduals) -> Result<(), CalibrationError> {
    if residuals.observation_count == 0 {
        return Err(CalibrationError::ZeroValue {
            field: "observation_count",
        });
    }
    if residuals.median_reprojection_nano_pixels > residuals.maximum_reprojection_nano_pixels
        || residuals.declared_uncertainty_nano_pixels
            < residuals.maximum_reprojection_nano_pixels
    {
        return Err(CalibrationError::InvalidResidualSummary);
    }
    Ok(())
}

fn validate_derivation(
    source: &CalibrationModel,
    derivation: ImageDerivation,
) -> Result<(), CalibrationError> {
    if derivation.source_width != source.source_width
        || derivation.source_height != source.source_height
    {
        return Err(CalibrationError::SourceDimensionMismatch);
    }
    validate_dimensions(derivation.output_width, derivation.output_height)?;
    validate_crop(derivation)
}

fn validate_crop(derivation: ImageDerivation) -> Result<(), CalibrationError> {
    if derivation.crop_width == 0 || derivation.crop_height == 0 {
        return Err(CalibrationError::ZeroValue {
            field: "crop_dimension",
        });
    }
    let end_x = derivation.crop_x.checked_add(derivation.crop_width).ok_or(CalibrationError::ArithmeticOverflow { field: "crop_end_x" })?;
    let end_y = derivation.crop_y.checked_add(derivation.crop_height).ok_or(CalibrationError::ArithmeticOverflow { field: "crop_end_y" })?;
    if end_x > derivation.source_width || end_y > derivation.source_height {
        return Err(CalibrationError::CropOutsideSource);
    }
    Ok(())
}

fn derive_readout(
    shutter: ShutterModel,
    derivation: ImageDerivation,
) -> Result<DerivedReadout, CalibrationError> {
    match shutter {
        ShutterModel::Global => Ok(DerivedReadout {
            rolling: false,
            direction: None,
            first_observed_line_offset_ns: 0,
            observed_readout_time_ns: 0,
            reference_phase_nano: 0,
        }),
        ShutterModel::Rolling {
            direction,
            readout_time_ns,
            reference_phase_nano,
        } => {
            let (leading, extent, total) = match direction {
                ReadoutDirection::TopToBottom => (
                    derivation.crop_y,
                    derivation.crop_height,
                    derivation.source_height,
                ),
                ReadoutDirection::BottomToTop => (
                    derivation
                        .source_height
                        .checked_sub(
                            derivation
                                .crop_y
                                .checked_add(derivation.crop_height)
                                .ok_or(CalibrationError::ArithmeticOverflow { field: "bottom_crop_end" })?,
                        )
                        .ok_or(CalibrationError::CropOutsideSource)?,
                    derivation.crop_height,
                    derivation.source_height,
                ),
                ReadoutDirection::LeftToRight => (
                    derivation.crop_x,
                    derivation.crop_width,
                    derivation.source_width,
                ),
                ReadoutDirection::RightToLeft => (
                    derivation
                        .source_width
                        .checked_sub(
                            derivation
                                .crop_x
                                .checked_add(derivation.crop_width)
                                .ok_or(CalibrationError::ArithmeticOverflow { field: "right_crop_end" })?,
                        )
                        .ok_or(CalibrationError::CropOutsideSource)?,
                    derivation.crop_width,
                    derivation.source_width,
                ),
            };
            Ok(DerivedReadout {
                rolling: true,
                direction: Some(direction),
                first_observed_line_offset_ns: scale_u64(
                    readout_time_ns,
                    leading,
                    total,
                    "readout_leading_offset",
                )?,
                observed_readout_time_ns: scale_u64(
                    readout_time_ns,
                    extent,
                    total,
                    "observed_readout_time",
                )?,
                reference_phase_nano,
            })
        }
    }
}

fn scale_fixed(
    value: i64,
    output_extent: u32,
    source_extent: u32,
    field: &'static str,
) -> Result<i64, CalibrationError> {
    let numerator = i128::from(value)
        .checked_mul(i128::from(output_extent))
        .ok_or(CalibrationError::ArithmeticOverflow { field })?;
    let scaled = divide_round_nearest(numerator, i128::from(source_extent), field)?;
    i64::try_from(scaled).map_err(|_| CalibrationError::ArithmeticOverflow { field })
}

fn scale_u64(
    value: u64,
    numerator_scale: u32,
    denominator_scale: u32,
    field: &'static str,
) -> Result<u64, CalibrationError> {
    let numerator = u128::from(value)
        .checked_mul(u128::from(numerator_scale))
        .ok_or(CalibrationError::ArithmeticOverflow { field })?;
    let rounded = numerator
        .checked_add(u128::from(denominator_scale) / 2)
        .ok_or(CalibrationError::ArithmeticOverflow { field })?
        / u128::from(denominator_scale);
    u64::try_from(rounded).map_err(|_| CalibrationError::ArithmeticOverflow { field })
}

fn divide_round_nearest(
    numerator: i128,
    denominator: i128,
    field: &'static str,
) -> Result<i128, CalibrationError> {
    if denominator <= 0 {
        return Err(CalibrationError::ZeroValue { field });
    }
    let quotient = numerator / denominator;
    let remainder = numerator % denominator;
    let doubled = remainder
        .unsigned_abs()
        .checked_mul(2)
        .ok_or(CalibrationError::ArithmeticOverflow { field })?;
    if doubled < denominator.unsigned_abs() {
        return Ok(quotient);
    }
    if numerator >= 0 {
        quotient.checked_add(1).ok_or(CalibrationError::ArithmeticOverflow { field })
    } else {
        quotient.checked_sub(1).ok_or(CalibrationError::ArithmeticOverflow { field })
    }
}

fn pixel_edge_nano(value: u32) -> Result<i64, CalibrationError> {
    i64::from(value)
        .checked_mul(NANO_SCALE)
        .ok_or(CalibrationError::ArithmeticOverflow {
            field: "pixel_edge_nano",
        })
}

fn dot_row(matrix: &[i64; 9], left: usize, right: usize) -> Result<i128, CalibrationError> {
    let mut sum = 0_i128;
    for column in 0..3 {
        let left_index = left
            .checked_mul(3)
            .and_then(|value| value.checked_add(column))
            .ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_index" })?;
        let right_index = right
            .checked_mul(3)
            .and_then(|value| value.checked_add(column))
            .ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_index" })?;
        let left_value = *matrix.get(left_index).ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_index" })?;
        let right_value = *matrix.get(right_index).ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_index" })?;
        sum = sum
            .checked_add(i128::from(left_value).checked_mul(i128::from(right_value)).ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_dot" })?)
            .ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_dot" })?;
    }
    Ok(sum)
}

fn determinant(matrix: &[i64; 9]) -> Result<i128, CalibrationError> {
    let value = |index: usize| -> Result<i128, CalibrationError> {
        matrix
            .get(index)
            .copied()
            .map(i128::from)
            .ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_index" })
    };
    let a = value(0)?;
    let b = value(1)?;
    let c = value(2)?;
    let d = value(3)?;
    let e = value(4)?;
    let f = value(5)?;
    let g = value(6)?;
    let h = value(7)?;
    let i = value(8)?;
    let positive = a
        .checked_mul(e)
        .and_then(|x| x.checked_mul(i))
        .and_then(|x| b.checked_mul(f).and_then(|y| y.checked_mul(g)).and_then(|y| x.checked_add(y)))
        .and_then(|x| c.checked_mul(d).and_then(|y| y.checked_mul(h)).and_then(|y| x.checked_add(y)))
        .ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_determinant" })?;
    let negative = c
        .checked_mul(e)
        .and_then(|x| x.checked_mul(g))
        .and_then(|x| b.checked_mul(d).and_then(|y| y.checked_mul(i)).and_then(|y| x.checked_add(y)))
        .and_then(|x| a.checked_mul(f).and_then(|y| y.checked_mul(h)).and_then(|y| x.checked_add(y)))
        .ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_determinant" })?;
    positive
        .checked_sub(negative)
        .ok_or(CalibrationError::ArithmeticOverflow { field: "rotation_determinant" })
}

const fn absolute_i128(value: i128) -> i128 {
    if value < 0 { -value } else { value }
}

fn reject_zero_digest(field: &'static str, digest: &EvidenceDigest) -> Result<(), CalibrationError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(CalibrationError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn validate_nonzero(field: &'static str, value: u64) -> Result<(), CalibrationError> {
    if value == 0 {
        Err(CalibrationError::ZeroValue { field })
    } else {
        Ok(())
    }
}

fn distortion_coefficients(distortion: DistortionModel) -> [i64; 5] {
    match distortion {
        DistortionModel::None => [0; 5],
        DistortionModel::BrownConrady {
            k1_nano,
            k2_nano,
            p1_nano,
            p2_nano,
            k3_nano,
        } => [k1_nano, k2_nano, p1_nano, p2_nano, k3_nano],
    }
}

fn shutter_values(shutter: ShutterModel) -> (u64, u32) {
    match shutter {
        ShutterModel::Global => (0, 0),
        ShutterModel::Rolling {
            readout_time_ns,
            reference_phase_nano,
            ..
        } => (readout_time_ns, reference_phase_nano),
    }
}

fn encode_intrinsics(
    encoder: &mut Encoder,
    intrinsics: PinholeIntrinsics,
) -> Result<(), CalibrationError> {
    encoder.put_i64(intrinsics.fx_nano_pixels);
    encoder.put_i64(intrinsics.fy_nano_pixels);
    encoder.put_i64(intrinsics.cx_nano_pixels);
    encoder.put_i64(intrinsics.cy_nano_pixels);
    encoder.put_u8(intrinsics.pixel_convention.code());
    Ok(())
}

fn encode_distortion(
    encoder: &mut Encoder,
    distortion: DistortionModel,
) -> Result<(), CalibrationError> {
    encoder.put_u8(distortion.code());
    for coefficient in distortion_coefficients(distortion) {
        encoder.put_i64(coefficient);
    }
    Ok(())
}

fn encode_shutter(encoder: &mut Encoder, shutter: ShutterModel) -> Result<(), CalibrationError> {
    encoder.put_u8(shutter.code());
    match shutter {
        ShutterModel::Global => {
            encoder.put_u8(0);
            encoder.put_u64(0);
            encoder.put_u32(0);
        }
        ShutterModel::Rolling {
            direction,
            readout_time_ns,
            reference_phase_nano,
        } => {
            encoder.put_u8(direction.code());
            encoder.put_u64(readout_time_ns);
            encoder.put_u32(reference_phase_nano);
        }
    }
    Ok(())
}

fn encode_rigid_transform(
    encoder: &mut Encoder,
    transform: RigidTransform,
) -> Result<(), CalibrationError> {
    for value in transform.rotation_nano {
        encoder.put_i64(value);
    }
    for value in transform.translation_micrometers {
        encoder.put_i64(value);
    }
    Ok(())
}

fn encode_derivation(encoder: &mut Encoder, derivation: ImageDerivation) {
    encoder.put_u32(derivation.source_width);
    encoder.put_u32(derivation.source_height);
    encoder.put_u32(derivation.crop_x);
    encoder.put_u32(derivation.crop_y);
    encoder.put_u32(derivation.crop_width);
    encoder.put_u32(derivation.crop_height);
    encoder.put_u32(derivation.output_width);
    encoder.put_u32(derivation.output_height);
}

fn push_i64_array<const N: usize>(
    output: &mut String,
    values: &[i64; N],
) -> Result<(), CalibrationError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}

fn json_rendering(error: std::fmt::Error) -> CalibrationError {
    CalibrationError::JsonRendering(error.to_string())
}

#[cfg(test)]
mod tests {
    use super::{
        CalibrationError, CalibrationModel, CalibrationResiduals, CalibrationScope,
        CaptureCalibrationState, DistortionModel, ImageDerivation, NANO_SCALE, PinholeIntrinsics,
        PixelConvention, ReadoutDirection, RigidTransform, ShutterModel, derive_calibration,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn model() -> CalibrationModel {
        CalibrationModel {
            evidence_digest: digest(1),
            calibration_epoch: 1,
            model_generation: 1,
            source_width: 4_000,
            source_height: 3_000,
            intrinsics: PinholeIntrinsics {
                fx_nano_pixels: 2_000 * NANO_SCALE,
                fy_nano_pixels: 2_100 * NANO_SCALE,
                cx_nano_pixels: 2_000 * NANO_SCALE,
                cy_nano_pixels: 1_500 * NANO_SCALE,
                pixel_convention: PixelConvention::TopLeftHalfPixel,
            },
            distortion: DistortionModel::BrownConrady {
                k1_nano: -10_000_000,
                k2_nano: 2_000_000,
                p1_nano: 100_000,
                p2_nano: -100_000,
                k3_nano: 0,
            },
            shutter: ShutterModel::Rolling {
                direction: ReadoutDirection::TopToBottom,
                readout_time_ns: 30_000_000,
                reference_phase_nano: 500_000_000,
            },
            camera_from_body: RigidTransform::IDENTITY,
            scope: CalibrationScope {
                camera_profile_digest: digest(2),
                lens_state_digest: digest(3),
                minimum_temperature_millicelsius: -20_000,
                maximum_temperature_millicelsius: 70_000,
            },
            residuals: CalibrationResiduals {
                observation_count: 200,
                median_reprojection_nano_pixels: 100_000_000,
                maximum_reprojection_nano_pixels: 300_000_000,
                declared_uncertainty_nano_pixels: 500_000_000,
            },
        }
    }

    #[test]
    fn crop_resize_transforms_intrinsics_and_readout() {
        let source = model();
        let derived = derive_calibration(
            &source,
            ImageDerivation {
                source_width: 4_000,
                source_height: 3_000,
                crop_x: 1_000,
                crop_y: 500,
                crop_width: 2_000,
                crop_height: 1_500,
                output_width: 1_000,
                output_height: 750,
            },
        );
        assert!(matches!(derived, Ok(ref value) if value.intrinsics.fx_nano_pixels == 1_000 * NANO_SCALE && value.intrinsics.fy_nano_pixels == 1_050 * NANO_SCALE && value.intrinsics.cx_nano_pixels == 500 * NANO_SCALE && value.intrinsics.cy_nano_pixels == 500 * NANO_SCALE && value.readout.first_observed_line_offset_ns == 5_000_000 && value.readout.observed_readout_time_ns == 15_000_000 && value.validate().is_ok() && value.digest().is_ok()));
    }

    #[test]
    fn applicability_is_exact_and_temperature_bounded() {
        let source = model();
        let admitted = CaptureCalibrationState {
            camera_profile_digest: digest(2),
            lens_state_digest: digest(3),
            source_width: 4_000,
            source_height: 3_000,
            temperature_millicelsius: 20_000,
        };
        assert!(source.admit_capture(&admitted).is_ok());
        let mut wrong = admitted;
        wrong.lens_state_digest = digest(4);
        assert!(matches!(source.admit_capture(&wrong), Err(CalibrationError::CaptureOutsideScope { reason: "lens_state_digest" })));
    }

    #[test]
    fn invalid_rotation_and_crop_fail_closed() {
        let mut source = model();
        source.camera_from_body.rotation_nano[0] = 0;
        assert!(matches!(source.validate(), Err(CalibrationError::RotationNotOrthonormal)));
        let source = model();
        assert!(matches!(derive_calibration(&source, ImageDerivation { source_width: 4_000, source_height: 3_000, crop_x: 3_500, crop_y: 0, crop_width: 1_000, crop_height: 1_000, output_width: 500, output_height: 500 }), Err(CalibrationError::CropOutsideSource)));
    }

    #[test]
    fn identities_are_deterministic_and_derivation_sensitive() {
        let source = model();
        assert!(matches!((source.digest(), source.digest()), (Ok(left), Ok(right)) if left == right));
        let first = derive_calibration(&source, ImageDerivation { source_width: 4_000, source_height: 3_000, crop_x: 0, crop_y: 0, crop_width: 4_000, crop_height: 3_000, output_width: 2_000, output_height: 1_500 });
        let second = derive_calibration(&source, ImageDerivation { source_width: 4_000, source_height: 3_000, crop_x: 1, crop_y: 0, crop_width: 3_999, crop_height: 3_000, output_width: 2_000, output_height: 1_500 });
        assert!(matches!((first, second), (Ok(first), Ok(second)) if first.digest().is_ok() && second.digest().is_ok() && first.digest() != second.digest()));
        assert!(matches!(source.to_json(), Ok(ref json) if json.contains("fdgr.calibration_model/1")));
    }
}
