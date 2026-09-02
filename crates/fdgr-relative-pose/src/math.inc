const MAX_UNIT_COMPONENT_NANO: i64 = 2_000_000_000;
const UNIT_SQUARED: i128 = 1_000_000_000_000_000_000;
const UNIT_CUBED: i128 = 1_000_000_000_000_000_000_000_000_000;

fn validate_unit_vector(
    field: &'static str,
    vector: [i64; 3],
) -> Result<(), RelativePoseError> {
    for value in vector {
        if value.unsigned_abs() > MAX_UNIT_COMPONENT_NANO.unsigned_abs() {
            return Err(RelativePoseError::BoundExceeded {
                field,
                actual: u128::from(value.unsigned_abs()),
                maximum: u128::from(MAX_UNIT_COMPONENT_NANO.unsigned_abs()),
            });
        }
    }
    let error_ppm = norm_error_ppm(vector)?;
    if error_ppm > MAX_UNIT_NORM_ERROR_PPM {
        return Err(RelativePoseError::UnitVectorInvalid {
            field,
            error_ppm,
            maximum_ppm: MAX_UNIT_NORM_ERROR_PPM,
        });
    }
    Ok(())
}

fn validate_rotation(rotation: [i64; 9]) -> Result<(), RelativePoseError> {
    for value in rotation {
        if value.unsigned_abs() > MAX_UNIT_COMPONENT_NANO.unsigned_abs() {
            return Err(RelativePoseError::BoundExceeded {
                field: "rotation_component_nano",
                actual: u128::from(value.unsigned_abs()),
                maximum: u128::from(MAX_UNIT_COMPONENT_NANO.unsigned_abs()),
            });
        }
    }
    for index in 0..3 {
        let row = rotation_row(rotation, index)?;
        let column = rotation_column(rotation, index)?;
        let row_error = norm_error_ppm(row)?;
        if row_error > MAX_ROTATION_ERROR_PPM {
            return Err(RelativePoseError::RotationNormInvalid {
                field: match index {
                    0 => "row_0",
                    1 => "row_1",
                    _ => "row_2",
                },
                error_ppm: row_error,
            });
        }
        let column_error = norm_error_ppm(column)?;
        if column_error > MAX_ROTATION_ERROR_PPM {
            return Err(RelativePoseError::RotationNormInvalid {
                field: match index {
                    0 => "column_0",
                    1 => "column_1",
                    _ => "column_2",
                },
                error_ppm: column_error,
            });
        }
    }
    for (left, right, field) in [
        (0, 1, "rows_0_1"),
        (0, 2, "rows_0_2"),
        (1, 2, "rows_1_2"),
    ] {
        let error_ppm = orthogonality_error_ppm(
            rotation_row(rotation, left)?,
            rotation_row(rotation, right)?,
        )?;
        if error_ppm > MAX_ROTATION_ERROR_PPM {
            return Err(RelativePoseError::RotationOrthogonalityInvalid {
                field,
                error_ppm,
            });
        }
    }
    let determinant_error = determinant_error_ppm(rotation)?;
    if determinant_error > MAX_ROTATION_DETERMINANT_ERROR_PPM {
        return Err(RelativePoseError::RotationDeterminantInvalid {
            error_ppm: determinant_error,
        });
    }
    Ok(())
}

fn rotation_row(rotation: [i64; 9], row: usize) -> Result<[i64; 3], RelativePoseError> {
    let start = row
        .checked_mul(3)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_row_index",
        })?;
    Ok([
        *rotation
            .get(start)
            .ok_or(RelativePoseError::InternalLookup {
                field: "rotation_row_0",
            })?,
        *rotation
            .get(start.saturating_add(1))
            .ok_or(RelativePoseError::InternalLookup {
                field: "rotation_row_1",
            })?,
        *rotation
            .get(start.saturating_add(2))
            .ok_or(RelativePoseError::InternalLookup {
                field: "rotation_row_2",
            })?,
    ])
}

fn rotation_column(
    rotation: [i64; 9],
    column: usize,
) -> Result<[i64; 3], RelativePoseError> {
    Ok([
        *rotation
            .get(column)
            .ok_or(RelativePoseError::InternalLookup {
                field: "rotation_column_0",
            })?,
        *rotation
            .get(column.saturating_add(3))
            .ok_or(RelativePoseError::InternalLookup {
                field: "rotation_column_1",
            })?,
        *rotation
            .get(column.saturating_add(6))
            .ok_or(RelativePoseError::InternalLookup {
                field: "rotation_column_2",
            })?,
    ])
}

fn checked_dot(
    left: [i64; 3],
    right: [i64; 3],
    field: &'static str,
) -> Result<i128, RelativePoseError> {
    let mut total = 0_i128;
    for index in 0..3 {
        let left_value = *left.get(index).ok_or(RelativePoseError::InternalLookup { field })?;
        let right_value = *right.get(index).ok_or(RelativePoseError::InternalLookup { field })?;
        let product = i128::from(left_value)
            .checked_mul(i128::from(right_value))
            .ok_or(RelativePoseError::ArithmeticOverflow { field })?;
        total = total
            .checked_add(product)
            .ok_or(RelativePoseError::ArithmeticOverflow { field })?;
    }
    Ok(total)
}

fn norm_error_ppm(vector: [i64; 3]) -> Result<u64, RelativePoseError> {
    let norm_squared = checked_dot(vector, vector, "unit_norm_squared")?;
    let difference = norm_squared
        .checked_sub(UNIT_SQUARED)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "unit_norm_difference",
        })?
        .unsigned_abs();
    ratio_u128_to_u64(
        difference,
        UNIT_SQUARED.unsigned_abs(),
        u128::from(PPM_SCALE),
        "unit_norm_error_ppm",
    )
}

fn orthogonality_error_ppm(
    left: [i64; 3],
    right: [i64; 3],
) -> Result<u64, RelativePoseError> {
    ratio_u128_to_u64(
        checked_dot(left, right, "rotation_orthogonality")?.unsigned_abs(),
        UNIT_SQUARED.unsigned_abs(),
        u128::from(PPM_SCALE),
        "rotation_orthogonality_ppm",
    )
}

fn determinant_error_ppm(rotation: [i64; 9]) -> Result<u64, RelativePoseError> {
    let value = |index: usize| -> Result<i128, RelativePoseError> {
        rotation
            .get(index)
            .copied()
            .map(i128::from)
            .ok_or(RelativePoseError::InternalLookup {
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
        .and_then(|value| {
            value.checked_add(checked_triple(c, d, h, "rotation_determinant").ok()?)
        })
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_determinant",
        })?;
    let negative = checked_triple(c, e, g, "rotation_determinant")?
        .checked_add(checked_triple(b, d, i, "rotation_determinant")?)
        .and_then(|value| {
            value.checked_add(checked_triple(a, f, h, "rotation_determinant").ok()?)
        })
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_determinant",
        })?;
    let determinant = positive
        .checked_sub(negative)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_determinant",
        })?;
    let difference = determinant
        .checked_sub(UNIT_CUBED)
        .ok_or(RelativePoseError::ArithmeticOverflow {
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
) -> Result<i128, RelativePoseError> {
    first
        .checked_mul(second)
        .and_then(|value| value.checked_mul(third))
        .ok_or(RelativePoseError::ArithmeticOverflow { field })
}

fn rotate_bearing(
    rotation: [i64; 9],
    bearing: [i64; 3],
) -> Result<[i64; 3], RelativePoseError> {
    let mut output = [0_i64; 3];
    for row in 0..3 {
        let row_vector = rotation_row(rotation, row)?;
        let raw = checked_dot(row_vector, bearing, "rotation_bearing_dot")?;
        let scaled = divide_round_nearest(raw, i128::from(UNIT_NANO), "rotation_bearing")?;
        let slot = output
            .get_mut(row)
            .ok_or(RelativePoseError::InternalLookup {
                field: "rotated_bearing_component",
            })?;
        *slot = i64::try_from(scaled).map_err(|_| RelativePoseError::ArithmeticOverflow {
            field: "rotated_bearing_component",
        })?;
    }
    Ok(output)
}

fn cross_scaled(
    left: [i64; 3],
    right: [i64; 3],
) -> Result<[i64; 3], RelativePoseError> {
    let raw = [
        checked_cross_term(left[1], right[2], left[2], right[1])?,
        checked_cross_term(left[2], right[0], left[0], right[2])?,
        checked_cross_term(left[0], right[1], left[1], right[0])?,
    ];
    let mut output = [0_i64; 3];
    for (index, value) in raw.into_iter().enumerate() {
        let scaled = divide_round_nearest(value, i128::from(UNIT_NANO), "scaled_cross")?;
        let slot = output
            .get_mut(index)
            .ok_or(RelativePoseError::InternalLookup {
                field: "scaled_cross_component",
            })?;
        *slot = i64::try_from(scaled).map_err(|_| RelativePoseError::ArithmeticOverflow {
            field: "scaled_cross_component",
        })?;
    }
    Ok(output)
}

fn checked_cross_term(
    a: i64,
    b: i64,
    c: i64,
    d: i64,
) -> Result<i128, RelativePoseError> {
    i128::from(a)
        .checked_mul(i128::from(b))
        .and_then(|left| {
            i128::from(c)
                .checked_mul(i128::from(d))
                .and_then(|right| left.checked_sub(right))
        })
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "cross_product",
        })
}

fn vector_norm_nano(vector: [i64; 3]) -> Result<u64, RelativePoseError> {
    let squared = checked_dot(vector, vector, "vector_norm_squared")?;
    if squared < 0 {
        return Err(RelativePoseError::ArithmeticOverflow {
            field: "vector_norm_squared",
        });
    }
    let value = u128::try_from(squared).map_err(|_| RelativePoseError::ArithmeticOverflow {
        field: "vector_norm_squared",
    })?;
    u64::try_from(integer_sqrt(value)).map_err(|_| RelativePoseError::ArithmeticOverflow {
        field: "vector_norm",
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

fn divide_round_nearest(
    numerator: i128,
    denominator: i128,
    field: &'static str,
) -> Result<i128, RelativePoseError> {
    if denominator <= 0 {
        return Err(RelativePoseError::ZeroValue { field });
    }
    let quotient = numerator / denominator;
    let remainder = numerator % denominator;
    let doubled = remainder
        .unsigned_abs()
        .checked_mul(2)
        .ok_or(RelativePoseError::ArithmeticOverflow { field })?;
    if doubled < denominator.unsigned_abs() {
        return Ok(quotient);
    }
    if numerator >= 0 {
        quotient
            .checked_add(1)
            .ok_or(RelativePoseError::ArithmeticOverflow { field })
    } else {
        quotient
            .checked_sub(1)
            .ok_or(RelativePoseError::ArithmeticOverflow { field })
    }
}

fn ratio_u128_to_u64(
    numerator: u128,
    denominator: u128,
    scale: u128,
    field: &'static str,
) -> Result<u64, RelativePoseError> {
    if denominator == 0 {
        return Err(RelativePoseError::ZeroValue { field });
    }
    let scaled = numerator
        .checked_mul(scale)
        .ok_or(RelativePoseError::ArithmeticOverflow { field })?;
    let rounded = scaled
        .checked_add(denominator / 2)
        .ok_or(RelativePoseError::ArithmeticOverflow { field })?
        / denominator;
    u64::try_from(rounded).map_err(|_| RelativePoseError::ArithmeticOverflow { field })
}

fn ratio_ppm(numerator: u32, denominator: u32) -> Result<u32, RelativePoseError> {
    if denominator == 0 {
        return Ok(0);
    }
    let value = ratio_u128_to_u64(
        u128::from(numerator),
        u128::from(denominator),
        u128::from(PPM_SCALE),
        "aggregate_ratio_ppm",
    )?;
    u32::try_from(value).map_err(|_| RelativePoseError::ArithmeticOverflow {
        field: "aggregate_ratio_ppm",
    })
}

fn median_u64(mut values: Vec<u64>, empty: u64) -> u64 {
    if values.is_empty() {
        return empty;
    }
    values.sort_unstable();
    let middle = values.len() / 2;
    if values.len() % 2 == 1 {
        values.get(middle).copied().unwrap_or(empty)
    } else {
        let left = values.get(middle.saturating_sub(1)).copied().unwrap_or(empty);
        let right = values.get(middle).copied().unwrap_or(empty);
        left.saturating_add(right).saturating_add(1) / 2
    }
}
