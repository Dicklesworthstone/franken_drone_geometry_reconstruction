#[cfg(test)]
mod bounds_tests {
    use super::{
        MAX_REPROJECTION_RESIDUAL_NANO_PIXELS, MAX_SAFE_JSON_INTEGER, ReprojectionCalibration,
        ReprojectionError, normalized_residual_ppm, reprojection_calibration_table_digest,
        residual_magnitude, validate_signed_safe_number,
    };
    use fdgr_calibration::{
        DerivedCalibration, DerivedReadout, DistortionModel, ImageDerivation, PinholeIntrinsics,
        PixelConvention, RigidTransform,
    };
    use fdgr_types::EvidenceDigest;
    use std::error::Error as StdError;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn calibration(uncertainty: u64) -> DerivedCalibration {
        DerivedCalibration {
            source_calibration_digest: digest(1),
            derivation: ImageDerivation {
                source_width: 640,
                source_height: 480,
                crop_x: 0,
                crop_y: 0,
                crop_width: 640,
                crop_height: 480,
                output_width: 640,
                output_height: 480,
            },
            intrinsics: PinholeIntrinsics {
                fx_nano_pixels: 100_000_000_000,
                fy_nano_pixels: 100_000_000_000,
                cx_nano_pixels: 320_000_000_000,
                cy_nano_pixels: 240_000_000_000,
                pixel_convention: PixelConvention::TopLeftHalfPixel,
            },
            distortion: DistortionModel::None,
            readout: DerivedReadout {
                rolling: false,
                direction: None,
                first_observed_line_offset_ns: 0,
                observed_readout_time_ns: 0,
                reference_phase_nano: 0,
            },
            camera_from_body: RigidTransform::IDENTITY,
            declared_uncertainty_nano_pixels: uncertainty,
        }
    }

    #[test]
    fn signed_residual_components_must_round_trip_through_json() {
        let oversized = i64::try_from(MAX_SAFE_JSON_INTEGER.saturating_add(1));
        assert!(matches!(
            oversized,
            Ok(value)
                if matches!(
                    validate_signed_safe_number("residual", value),
                    Err(ReprojectionError::BoundExceeded { field: "residual", .. })
                )
        ));
    }

    #[test]
    fn residual_magnitude_cannot_exceed_public_contract() {
        let oversized = i64::try_from(
            MAX_REPROJECTION_RESIDUAL_NANO_PIXELS.saturating_add(1),
        );
        assert!(matches!(
            oversized,
            Ok(value)
                if matches!(
                    residual_magnitude(value, 0),
                    Err(ReprojectionError::BoundExceeded {
                        field: "residual_nano_pixels",
                        ..
                    })
                )
        ));
    }

    #[test]
    fn normalized_residual_cannot_escape_safe_integer_domain() {
        assert!(matches!(
            normalized_residual_ppm(10_000_000_000_000, 0, 0),
            Err(ReprojectionError::BoundExceeded {
                field: "normalized_residual_ppm",
                ..
            })
        ));
    }

    #[test]
    fn materialized_calibration_uncertainty_is_schema_bounded(
    ) -> Result<(), Box<dyn StdError>> {
        let calibration = calibration(MAX_SAFE_JSON_INTEGER.saturating_add(1));
        let effective_calibration_digest = calibration.digest()?;
        let row = ReprojectionCalibration {
            camera_node_id: 1,
            frame_digest: digest(2),
            effective_calibration_digest,
            calibration,
        };
        assert!(matches!(
            reprojection_calibration_table_digest(&[row]),
            Err(ReprojectionError::BoundExceeded {
                field: "declared_uncertainty_nano_pixels",
                ..
            })
        ));
        Ok(())
    }
}
