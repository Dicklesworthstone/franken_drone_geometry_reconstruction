#[cfg(test)]
mod tests {
    use super::{
        CalibratedProjectionPolicy, ProjectionError, RectifiedProjectionPolicy,
        project_calibrated_camera_point, project_rectified_camera_point,
    };
    use fdgr_calibration::{
        DerivedCalibration, DerivedReadout, DistortionModel, ImageDerivation, NANO_SCALE,
        PinholeIntrinsics, PixelConvention, ReadoutDirection, RigidTransform,
    };
    use fdgr_types::EvidenceDigest;

    fn calibration() -> DerivedCalibration {
        DerivedCalibration {
            source_calibration_digest: EvidenceDigest::from_bytes([1; 32]),
            derivation: ImageDerivation {
                source_width: 1_000,
                source_height: 800,
                crop_x: 0,
                crop_y: 0,
                crop_width: 1_000,
                crop_height: 800,
                output_width: 1_000,
                output_height: 800,
            },
            intrinsics: PinholeIntrinsics {
                fx_nano_pixels: 500 * NANO_SCALE,
                fy_nano_pixels: 400 * NANO_SCALE,
                cx_nano_pixels: 500 * NANO_SCALE,
                cy_nano_pixels: 400 * NANO_SCALE,
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
            declared_uncertainty_nano_pixels: 100_000_000,
        }
    }

    #[test]
    fn principal_and_off_axis_rectified_projection_are_exact() {
        let model = calibration();
        let center = project_rectified_camera_point(
            &model,
            [0, 0, 2 * NANO_SCALE],
            RectifiedProjectionPolicy::default(),
        );
        assert!(matches!(center, Ok(value) if value.x_nano_pixels == 500 * NANO_SCALE && value.y_nano_pixels == 400 * NANO_SCALE && value.within_image_domain));
        let offset = project_rectified_camera_point(
            &model,
            [NANO_SCALE, -NANO_SCALE, 2 * NANO_SCALE],
            RectifiedProjectionPolicy::default(),
        );
        assert!(matches!(offset, Ok(value) if value.x_nano_pixels == 750 * NANO_SCALE && value.y_nano_pixels == 200 * NANO_SCALE && value.within_image_domain));
    }

    #[test]
    fn calibrated_projection_retains_normalized_coordinates() {
        let value = project_calibrated_camera_point(
            &calibration(),
            [NANO_SCALE, -NANO_SCALE, 2 * NANO_SCALE],
            CalibratedProjectionPolicy::default(),
        );
        assert!(matches!(
            value,
            Ok(projected)
                if projected.x_nano_pixels == 750 * NANO_SCALE
                    && projected.y_nano_pixels == 200 * NANO_SCALE
                    && projected.normalized_x_nano == 500_000_000
                    && projected.normalized_y_nano == -500_000_000
                    && projected.within_image_domain
        ));
    }

    #[test]
    fn outside_image_is_evidence_not_projection_failure() {
        let value = project_calibrated_camera_point(
            &calibration(),
            [3 * NANO_SCALE, 0, NANO_SCALE],
            CalibratedProjectionPolicy::default(),
        );
        assert!(matches!(value, Ok(projected) if !projected.within_image_domain && projected.x_nano_pixels == 2_000 * NANO_SCALE));
    }

    #[test]
    fn depth_and_numeric_bounds_fail_closed() {
        let policy = RectifiedProjectionPolicy::default();
        assert!(matches!(
            project_rectified_camera_point(&calibration(), [0, 0, 0], policy),
            Err(ProjectionError::InsufficientPositiveDepth { .. })
        ));
        let mut narrow = policy;
        narrow.max_projected_coordinate_abs_nano_pixels = 600 * NANO_SCALE.unsigned_abs();
        assert!(matches!(
            project_rectified_camera_point(
                &calibration(),
                [NANO_SCALE, 0, NANO_SCALE],
                narrow
            ),
            Err(ProjectionError::BoundExceeded {
                field: "x_nano_pixels",
                ..
            })
        ));
        let mut normalized = CalibratedProjectionPolicy::default();
        normalized.max_normalized_coordinate_abs_nano = 100_000_000;
        assert!(matches!(
            project_calibrated_camera_point(
                &calibration(),
                [NANO_SCALE, 0, 2 * NANO_SCALE],
                normalized
            ),
            Err(ProjectionError::NormalizedCoordinateExceeded { .. })
        ));
    }

    #[test]
    fn brown_conrady_zero_coefficients_equal_rectified_projection() {
        let plain = calibration();
        let mut brown = plain.clone();
        brown.distortion = DistortionModel::BrownConrady {
            k1_nano: 0,
            k2_nano: 0,
            p1_nano: 0,
            p2_nano: 0,
            k3_nano: 0,
        };
        let left = project_calibrated_camera_point(
            &plain,
            [NANO_SCALE, 2 * NANO_SCALE, 10 * NANO_SCALE],
            CalibratedProjectionPolicy::default(),
        );
        let right = project_calibrated_camera_point(
            &brown,
            [NANO_SCALE, 2 * NANO_SCALE, 10 * NANO_SCALE],
            CalibratedProjectionPolicy::default(),
        );
        assert_eq!(left, right);
    }

    #[test]
    fn brown_conrady_radial_distortion_is_applied_in_normalized_space() {
        let mut model = calibration();
        model.distortion = DistortionModel::BrownConrady {
            k1_nano: 100_000_000,
            k2_nano: 0,
            p1_nano: 0,
            p2_nano: 0,
            k3_nano: 0,
        };
        let value = project_calibrated_camera_point(
            &model,
            [NANO_SCALE, 0, 2 * NANO_SCALE],
            CalibratedProjectionPolicy::default(),
        );
        assert!(matches!(
            value,
            Ok(projected)
                if projected.normalized_x_nano == 500_000_000
                    && projected.x_nano_pixels == 756_250_000_000
                    && projected.y_nano_pixels == 400_000_000_000
        ));
    }

    #[test]
    fn rectified_compatibility_surface_still_rejects_distortion() {
        let mut distorted = calibration();
        distorted.distortion = DistortionModel::BrownConrady {
            k1_nano: 1,
            k2_nano: 0,
            p1_nano: 0,
            p2_nano: 0,
            k3_nano: 0,
        };
        assert!(matches!(
            project_rectified_camera_point(
                &distorted,
                [0, 0, NANO_SCALE],
                RectifiedProjectionPolicy::default()
            ),
            Err(ProjectionError::UnsupportedDistortion {
                model: "brown_conrady"
            })
        ));
    }

    #[test]
    fn rolling_shutter_is_never_silently_approximated() {
        let mut rolling = calibration();
        rolling.readout = DerivedReadout {
            rolling: true,
            direction: Some(ReadoutDirection::TopToBottom),
            first_observed_line_offset_ns: 0,
            observed_readout_time_ns: 10_000_000,
            reference_phase_nano: 500_000_000,
        };
        assert!(matches!(
            project_calibrated_camera_point(
                &rolling,
                [0, 0, NANO_SCALE],
                CalibratedProjectionPolicy::default()
            ),
            Err(ProjectionError::RollingShutterRequiresMotionModel)
        ));
    }
}
