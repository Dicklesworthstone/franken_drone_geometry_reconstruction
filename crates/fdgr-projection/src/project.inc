/// Projects one exact camera-frame point through a distortion-free global-shutter calibration.
///
/// This compatibility operation preserves the original rectified-only contract. Use
/// [`project_calibrated_camera_point`] when an admitted Brown-Conrady model must be applied.
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
    validate_rectified_policy(policy)?;
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
    validate_camera_point(point_camera_nano)?;
    let depth_nano = validate_depth(point_camera_nano[2], policy.minimum_positive_depth_nano)?;
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
    validate_projected_coordinate(
        "x_nano_pixels",
        x_nano_pixels,
        policy.max_projected_coordinate_abs_nano_pixels,
    )?;
    validate_projected_coordinate(
        "y_nano_pixels",
        y_nano_pixels,
        policy.max_projected_coordinate_abs_nano_pixels,
    )?;
    Ok(ProjectedPixel {
        x_nano_pixels,
        y_nano_pixels,
        depth_nano,
        within_image_domain: within_image_domain(calibration, x_nano_pixels, y_nano_pixels)?,
    })
}

/// Projects one exact camera-frame point through a global-shutter pinhole or Brown-Conrady model.
///
/// Perspective division cancels the arbitrary component scale. Distortion is applied in the
/// normalized camera domain using staged fixed-point arithmetic. The operation still grants no
/// pose, landmark, metric, accuracy, optimization, or publication authority.
///
/// # Errors
///
/// Returns a stable calibration, rolling-shutter, depth, normalized-domain, bound, or arithmetic
/// error.
pub fn project_calibrated_camera_point(
    calibration: &DerivedCalibration,
    point_camera_nano: [i64; 3],
    policy: CalibratedProjectionPolicy,
) -> Result<CalibratedProjectedPixel, ProjectionError> {
    calibration.validate()?;
    validate_calibrated_policy(policy)?;
    if calibration.readout.rolling {
        return Err(ProjectionError::RollingShutterRequiresMotionModel);
    }
    validate_camera_point(point_camera_nano)?;
    let depth_nano = validate_depth(point_camera_nano[2], policy.minimum_positive_depth_nano)?;
    let normalized_x = divide_round_nearest(
        i128::from(point_camera_nano[0])
            .checked_mul(i128::from(NANO_SCALE))
            .ok_or(ProjectionError::ArithmeticOverflow {
                field: "normalized_x",
            })?,
        i128::from(depth_nano),
        "normalized_x",
    )?;
    let normalized_y = divide_round_nearest(
        i128::from(point_camera_nano[1])
            .checked_mul(i128::from(NANO_SCALE))
            .ok_or(ProjectionError::ArithmeticOverflow {
                field: "normalized_y",
            })?,
        i128::from(depth_nano),
        "normalized_y",
    )?;
    let normalized_x = i64::try_from(normalized_x).map_err(|_| {
        ProjectionError::ArithmeticOverflow {
            field: "normalized_x",
        }
    })?;
    let normalized_y = i64::try_from(normalized_y).map_err(|_| {
        ProjectionError::ArithmeticOverflow {
            field: "normalized_y",
        }
    })?;
    if normalized_x.unsigned_abs() > policy.max_normalized_coordinate_abs_nano
        || normalized_y.unsigned_abs() > policy.max_normalized_coordinate_abs_nano
    {
        return Err(ProjectionError::NormalizedCoordinateExceeded {
            x_nano: normalized_x,
            y_nano: normalized_y,
            maximum_abs_nano: policy.max_normalized_coordinate_abs_nano,
        });
    }
    let distorted = distort_normalized(calibration.distortion, [normalized_x, normalized_y])?;
    let x_nano_pixels = project_normalized_axis(
        calibration.intrinsics.fx_nano_pixels,
        distorted[0],
        calibration.intrinsics.cx_nano_pixels,
        "project_distorted_x",
    )?;
    let y_nano_pixels = project_normalized_axis(
        calibration.intrinsics.fy_nano_pixels,
        distorted[1],
        calibration.intrinsics.cy_nano_pixels,
        "project_distorted_y",
    )?;
    validate_projected_coordinate(
        "x_nano_pixels",
        x_nano_pixels,
        policy.max_projected_coordinate_abs_nano_pixels,
    )?;
    validate_projected_coordinate(
        "y_nano_pixels",
        y_nano_pixels,
        policy.max_projected_coordinate_abs_nano_pixels,
    )?;
    Ok(CalibratedProjectedPixel {
        x_nano_pixels,
        y_nano_pixels,
        depth_nano,
        normalized_x_nano: normalized_x,
        normalized_y_nano: normalized_y,
        within_image_domain: within_image_domain(calibration, x_nano_pixels, y_nano_pixels)?,
    })
}

fn distort_normalized(
    distortion: DistortionModel,
    normalized: [i64; 2],
) -> Result<[i128; 2], ProjectionError> {
    let DistortionModel::BrownConrady {
        k1_nano,
        k2_nano,
        p1_nano,
        p2_nano,
        k3_nano,
    } = distortion
    else {
        return Ok([i128::from(normalized[0]), i128::from(normalized[1])]);
    };
    let scale = i128::from(NANO_SCALE);
    let x = i128::from(normalized[0]);
    let y = i128::from(normalized[1]);
    let x2 = fixed_product(x, x, "distortion_x2")?;
    let y2 = fixed_product(y, y, "distortion_y2")?;
    let xy = fixed_product(x, y, "distortion_xy")?;
    let r2 = x2
        .checked_add(y2)
        .ok_or(ProjectionError::ArithmeticOverflow {
            field: "distortion_r2",
        })?;
    let r4 = fixed_product(r2, r2, "distortion_r4")?;
    let r6 = fixed_product(r4, r2, "distortion_r6")?;
    let radial_k1 = fixed_product(i128::from(k1_nano), r2, "distortion_k1")?;
    let radial_k2 = fixed_product(i128::from(k2_nano), r4, "distortion_k2")?;
    let radial_k3 = fixed_product(i128::from(k3_nano), r6, "distortion_k3")?;
    let radial = scale
        .checked_add(radial_k1)
        .and_then(|value| value.checked_add(radial_k2))
        .and_then(|value| value.checked_add(radial_k3))
        .ok_or(ProjectionError::ArithmeticOverflow {
            field: "distortion_radial",
        })?;
    let radial_x = fixed_product(x, radial, "distortion_radial_x")?;
    let radial_y = fixed_product(y, radial, "distortion_radial_y")?;
    let twice_xy = xy
        .checked_mul(2)
        .ok_or(ProjectionError::ArithmeticOverflow {
            field: "distortion_twice_xy",
        })?;
    let twice_x2 = x2
        .checked_mul(2)
        .ok_or(ProjectionError::ArithmeticOverflow {
            field: "distortion_twice_x2",
        })?;
    let twice_y2 = y2
        .checked_mul(2)
        .ok_or(ProjectionError::ArithmeticOverflow {
            field: "distortion_twice_y2",
        })?;
    let r2_plus_twice_x2 = r2
        .checked_add(twice_x2)
        .ok_or(ProjectionError::ArithmeticOverflow {
            field: "distortion_r2_plus_twice_x2",
        })?;
    let r2_plus_twice_y2 = r2
        .checked_add(twice_y2)
        .ok_or(ProjectionError::ArithmeticOverflow {
            field: "distortion_r2_plus_twice_y2",
        })?;
    let tangential_x = fixed_product(
        i128::from(p1_nano),
        twice_xy,
        "distortion_tangential_x_p1",
    )?
    .checked_add(fixed_product(
        i128::from(p2_nano),
        r2_plus_twice_x2,
        "distortion_tangential_x_p2",
    )?)
    .ok_or(ProjectionError::ArithmeticOverflow {
        field: "distortion_tangential_x",
    })?;
    let tangential_y = fixed_product(
        i128::from(p1_nano),
        r2_plus_twice_y2,
        "distortion_tangential_y_p1",
    )?
    .checked_add(fixed_product(
        i128::from(p2_nano),
        twice_xy,
        "distortion_tangential_y_p2",
    )?)
    .ok_or(ProjectionError::ArithmeticOverflow {
        field: "distortion_tangential_y",
    })?;
    Ok([
        radial_x
            .checked_add(tangential_x)
            .ok_or(ProjectionError::ArithmeticOverflow {
                field: "distorted_x",
            })?,
        radial_y
            .checked_add(tangential_y)
            .ok_or(ProjectionError::ArithmeticOverflow {
                field: "distorted_y",
            })?,
    ])
}

fn fixed_product(left: i128, right: i128, field: &'static str) -> Result<i128, ProjectionError> {
    let numerator = left
        .checked_mul(right)
        .ok_or(ProjectionError::ArithmeticOverflow { field })?;
    divide_round_nearest(numerator, i128::from(NANO_SCALE), field)
}

fn validate_rectified_policy(policy: RectifiedProjectionPolicy) -> Result<(), ProjectionError> {
    validate_common_policy(
        policy.minimum_positive_depth_nano,
        policy.max_projected_coordinate_abs_nano_pixels,
    )
}

fn validate_calibrated_policy(policy: CalibratedProjectionPolicy) -> Result<(), ProjectionError> {
    validate_common_policy(
        policy.minimum_positive_depth_nano,
        policy.max_projected_coordinate_abs_nano_pixels,
    )?;
    if policy.max_normalized_coordinate_abs_nano == 0 {
        return Err(ProjectionError::ZeroValue {
            field: "max_normalized_coordinate_abs_nano",
        });
    }
    if policy.max_normalized_coordinate_abs_nano > MAX_NORMALIZED_COORDINATE_ABS_NANO {
        return Err(ProjectionError::BoundExceeded {
            field: "max_normalized_coordinate_abs_nano",
            actual: u128::from(policy.max_normalized_coordinate_abs_nano),
            maximum: u128::from(MAX_NORMALIZED_COORDINATE_ABS_NANO),
        });
    }
    Ok(())
}

fn validate_common_policy(
    minimum_positive_depth_nano: u64,
    max_projected_coordinate_abs_nano_pixels: u64,
) -> Result<(), ProjectionError> {
    if minimum_positive_depth_nano == 0 {
        return Err(ProjectionError::ZeroValue {
            field: "minimum_positive_depth_nano",
        });
    }
    if minimum_positive_depth_nano > MAX_CAMERA_POINT_ABS_NANO {
        return Err(ProjectionError::BoundExceeded {
            field: "minimum_positive_depth_nano",
            actual: u128::from(minimum_positive_depth_nano),
            maximum: u128::from(MAX_CAMERA_POINT_ABS_NANO),
        });
    }
    if max_projected_coordinate_abs_nano_pixels == 0 {
        return Err(ProjectionError::ZeroValue {
            field: "max_projected_coordinate_abs_nano_pixels",
        });
    }
    if max_projected_coordinate_abs_nano_pixels > MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS {
        return Err(ProjectionError::BoundExceeded {
            field: "max_projected_coordinate_abs_nano_pixels",
            actual: u128::from(max_projected_coordinate_abs_nano_pixels),
            maximum: u128::from(MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS),
        });
    }
    Ok(())
}

fn validate_camera_point(point_camera_nano: [i64; 3]) -> Result<(), ProjectionError> {
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
    Ok(())
}

fn validate_depth(depth_nano: i64, minimum: u64) -> Result<i64, ProjectionError> {
    if depth_nano <= 0 || depth_nano.unsigned_abs() < minimum {
        Err(ProjectionError::InsufficientPositiveDepth {
            depth_nano,
            minimum_positive_depth_nano: minimum,
        })
    } else {
        Ok(depth_nano)
    }
}

fn validate_projected_coordinate(
    field: &'static str,
    value: i64,
    maximum: u64,
) -> Result<(), ProjectionError> {
    let magnitude = value.unsigned_abs();
    if magnitude > maximum {
        Err(ProjectionError::BoundExceeded {
            field,
            actual: u128::from(magnitude),
            maximum: u128::from(maximum),
        })
    } else {
        Ok(())
    }
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

fn project_normalized_axis(
    focal_nano_pixels: i64,
    normalized_nano: i128,
    principal_nano_pixels: i64,
    field: &'static str,
) -> Result<i64, ProjectionError> {
    let offset = divide_round_nearest(
        i128::from(focal_nano_pixels)
            .checked_mul(normalized_nano)
            .ok_or(ProjectionError::ArithmeticOverflow { field })?,
        i128::from(NANO_SCALE),
        field,
    )?;
    let projected = i128::from(principal_nano_pixels)
        .checked_add(offset)
        .ok_or(ProjectionError::ArithmeticOverflow { field })?;
    i64::try_from(projected).map_err(|_| ProjectionError::ArithmeticOverflow { field })
}

fn within_image_domain(
    calibration: &DerivedCalibration,
    x_nano_pixels: i64,
    y_nano_pixels: i64,
) -> Result<bool, ProjectionError> {
    let width_nano = image_edge_nano(calibration.derivation.output_width, "image_width_nano")?;
    let height_nano = image_edge_nano(calibration.derivation.output_height, "image_height_nano")?;
    Ok(x_nano_pixels >= 0
        && x_nano_pixels < width_nano
        && y_nano_pixels >= 0
        && y_nano_pixels < height_nano)
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
