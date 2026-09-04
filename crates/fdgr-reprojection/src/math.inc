#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct OperationBudget {
    operations: u64,
    maximum: u64,
}

impl OperationBudget {
    const fn new(maximum: u64) -> Self {
        Self {
            operations: 0,
            maximum,
        }
    }

    fn charge(&mut self, field: &'static str) -> Result<(), ReprojectionError> {
        let attempted = self
            .operations
            .checked_add(1)
            .ok_or(ReprojectionError::ArithmeticOverflow { field })?;
        if attempted > self.maximum {
            return Err(ReprojectionError::OperationBudgetExceeded {
                attempted,
                maximum: self.maximum,
            });
        }
        self.operations = attempted;
        Ok(())
    }
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), ReprojectionError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(ReprojectionError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn validate_safe_nonzero(field: &'static str, value: u64) -> Result<(), ReprojectionError> {
    if value == 0 {
        return Err(ReprojectionError::ZeroValue { field });
    }
    validate_safe_number(field, value)
}

fn validate_safe_number(field: &'static str, value: u64) -> Result<(), ReprojectionError> {
    if value > MAX_SAFE_JSON_INTEGER {
        Err(ReprojectionError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        })
    } else {
        Ok(())
    }
}

fn validate_basis(basis: &ReprojectionBasis) -> Result<(), ReprojectionError> {
    for (field, digest) in [
        ("bundle_admission_digest", &basis.bundle_admission_digest),
        ("calibration_table_digest", &basis.calibration_table_digest),
        ("algorithm_digest", &basis.algorithm_digest),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_nonzero("generation", basis.generation)
}

fn validate_policy(policy: ReprojectionPolicy) -> Result<(), ReprojectionError> {
    if policy.projection.minimum_positive_depth_nano == 0 {
        return Err(ReprojectionError::ZeroValue {
            field: "minimum_positive_depth_nano",
        });
    }
    if policy.projection.minimum_positive_depth_nano > MAX_CAMERA_POINT_ABS_NANO {
        return Err(ReprojectionError::BoundExceeded {
            field: "minimum_positive_depth_nano",
            actual: u128::from(policy.projection.minimum_positive_depth_nano),
            maximum: u128::from(MAX_CAMERA_POINT_ABS_NANO),
        });
    }
    if policy.projection.max_normalized_coordinate_abs_nano == 0 {
        return Err(ReprojectionError::ZeroValue {
            field: "max_normalized_coordinate_abs_nano",
        });
    }
    if policy.projection.max_normalized_coordinate_abs_nano
        > MAX_NORMALIZED_COORDINATE_ABS_NANO
    {
        return Err(ReprojectionError::BoundExceeded {
            field: "max_normalized_coordinate_abs_nano",
            actual: u128::from(policy.projection.max_normalized_coordinate_abs_nano),
            maximum: u128::from(MAX_NORMALIZED_COORDINATE_ABS_NANO),
        });
    }
    if policy
        .projection
        .max_projected_coordinate_abs_nano_pixels
        == 0
    {
        return Err(ReprojectionError::ZeroValue {
            field: "max_projected_coordinate_abs_nano_pixels",
        });
    }
    if policy.projection.max_projected_coordinate_abs_nano_pixels
        > MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS
    {
        return Err(ReprojectionError::BoundExceeded {
            field: "max_projected_coordinate_abs_nano_pixels",
            actual: u128::from(
                policy.projection.max_projected_coordinate_abs_nano_pixels,
            ),
            maximum: u128::from(MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS),
        });
    }
    if policy.max_residual_nano_pixels == 0 {
        return Err(ReprojectionError::ZeroValue {
            field: "max_residual_nano_pixels",
        });
    }
    if policy.max_residual_nano_pixels > MAX_REPROJECTION_RESIDUAL_NANO_PIXELS {
        return Err(ReprojectionError::BoundExceeded {
            field: "max_residual_nano_pixels",
            actual: u128::from(policy.max_residual_nano_pixels),
            maximum: u128::from(MAX_REPROJECTION_RESIDUAL_NANO_PIXELS),
        });
    }
    if policy.max_normalized_residual_ppm == 0 {
        return Err(ReprojectionError::ZeroValue {
            field: "max_normalized_residual_ppm",
        });
    }
    if policy.max_normalized_residual_ppm > MAX_REPROJECTION_NORMALIZED_RESIDUAL_PPM {
        return Err(ReprojectionError::BoundExceeded {
            field: "max_normalized_residual_ppm",
            actual: u128::from(policy.max_normalized_residual_ppm),
            maximum: u128::from(MAX_REPROJECTION_NORMALIZED_RESIDUAL_PPM),
        });
    }
    for (field, value) in [
        (
            "min_optimize_observations_per_component",
            policy.min_optimize_observations_per_component,
        ),
        (
            "min_held_out_observations_per_component",
            policy.min_held_out_observations_per_component,
        ),
    ] {
        if value == 0 {
            return Err(ReprojectionError::ZeroValue { field });
        }
    }
    if policy.max_operations == 0 {
        return Err(ReprojectionError::ZeroValue {
            field: "max_operations",
        });
    }
    if policy.max_operations > MAX_REPROJECTION_OPERATIONS {
        return Err(ReprojectionError::BoundExceeded {
            field: "max_operations",
            actual: u128::from(policy.max_operations),
            maximum: u128::from(MAX_REPROJECTION_OPERATIONS),
        });
    }
    Ok(())
}

fn root_point_to_camera(
    rotation_node_from_root_nano: [i64; 9],
    camera_center_from_root_nano: [i64; 3],
    point_from_root_nano: [i64; 3],
) -> Result<[i64; 3], ReprojectionError> {
    let mut relative = [0_i64; 3];
    for index in 0..3 {
        let point = *point_from_root_nano
            .get(index)
            .ok_or(ReprojectionError::InternalLookup {
                field: "root_point_coordinate",
            })?;
        let center = *camera_center_from_root_nano
            .get(index)
            .ok_or(ReprojectionError::InternalLookup {
                field: "camera_center_coordinate",
            })?;
        *relative
            .get_mut(index)
            .ok_or(ReprojectionError::InternalLookup {
                field: "relative_point_coordinate",
            })? = point
            .checked_sub(center)
            .ok_or(ReprojectionError::ArithmeticOverflow {
                field: "point_minus_camera_center",
            })?;
    }
    let mut output = [0_i64; 3];
    for row in 0..3 {
        let mut total = 0_i128;
        for column in 0..3 {
            let rotation_index = row
                .checked_mul(3)
                .and_then(|value| value.checked_add(column))
                .ok_or(ReprojectionError::ArithmeticOverflow {
                    field: "rotation_index",
                })?;
            let rotation = i128::from(*rotation_node_from_root_nano.get(rotation_index).ok_or(
                ReprojectionError::InternalLookup {
                    field: "camera_rotation",
                },
            )?);
            let coordinate = i128::from(*relative.get(column).ok_or(
                ReprojectionError::InternalLookup {
                    field: "relative_point",
                },
            )?);
            total = total
                .checked_add(rotation.checked_mul(coordinate).ok_or(
                    ReprojectionError::ArithmeticOverflow {
                        field: "camera_transform_product",
                    },
                )?)
                .ok_or(ReprojectionError::ArithmeticOverflow {
                    field: "camera_transform_sum",
                })?;
        }
        let scaled = divide_round_nearest(
            total,
            i128::from(NANO_SCALE),
            "camera_transform_scale",
        )?;
        *output
            .get_mut(row)
            .ok_or(ReprojectionError::InternalLookup {
                field: "camera_transform_output",
            })? = i64::try_from(scaled).map_err(|_| ReprojectionError::ArithmeticOverflow {
            field: "camera_transform_output",
        })?;
    }
    Ok(output)
}

fn divide_round_nearest(
    numerator: i128,
    denominator: i128,
    field: &'static str,
) -> Result<i128, ReprojectionError> {
    if denominator <= 0 {
        return Err(ReprojectionError::ZeroValue { field });
    }
    let quotient = numerator / denominator;
    let remainder = numerator % denominator;
    let doubled = remainder
        .unsigned_abs()
        .checked_mul(2)
        .ok_or(ReprojectionError::ArithmeticOverflow { field })?;
    if doubled < denominator.unsigned_abs() {
        return Ok(quotient);
    }
    if numerator >= 0 {
        quotient
            .checked_add(1)
            .ok_or(ReprojectionError::ArithmeticOverflow { field })
    } else {
        quotient
            .checked_sub(1)
            .ok_or(ReprojectionError::ArithmeticOverflow { field })
    }
}

fn residual_magnitude(dx: i64, dy: i64) -> Result<u64, ReprojectionError> {
    let dx = i128::from(dx).unsigned_abs();
    let dy = i128::from(dy).unsigned_abs();
    let squared = dx
        .checked_mul(dx)
        .and_then(|value| dy.checked_mul(dy).and_then(|term| value.checked_add(term)))
        .ok_or(ReprojectionError::ArithmeticOverflow {
            field: "residual_squared",
        })?;
    u64::try_from(integer_sqrt(squared)).map_err(|_| ReprojectionError::ArithmeticOverflow {
        field: "residual_magnitude",
    })
}

fn integer_sqrt(value: u128) -> u128 {
    if value < 2 {
        return value;
    }
    let mut low = 1_u128;
    let mut high = value.min(u128::from(u64::MAX));
    let mut answer = 1_u128;
    while low <= high {
        let middle = low + (high - low) / 2;
        if middle <= value / middle {
            answer = middle;
            low = middle.saturating_add(1);
        } else {
            high = middle.saturating_sub(1);
        }
    }
    answer
}

fn normalized_residual_ppm(
    residual_nano_pixels: u64,
    observation_uncertainty_nano_pixels: u64,
    calibration_uncertainty_nano_pixels: u64,
) -> Result<u64, ReprojectionError> {
    let denominator = observation_uncertainty_nano_pixels
        .max(calibration_uncertainty_nano_pixels)
        .max(1);
    let scaled = u128::from(residual_nano_pixels)
        .checked_mul(u128::from(PPM_SCALE))
        .ok_or(ReprojectionError::ArithmeticOverflow {
            field: "normalized_residual",
        })?;
    let rounded = scaled
        .checked_add(u128::from(denominator) / 2)
        .ok_or(ReprojectionError::ArithmeticOverflow {
            field: "normalized_residual",
        })?
        / u128::from(denominator);
    u64::try_from(rounded).map_err(|_| ReprojectionError::ArithmeticOverflow {
        field: "normalized_residual",
    })
}

fn summarize_residuals(
    eligible: u64,
    evidence: &[&ReprojectionObservationEvidence],
) -> Result<ReprojectionResidualSummary, ReprojectionError> {
    let mut residuals = evidence
        .iter()
        .filter_map(|entry| entry.residual_nano_pixels)
        .collect::<Vec<_>>();
    residuals.sort_unstable();
    let projected_observation_count = u64::try_from(residuals.len()).map_err(|_| {
        ReprojectionError::ArithmeticOverflow {
            field: "projected_observation_count",
        }
    })?;
    let inlier_observation_count = u64::try_from(
        evidence
            .iter()
            .filter(|entry| {
                entry.disposition == ReprojectionObservationDisposition::ProjectedInlier
            })
            .count(),
    )
    .map_err(|_| ReprojectionError::ArithmeticOverflow {
        field: "inlier_observation_count",
    })?;
    let (rms, median, maximum) = if residuals.is_empty() {
        (None, None, None)
    } else {
        let mut squared_sum = 0_u128;
        for residual in &residuals {
            let value = u128::from(*residual);
            squared_sum = squared_sum
                .checked_add(value.checked_mul(value).ok_or(
                    ReprojectionError::ArithmeticOverflow {
                        field: "rms_residual_square",
                    },
                )?)
                .ok_or(ReprojectionError::ArithmeticOverflow {
                    field: "rms_residual_sum",
                })?;
        }
        let mean = squared_sum / u128::from(projected_observation_count);
        let rms = u64::try_from(integer_sqrt(mean)).map_err(|_| {
            ReprojectionError::ArithmeticOverflow {
                field: "rms_residual",
            }
        })?;
        let middle = residuals.len() / 2;
        let median = if residuals.len() % 2 == 1 {
            *residuals
                .get(middle)
                .ok_or(ReprojectionError::InternalLookup {
                    field: "median_residual",
                })?
        } else {
            let left = u128::from(*residuals.get(middle.saturating_sub(1)).ok_or(
                ReprojectionError::InternalLookup {
                    field: "median_residual_left",
                },
            )?);
            let right = u128::from(*residuals.get(middle).ok_or(
                ReprojectionError::InternalLookup {
                    field: "median_residual_right",
                },
            )?);
            u64::try_from((left + right) / 2).map_err(|_| {
                ReprojectionError::ArithmeticOverflow {
                    field: "median_residual",
                }
            })?
        };
        let maximum = *residuals
            .last()
            .ok_or(ReprojectionError::InternalLookup {
                field: "maximum_residual",
            })?;
        (Some(rms), Some(median), Some(maximum))
    };
    Ok(ReprojectionResidualSummary {
        eligible_observation_count: eligible,
        projected_observation_count,
        inlier_observation_count,
        rms_residual_nano_pixels: rms,
        median_residual_nano_pixels: median,
        maximum_residual_nano_pixels: maximum,
    })
}

const fn role_code(role: BundleObservationRole) -> u8 {
    match role {
        BundleObservationRole::Optimize => 1,
        BundleObservationRole::HeldOut => 2,
    }
}

fn encode_optional_u64(encoder: &mut Encoder, value: Option<u64>) {
    match value {
        Some(value) => {
            encoder.put_bool(true);
            encoder.put_u64(value);
        }
        None => encoder.put_bool(false),
    }
}

fn encode_optional_i64(encoder: &mut Encoder, value: Option<i64>) {
    match value {
        Some(value) => {
            encoder.put_bool(true);
            encoder.put_i64(value);
        }
        None => encoder.put_bool(false),
    }
}

fn encode_u64s(encoder: &mut Encoder, values: &[u64]) -> Result<(), ReprojectionError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn usize_to_u64(value: usize) -> Result<u64, ReprojectionError> {
    u64::try_from(value).map_err(|_| ReprojectionError::ArithmeticOverflow {
        field: "collection_length",
    })
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |value| value.to_string())
}

fn optional_i64_json(value: Option<i64>) -> String {
    value.map_or_else(|| "null".to_owned(), |value| value.to_string())
}

fn optional_bool_json(value: Option<bool>) -> &'static str {
    match value {
        Some(true) => "true",
        Some(false) => "false",
        None => "null",
    }
}

fn push_u64s(output: &mut String, values: &[u64]) -> Result<(), ReprojectionError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn json_error(error: fmt::Error) -> ReprojectionError {
    ReprojectionError::JsonRendering(error.to_string())
}
