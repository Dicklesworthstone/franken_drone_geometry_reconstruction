const UNIT_SQUARED: i128 = 1_000_000_000_000_000_000;
const UNIT_CUBED: i128 = 1_000_000_000_000_000_000_000_000_000;
const MAX_COMPONENT_NANO: i64 = 2_000_000_000;

fn identity_rotation() -> [i64; 9] {
    [UNIT_NANO, 0, 0, 0, UNIT_NANO, 0, 0, 0, UNIT_NANO]
}

fn transpose_rotation(rotation: [i64; 9]) -> [i64; 9] {
    [
        rotation[0], rotation[3], rotation[6], rotation[1], rotation[4], rotation[7],
        rotation[2], rotation[5], rotation[8],
    ]
}

fn multiply_rotations(
    left: [i64; 9],
    right: [i64; 9],
) -> Result<[i64; 9], PoseGraphError> {
    let mut output = [0_i64; 9];
    for row in 0..3 {
        for column in 0..3 {
            let mut total = 0_i128;
            for inner in 0..3 {
                let left_index = row * 3 + inner;
                let right_index = inner * 3 + column;
                let left_value = i128::from(*left.get(left_index).ok_or(
                    PoseGraphError::InternalLookup {
                        field: "rotation_multiply_left",
                    },
                )?);
                let right_value = i128::from(*right.get(right_index).ok_or(
                    PoseGraphError::InternalLookup {
                        field: "rotation_multiply_right",
                    },
                )?);
                total = total
                    .checked_add(left_value.checked_mul(right_value).ok_or(
                        PoseGraphError::ArithmeticOverflow {
                            field: "rotation_multiply_product",
                        },
                    )?)
                    .ok_or(PoseGraphError::ArithmeticOverflow {
                        field: "rotation_multiply_sum",
                    })?;
            }
            let scaled = divide_round_nearest(
                total,
                i128::from(UNIT_NANO),
                "rotation_multiply_scale",
            )?;
            *output
                .get_mut(row * 3 + column)
                .ok_or(PoseGraphError::InternalLookup {
                    field: "rotation_multiply_output",
                })? = i64::try_from(scaled).map_err(|_| {
                PoseGraphError::ArithmeticOverflow {
                    field: "rotation_multiply_output",
                }
            })?;
        }
    }
    Ok(output)
}

fn validate_rotation(
    field: &'static str,
    rotation: [i64; 9],
    maximum_ppm: u64,
) -> Result<(), PoseGraphError> {
    let error_ppm = rotation_error_ppm(rotation)?;
    if error_ppm > maximum_ppm {
        Err(PoseGraphError::RotationInvalid {
            field,
            error_ppm,
            maximum_ppm,
        })
    } else {
        Ok(())
    }
}

fn rotation_error_ppm(rotation: [i64; 9]) -> Result<u64, PoseGraphError> {
    for value in rotation {
        if value.unsigned_abs() > MAX_COMPONENT_NANO.unsigned_abs() {
            return Ok(MAX_ROTATION_ERROR_PPM);
        }
    }
    let mut maximum = 0_u64;
    for index in 0..3 {
        maximum = maximum.max(norm_error_ppm(rotation_row(rotation, index)?)?);
        maximum = maximum.max(norm_error_ppm(rotation_column(rotation, index)?)?);
    }
    for (left, right) in [(0, 1), (0, 2), (1, 2)] {
        maximum = maximum.max(orthogonality_error_ppm(
            rotation_row(rotation, left)?,
            rotation_row(rotation, right)?,
        )?);
    }
    maximum = maximum.max(determinant_error_ppm(rotation)?);
    Ok(maximum)
}

fn validate_unit_direction(edge_id: u64, direction: [i64; 3]) -> Result<(), PoseGraphError> {
    for value in direction {
        if value.unsigned_abs() > MAX_COMPONENT_NANO.unsigned_abs() {
            return Err(PoseGraphError::TranslationDirectionInvalid {
                edge_id,
                error_ppm: MAX_ROTATION_ERROR_PPM,
            });
        }
    }
    let error_ppm = norm_error_ppm(direction)?;
    if error_ppm > MAX_INPUT_ROTATION_ERROR_PPM {
        Err(PoseGraphError::TranslationDirectionInvalid {
            edge_id,
            error_ppm,
        })
    } else {
        Ok(())
    }
}

fn rotation_difference_ppm(
    left: [i64; 9],
    right: [i64; 9],
) -> Result<u64, PoseGraphError> {
    let mut maximum = 0_u128;
    for index in 0..9 {
        let left_value = i128::from(*left.get(index).ok_or(
            PoseGraphError::InternalLookup {
                field: "rotation_difference_left",
            },
        )?);
        let right_value = i128::from(*right.get(index).ok_or(
            PoseGraphError::InternalLookup {
                field: "rotation_difference_right",
            },
        )?);
        maximum = maximum.max(
            left_value
                .checked_sub(right_value)
                .ok_or(PoseGraphError::ArithmeticOverflow {
                    field: "rotation_difference",
                })?
                .unsigned_abs(),
        );
    }
    ratio_u128_to_u64(
        maximum,
        u128::from(UNIT_NANO.unsigned_abs()),
        u128::from(PPM_SCALE),
        "rotation_difference_ppm",
    )
}

fn rotation_row(rotation: [i64; 9], row: usize) -> Result<[i64; 3], PoseGraphError> {
    let start = row
        .checked_mul(3)
        .ok_or(PoseGraphError::ArithmeticOverflow {
            field: "rotation_row_index",
        })?;
    Ok([
        *rotation.get(start).ok_or(PoseGraphError::InternalLookup {
            field: "rotation_row_0",
        })?,
        *rotation
            .get(start.saturating_add(1))
            .ok_or(PoseGraphError::InternalLookup {
                field: "rotation_row_1",
            })?,
        *rotation
            .get(start.saturating_add(2))
            .ok_or(PoseGraphError::InternalLookup {
                field: "rotation_row_2",
            })?,
    ])
}

fn rotation_column(
    rotation: [i64; 9],
    column: usize,
) -> Result<[i64; 3], PoseGraphError> {
    Ok([
        *rotation
            .get(column)
            .ok_or(PoseGraphError::InternalLookup {
                field: "rotation_column_0",
            })?,
        *rotation
            .get(column.saturating_add(3))
            .ok_or(PoseGraphError::InternalLookup {
                field: "rotation_column_1",
            })?,
        *rotation
            .get(column.saturating_add(6))
            .ok_or(PoseGraphError::InternalLookup {
                field: "rotation_column_2",
            })?,
    ])
}

fn dot3(left: [i64; 3], right: [i64; 3], field: &'static str) -> Result<i128, PoseGraphError> {
    let mut total = 0_i128;
    for index in 0..3 {
        let left_value = i128::from(*left.get(index).ok_or(
            PoseGraphError::InternalLookup { field },
        )?);
        let right_value = i128::from(*right.get(index).ok_or(
            PoseGraphError::InternalLookup { field },
        )?);
        total = total
            .checked_add(left_value.checked_mul(right_value).ok_or(
                PoseGraphError::ArithmeticOverflow { field },
            )?)
            .ok_or(PoseGraphError::ArithmeticOverflow { field })?;
    }
    Ok(total)
}

fn norm_error_ppm(vector: [i64; 3]) -> Result<u64, PoseGraphError> {
    let squared = dot3(vector, vector, "vector_norm_squared")?;
    let difference = squared
        .checked_sub(UNIT_SQUARED)
        .ok_or(PoseGraphError::ArithmeticOverflow {
            field: "vector_norm_difference",
        })?
        .unsigned_abs();
    ratio_u128_to_u64(
        difference,
        UNIT_SQUARED.unsigned_abs(),
        u128::from(PPM_SCALE),
        "vector_norm_error_ppm",
    )
}

fn orthogonality_error_ppm(
    left: [i64; 3],
    right: [i64; 3],
) -> Result<u64, PoseGraphError> {
    ratio_u128_to_u64(
        dot3(left, right, "rotation_orthogonality")?.unsigned_abs(),
        UNIT_SQUARED.unsigned_abs(),
        u128::from(PPM_SCALE),
        "rotation_orthogonality_ppm",
    )
}

fn determinant_error_ppm(rotation: [i64; 9]) -> Result<u64, PoseGraphError> {
    let value = |index: usize| -> Result<i128, PoseGraphError> {
        rotation
            .get(index)
            .copied()
            .map(i128::from)
            .ok_or(PoseGraphError::InternalLookup {
                field: "rotation_determinant_component",
            })
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
    let positive = checked_triple(a, e, i, "rotation_determinant")?
        .checked_add(checked_triple(b, f, g, "rotation_determinant")?)
        .and_then(|total| {
            checked_triple(c, d, h, "rotation_determinant")
                .ok()
                .and_then(|term| total.checked_add(term))
        })
        .ok_or(PoseGraphError::ArithmeticOverflow {
            field: "rotation_determinant",
        })?;
    let negative = checked_triple(c, e, g, "rotation_determinant")?
        .checked_add(checked_triple(b, d, i, "rotation_determinant")?)
        .and_then(|total| {
            checked_triple(a, f, h, "rotation_determinant")
                .ok()
                .and_then(|term| total.checked_add(term))
        })
        .ok_or(PoseGraphError::ArithmeticOverflow {
            field: "rotation_determinant",
        })?;
    let determinant = positive
        .checked_sub(negative)
        .ok_or(PoseGraphError::ArithmeticOverflow {
            field: "rotation_determinant",
        })?;
    let difference = determinant
        .checked_sub(UNIT_CUBED)
        .ok_or(PoseGraphError::ArithmeticOverflow {
            field: "rotation_determinant_difference",
        })?
        .unsigned_abs();
    ratio_u128_to_u64(
        difference,
        UNIT_CUBED.unsigned_abs(),
        u128::from(PPM_SCALE),
        "rotation_determinant_error_ppm",
    )
}

fn checked_triple(
    first: i128,
    second: i128,
    third: i128,
    field: &'static str,
) -> Result<i128, PoseGraphError> {
    first
        .checked_mul(second)
        .and_then(|value| value.checked_mul(third))
        .ok_or(PoseGraphError::ArithmeticOverflow { field })
}

fn divide_round_nearest(
    numerator: i128,
    denominator: i128,
    field: &'static str,
) -> Result<i128, PoseGraphError> {
    if denominator <= 0 {
        return Err(PoseGraphError::ZeroValue { field });
    }
    let quotient = numerator / denominator;
    let remainder = numerator % denominator;
    let doubled = remainder
        .unsigned_abs()
        .checked_mul(2)
        .ok_or(PoseGraphError::ArithmeticOverflow { field })?;
    if doubled < denominator.unsigned_abs() {
        return Ok(quotient);
    }
    if numerator >= 0 {
        quotient
            .checked_add(1)
            .ok_or(PoseGraphError::ArithmeticOverflow { field })
    } else {
        quotient
            .checked_sub(1)
            .ok_or(PoseGraphError::ArithmeticOverflow { field })
    }
}

fn ratio_u128_to_u64(
    numerator: u128,
    denominator: u128,
    scale: u128,
    field: &'static str,
) -> Result<u64, PoseGraphError> {
    if denominator == 0 {
        return Err(PoseGraphError::ZeroValue { field });
    }
    let scaled = numerator
        .checked_mul(scale)
        .ok_or(PoseGraphError::ArithmeticOverflow { field })?;
    let rounded = scaled
        .checked_add(denominator / 2)
        .ok_or(PoseGraphError::ArithmeticOverflow { field })?
        / denominator;
    u64::try_from(rounded).map_err(|_| PoseGraphError::ArithmeticOverflow { field })
}
