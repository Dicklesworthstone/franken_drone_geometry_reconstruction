/// Projects one exact camera-frame point through a rectified derived calibration.
///
/// The input point uses the same arbitrary component-relative nano unit for all axes. Perspective
/// division cancels that arbitrary scale, so this operation does not grant metric authority.
///
/// # Errors
///
/// Returns a stable calibration, unsupported-model, depth, bound, or arithmetic error.
pub fn project_rectified_camera_point(
    calibration: &DerivedCalibration,
    point_camera_nano: [i64; 3],
    policy: RectifiedProjectionPolicy,
) -> Result<ProjectedPixel, ProjectionError> {
    calibration.validate()?;
    validate_policy(policy)?;
    match calibration.distortion {
        DistortionModel::None => {}
        model => {
            return Err(ProjectionError::UnsupportedDistortion {
                model: model.as_str(),
            });
        }
    }
    if calibration.readout.rolling {
        return Err(ProjectionError::RollingShutterRequiresMotionModel);
    }
    for (field, value) in [
        ("point_camera_x_nano", point_camera_nano[0]),
        ("point_camera_y_nano", point_camera_nano[1]),
        ("point_camera_z_nano", point_camera_nano[2]),
    ] {
        let magnitude = value.unsigned_abs();
        if magnitude > MAX_CAMERA_POINT_ABS_NANO {
            return Err(ProjectionError::BoundExceeded {
                field,
                actual: u128::from(magnitude),
                maximum: u128::from(MAX_CAMERA_POINT_ABS_NANO),
            });
        }
    }
    let depth_nano = point_camera_nano[2];
    if depth_nano <= 0 || depth_nano.unsigned_abs() < policy.minimum_positive_depth_nano {
        return Err(ProjectionError::InsufficientPositiveDepth {
            depth_nano,
            minimum_positive_depth_nano: policy.minimum_positive_depth_nano,
        });
    }
    let x_nano_pixels = project_axis(
        calibration.intrinsics.fx_nano_pixels,
        point_camera_nano[0],
        depth_nano,
        calibration.intrinsics.cx_nano_pixels,
        "project_x",
    )?;
    let y_nano_pixels = project_axis(
        calibration.intrinsics.fy_nano_pixels,
        point_camera_nano[1],
        depth_nano,
        calibration.intrinsics.cy_nano_pixels,
        "project_y",
    )?;
    for (field, value) in [
        ("x_nano_pixels", x_nano_pixels),
        ("y_nano_pixels", y_nano_pixels),
    ] {
        let magnitude = value.unsigned_abs();
        if magnitude > policy.max_projected_coordinate_abs_nano_pixels {
            return Err(ProjectionError::BoundExceeded {
                field,
                actual: u128::from(magnitude),
                maximum: u128::from(policy.max_projected_coordinate_abs_nano_pixels),
            });
        }
    }
    let width_nano = image_edge_nano(calibration.derivation.output_width, "image_width_nano")?;
    let height_nano = image_edge_nano(calibration.derivation.output_height, "image_height_nano")?;
    Ok(ProjectedPixel {
        x_nano_pixels,
        y_nano_pixels,
        depth_nano,
        within_image_domain: x_nano_pixels >= 0
            && x_nano_pixels < width_nano
            && y_nano_pixels >= 0
            && y_nano_pixels < height_nano,
    })
}

fn validate_policy(policy: RectifiedProjectionPolicy) -> Result<(), ProjectionError> {
    if policy.minimum_positive_depth_nano == 0 {
        return Err(ProjectionError::ZeroValue {
            field: "minimum_positive_depth_nano",
        });
    }
    if policy.minimum_positive_depth_nano > MAX_CAMERA_POINT_ABS_NANO {
        return Err(ProjectionError::BoundExceeded {
            field: "minimum_positive_depth_nano",
            actual: u128::from(policy.minimum_positive_depth_nano),
            maximum: u128::from(MAX_CAMERA_POINT_ABS_NANO),
        });
    }
    if policy.max_projected_coordinate_abs_nano_pixels == 0 {
        return Err(ProjectionError::ZeroValue {
            field: "max_projected_coordinate_abs_nano_pixels",
        });
    }
    if policy.max_projected_coordinate_abs_nano_pixels
        > MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS
    {
        return Err(ProjectionError::BoundExceeded {
            field: "max_projected_coordinate_abs_nano_pixels",
            actual: u128::from(policy.max_projected_coordinate_abs_nano_pixels),
            maximum: u128::from(MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS),
        });
    }
    Ok(())
}

fn project_axis(
    focal_nano_pixels: i64,
    camera_coordinate_nano: i64,
    depth_nano: i64,
    principal_nano_pixels: i64,
    field: &'static str,
) -> Result<i64, ProjectionError> {
    let numerator = i128::from(focal_nano_pixels)
        .checked_mul(i128::from(camera_coordinate_nano))
        .ok_or(ProjectionError::ArithmeticOverflow { field })?;
    let offset = divide_round_nearest(numerator, i128::from(depth_nano), field)?;
    let projected = i128::from(principal_nano_pixels)
        .checked_add(offset)
        .ok_or(ProjectionError::ArithmeticOverflow { field })?;
    i64::try_from(projected).map_err(|_| ProjectionError::ArithmeticOverflow { field })
}

fn image_edge_nano(value: u32, field: &'static str) -> Result<i64, ProjectionError> {
    i64::from(value)
        .checked_mul(NANO_SCALE)
        .ok_or(ProjectionError::ArithmeticOverflow { field })
}

fn divide_round_nearest(
    numerator: i128,
    denominator: i128,
    field: &'static str,
) -> Result<i128, ProjectionError> {
    if denominator <= 0 {
        return Err(ProjectionError::ZeroValue { field });
    }
    let quotient = numerator / denominator;
    let remainder = numerator % denominator;
    let doubled = remainder
        .unsigned_abs()
        .checked_mul(2)
        .ok_or(ProjectionError::ArithmeticOverflow { field })?;
    if doubled < denominator.unsigned_abs() {
        return Ok(quotient);
    }
    if numerator >= 0 {
        quotient
            .checked_add(1)
            .ok_or(ProjectionError::ArithmeticOverflow { field })
    } else {
        quotient
            .checked_sub(1)
            .ok_or(ProjectionError::ArithmeticOverflow { field })
    }
}
