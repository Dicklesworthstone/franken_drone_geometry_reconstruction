const fn greatest_common_divisor(mut left: u64, mut right: u64) -> u64 {
    while right != 0 {
        let remainder = left % right;
        left = right;
        right = remainder;
    }
    left
}

fn normalized_fraction(numerator: u64, denominator: u64) -> Result<(u64, u64), EdgeScaleError> {
    if numerator == 0 {
        return Err(EdgeScaleError::ZeroValue { field: "ratio_numerator" });
    }
    if denominator == 0 {
        return Err(EdgeScaleError::ZeroValue { field: "ratio_denominator" });
    }
    let divisor = greatest_common_divisor(numerator, denominator);
    if divisor == 0 {
        return Err(EdgeScaleError::ArithmeticOverflow { field: "ratio_gcd" });
    }
    Ok((numerator / divisor, denominator / divisor))
}

fn fraction_residual_ppm(
    left_numerator: u64,
    left_denominator: u64,
    right_numerator: u64,
    right_denominator: u64,
) -> Result<u64, EdgeScaleError> {
    let left = u128::from(left_numerator)
        .checked_mul(u128::from(right_denominator))
        .ok_or(EdgeScaleError::ArithmeticOverflow { field: "ratio_cross_product" })?;
    let right = u128::from(right_numerator)
        .checked_mul(u128::from(left_denominator))
        .ok_or(EdgeScaleError::ArithmeticOverflow { field: "ratio_cross_product" })?;
    symmetric_residual_ppm(left, right, "fraction_residual_ppm")
}

fn symmetric_residual_ppm(
    left: u128,
    right: u128,
    field: &'static str,
) -> Result<u64, EdgeScaleError> {
    let denominator = left.max(right);
    if denominator == 0 {
        return Err(EdgeScaleError::ZeroValue { field });
    }
    let difference = left.abs_diff(right);
    let numerator = difference
        .checked_mul(u128::from(PPM_SCALE))
        .ok_or(EdgeScaleError::ArithmeticOverflow { field })?;
    let rounded_up = numerator
        .checked_add(denominator.saturating_sub(1))
        .ok_or(EdgeScaleError::ArithmeticOverflow { field })?
        / denominator;
    u64::try_from(rounded_up).map_err(|_| EdgeScaleError::ArithmeticOverflow { field })
}

fn fraction_to_nano(numerator: u64, denominator: u64) -> Result<u64, EdgeScaleError> {
    mul_div_round(numerator, UNIT_NANO, denominator, "ratio_nano")
}

fn mul_div_round(
    left: u64,
    right: u64,
    denominator: u64,
    field: &'static str,
) -> Result<u64, EdgeScaleError> {
    if denominator == 0 {
        return Err(EdgeScaleError::ZeroValue { field });
    }
    let product = u128::from(left)
        .checked_mul(u128::from(right))
        .ok_or(EdgeScaleError::ArithmeticOverflow { field })?;
    let rounded = product
        .checked_add(u128::from(denominator / 2))
        .ok_or(EdgeScaleError::ArithmeticOverflow { field })?
        / u128::from(denominator);
    u64::try_from(rounded).map_err(|_| EdgeScaleError::ArithmeticOverflow { field })
}

fn implied_ratio_nano(lower_scale: u64, higher_scale: u64) -> Result<u64, EdgeScaleError> {
    mul_div_round(lower_scale, UNIT_NANO, higher_scale, "implied_ratio_nano")
}

fn relation_residual_ppm(implied_ratio_nano: u64, observed_ratio_nano: u64) -> Result<u64, EdgeScaleError> {
    symmetric_residual_ppm(
        u128::from(implied_ratio_nano),
        u128::from(observed_ratio_nano),
        "relation_residual_ppm",
    )
}

fn graph_priority(relation: &EdgeScaleRelation) -> EdgePriority {
    let independent_groups = u64::try_from(relation.supporting_correlation_group_ids.len())
        .unwrap_or(u64::MAX)
        .min(999_999);
    let primary = independent_groups
        .saturating_mul(1_000_000_000_000)
        .saturating_add(relation.evidence_rank.min(999).saturating_mul(1_000_000_000))
        .saturating_add(relation.independent_support_count.min(999_999_999));
    EdgePriority {
        primary,
        secondary: u64::from(relation.uncertainty_ppm),
    }
}

fn compare_fraction(
    left_numerator: u64,
    left_denominator: u64,
    right_numerator: u64,
    right_denominator: u64,
) -> Ordering {
    u128::from(left_numerator)
        .saturating_mul(u128::from(right_denominator))
        .cmp(&u128::from(right_numerator).saturating_mul(u128::from(left_denominator)))
}

fn validate_safe_nonzero(field: &'static str, value: u64) -> Result<(), EdgeScaleError> {
    if value == 0 {
        return Err(EdgeScaleError::ZeroValue { field });
    }
    validate_safe_number(field, value)
}

fn validate_safe_number(field: &'static str, value: u64) -> Result<(), EdgeScaleError> {
    if value > MAX_SAFE_JSON_INTEGER {
        return Err(EdgeScaleError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        });
    }
    Ok(())
}

fn reject_zero_digest(field: &'static str, digest: &EvidenceDigest) -> Result<(), EdgeScaleError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(EdgeScaleError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

const fn usize_to_u128(value: usize) -> u128 { value as u128 }

fn usize_to_u64(value: usize) -> Result<u64, EdgeScaleError> {
    u64::try_from(value).map_err(|_| EdgeScaleError::ArithmeticOverflow { field: "collection_length" })
}

fn json_rendering(error: fmt::Error) -> EdgeScaleError {
    EdgeScaleError::JsonRendering(error.to_string())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}
