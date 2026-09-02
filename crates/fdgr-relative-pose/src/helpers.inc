fn validate_basis(basis: &RelativePoseBasis) -> Result<(), RelativePoseError> {
    for (field, digest) in [
        (
            "epipolar_verification_digest",
            &basis.epipolar_verification_digest,
        ),
        (
            "correspondence_generation_digest",
            &basis.correspondence_generation_digest,
        ),
        ("calibration_digest", &basis.calibration_digest),
        ("observation_basis_digest", &basis.observation_basis_digest),
        ("candidate_basis_digest", &basis.candidate_basis_digest),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_identity("pair_id", basis.pair_id)?;
    validate_safe_number("left_sample_index", basis.left_sample_index)?;
    validate_safe_number("right_sample_index", basis.right_sample_index)?;
    validate_safe_identity("generation", basis.generation)?;
    if basis.left_sample_index >= basis.right_sample_index {
        return Err(RelativePoseError::NonCanonicalFramePair);
    }
    Ok(())
}

fn validate_policy(policy: RelativePosePolicy) -> Result<(), RelativePoseError> {
    if policy.min_supported_matches == 0 {
        return Err(RelativePoseError::ZeroValue {
            field: "min_supported_matches",
        });
    }
    if u128::from(policy.min_supported_matches) > usize_to_u128(MAX_POSE_OBSERVATIONS) {
        return Err(RelativePoseError::BoundExceeded {
            field: "min_supported_matches",
            actual: u128::from(policy.min_supported_matches),
            maximum: usize_to_u128(MAX_POSE_OBSERVATIONS),
        });
    }
    if policy.min_supported_ratio_ppm == 0 || u64::from(policy.min_supported_ratio_ppm) > MAX_POSE_PPM {
        return Err(RelativePoseError::BoundExceeded {
            field: "min_supported_ratio_ppm",
            actual: u128::from(policy.min_supported_ratio_ppm),
            maximum: u128::from(MAX_POSE_PPM),
        });
    }
    if policy.min_parallax_sine_ppm == 0 || u64::from(policy.min_parallax_sine_ppm) > MAX_POSE_PPM {
        return Err(RelativePoseError::BoundExceeded {
            field: "min_parallax_sine_ppm",
            actual: u128::from(policy.min_parallax_sine_ppm),
            maximum: u128::from(MAX_POSE_PPM),
        });
    }
    if policy.min_spatial_bins_per_image == 0 {
        return Err(RelativePoseError::ZeroValue {
            field: "min_spatial_bins_per_image",
        });
    }
    if policy.max_evaluations == 0 {
        return Err(RelativePoseError::ZeroValue {
            field: "max_evaluations",
        });
    }
    if policy.max_evaluations > MAX_POSE_EVALUATIONS {
        return Err(RelativePoseError::BoundExceeded {
            field: "max_evaluations",
            actual: u128::from(policy.max_evaluations),
            maximum: u128::from(MAX_POSE_EVALUATIONS),
        });
    }
    Ok(())
}

fn canonicalize_observations(
    mut observations: Vec<NormalizedCorrespondence>,
    basis: &RelativePoseBasis,
) -> Result<Vec<NormalizedCorrespondence>, RelativePoseError> {
    if observations.is_empty() {
        return Err(RelativePoseError::EmptyObservations);
    }
    if observations.len() > MAX_POSE_OBSERVATIONS {
        return Err(RelativePoseError::BoundExceeded {
            field: "observation_count",
            actual: usize_to_u128(observations.len()),
            maximum: usize_to_u128(MAX_POSE_OBSERVATIONS),
        });
    }
    observations.sort_by(|left, right| {
        left.match_id
            .cmp(&right.match_id)
            .then(left.left_observation_id.cmp(&right.left_observation_id))
            .then(left.right_observation_id.cmp(&right.right_observation_id))
    });
    let mut match_ids = BTreeSet::new();
    for observation in &observations {
        validate_safe_identity("match_id", observation.match_id)?;
        validate_safe_identity("left_observation_id", observation.left_observation_id)?;
        validate_safe_identity("right_observation_id", observation.right_observation_id)?;
        if observation.pair_id != basis.pair_id {
            return Err(RelativePoseError::PairIdentityMismatch {
                match_id: observation.match_id,
                expected: basis.pair_id,
                observed: observation.pair_id,
            });
        }
        if !match_ids.insert(observation.match_id) {
            return Err(RelativePoseError::DuplicateMatchId {
                match_id: observation.match_id,
            });
        }
        for (field, value) in [
            ("left_x_ppm", observation.left_x_ppm),
            ("left_y_ppm", observation.left_y_ppm),
            ("right_x_ppm", observation.right_x_ppm),
            ("right_y_ppm", observation.right_y_ppm),
        ] {
            if value.unsigned_abs() > fdgr_epipolar::MAX_ABS_COORDINATE_PPM.unsigned_abs() {
                return Err(RelativePoseError::BoundExceeded {
                    field,
                    actual: u128::from(value.unsigned_abs()),
                    maximum: u128::from(
                        fdgr_epipolar::MAX_ABS_COORDINATE_PPM.unsigned_abs(),
                    ),
                });
            }
        }
        if u64::from(observation.uncertainty_ppm) > MAX_POSE_PPM {
            return Err(RelativePoseError::BoundExceeded {
                field: "uncertainty_ppm",
                actual: u128::from(observation.uncertainty_ppm),
                maximum: u128::from(MAX_POSE_PPM),
            });
        }
        if observation.left_spatial_bin == 0 {
            return Err(RelativePoseError::ZeroValue {
                field: "left_spatial_bin",
            });
        }
        if observation.right_spatial_bin == 0 {
            return Err(RelativePoseError::ZeroValue {
                field: "right_spatial_bin",
            });
        }
    }
    Ok(observations)
}

fn canonicalize_candidates(
    mut candidates: Vec<RelativePoseCandidate>,
) -> Result<Vec<RelativePoseCandidate>, RelativePoseError> {
    if candidates.is_empty() {
        return Err(RelativePoseError::EmptyCandidates);
    }
    if candidates.len() > MAX_POSE_CANDIDATES {
        return Err(RelativePoseError::BoundExceeded {
            field: "candidate_count",
            actual: usize_to_u128(candidates.len()),
            maximum: usize_to_u128(MAX_POSE_CANDIDATES),
        });
    }
    candidates.sort_by(|left, right| left.candidate_id.cmp(&right.candidate_id));
    let mut ids = BTreeSet::new();
    let mut poses = BTreeSet::new();
    for candidate in &candidates {
        validate_safe_identity("candidate_id", candidate.candidate_id)?;
        validate_safe_identity("essential_candidate_id", candidate.essential_candidate_id)?;
        if !ids.insert(candidate.candidate_id) {
            return Err(RelativePoseError::DuplicateCandidateId {
                candidate_id: candidate.candidate_id,
            });
        }
        validate_rotation(candidate.candidate_id, &candidate.rotation_nano)?;
        validate_translation_direction(
            candidate.candidate_id,
            &candidate.translation_direction_nano,
        )?;
        if let Some((first_candidate_id, _, _)) = poses
            .iter()
            .find(|(_, rotation, translation)| {
                rotation == &candidate.rotation_nano
                    && translation == &candidate.translation_direction_nano
            })
        {
            return Err(RelativePoseError::DuplicateCandidatePose {
                first_candidate_id: *first_candidate_id,
                duplicate_candidate_id: candidate.candidate_id,
            });
        }
        poses.insert((
            candidate.candidate_id,
            candidate.rotation_nano,
            candidate.translation_direction_nano,
        ));
    }
    Ok(candidates)
}

fn validate_rotation(candidate_id: u64, rotation: &[i64; 9]) -> Result<(), RelativePoseError> {
    for coefficient in rotation {
        if coefficient.unsigned_abs() > MAX_TRANSLATION_DIRECTION_NANO.unsigned_abs() {
            return Err(RelativePoseError::BoundExceeded {
                field: "rotation_coefficient_nano",
                actual: u128::from(coefficient.unsigned_abs()),
                maximum: u128::from(MAX_TRANSLATION_DIRECTION_NANO.unsigned_abs()),
            });
        }
    }
    let scale = i128::from(NANO_SCALE);
    let target_norm = scale
        .checked_mul(scale)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_scale_squared",
        })?;
    for left in 0..3 {
        let norm = row_dot(rotation, left, left)?;
        if abs_i128(norm.checked_sub(target_norm).ok_or(
            RelativePoseError::ArithmeticOverflow {
                field: "rotation_row_norm",
            },
        )?) > ROTATION_DOT_TOLERANCE
        {
            return Err(RelativePoseError::RotationNotOrthonormal { candidate_id });
        }
        for right in left.saturating_add(1)..3 {
            if abs_i128(row_dot(rotation, left, right)?) > ROTATION_DOT_TOLERANCE {
                return Err(RelativePoseError::RotationNotOrthonormal { candidate_id });
            }
        }
    }
    for left in 0..3 {
        let norm = column_dot(rotation, left, left)?;
        if abs_i128(norm.checked_sub(target_norm).ok_or(
            RelativePoseError::ArithmeticOverflow {
                field: "rotation_column_norm",
            },
        )?) > ROTATION_DOT_TOLERANCE
        {
            return Err(RelativePoseError::RotationNotOrthonormal { candidate_id });
        }
        for right in left.saturating_add(1)..3 {
            if abs_i128(column_dot(rotation, left, right)?) > ROTATION_DOT_TOLERANCE {
                return Err(RelativePoseError::RotationNotOrthonormal { candidate_id });
            }
        }
    }
    let target_determinant = target_norm
        .checked_mul(scale)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_scale_cubed",
        })?;
    let determinant_error = determinant(rotation)?
        .checked_sub(target_determinant)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_determinant_error",
        })?;
    if abs_i128(determinant_error) > ROTATION_DETERMINANT_TOLERANCE {
        return Err(RelativePoseError::RotationDeterminantInvalid { candidate_id });
    }
    Ok(())
}

fn validate_translation_direction(
    candidate_id: u64,
    translation: &[i64; 3],
) -> Result<(), RelativePoseError> {
    let mut norm = 0_i128;
    for coefficient in translation {
        if coefficient.unsigned_abs() > MAX_TRANSLATION_DIRECTION_NANO.unsigned_abs() {
            return Err(RelativePoseError::BoundExceeded {
                field: "translation_direction_nano",
                actual: u128::from(coefficient.unsigned_abs()),
                maximum: u128::from(MAX_TRANSLATION_DIRECTION_NANO.unsigned_abs()),
            });
        }
        norm = norm
            .checked_add(
                i128::from(*coefficient)
                    .checked_mul(i128::from(*coefficient))
                    .ok_or(RelativePoseError::ArithmeticOverflow {
                        field: "translation_norm",
                    })?,
            )
            .ok_or(RelativePoseError::ArithmeticOverflow {
                field: "translation_norm",
            })?;
    }
    let target = i128::from(NANO_SCALE)
        .checked_mul(i128::from(NANO_SCALE))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "translation_scale_squared",
        })?;
    if abs_i128(norm.checked_sub(target).ok_or(
        RelativePoseError::ArithmeticOverflow {
            field: "translation_norm_error",
        },
    )?) > TRANSLATION_NORM_TOLERANCE
    {
        return Err(RelativePoseError::TranslationDirectionNotUnit { candidate_id });
    }
    Ok(())
}

fn triangulate_signs(
    candidate: &RelativePoseCandidate,
    observation: &NormalizedCorrespondence,
    min_parallax_sine_ppm: u32,
) -> Result<TriangulationEvidence, RelativePoseError> {
    let left_ray = [
        observation.left_x_ppm,
        observation.left_y_ppm,
        fdgr_epipolar::COORDINATE_SCALE_PPM,
    ];
    let right_ray_camera = [
        observation.right_x_ppm,
        observation.right_y_ppm,
        fdgr_epipolar::COORDINATE_SCALE_PPM,
    ];
    let right_ray = rotate_transpose_scaled(&candidate.rotation_nano, &right_ray_camera)?;
    let rotated_translation = rotate_transpose_scaled(
        &candidate.rotation_nano,
        &candidate.translation_direction_nano,
    )?;
    let right_center = [
        checked_neg(rotated_translation[0], "right_center_x")?,
        checked_neg(rotated_translation[1], "right_center_y")?,
        checked_neg(rotated_translation[2], "right_center_z")?,
    ];
    let a = dot_i64(&left_ray, &left_ray)?;
    let b = dot_i64(&left_ray, &right_ray)?;
    let c = dot_i64(&right_ray, &right_ray)?;
    let d = dot_i64(&left_ray, &right_center)?;
    let e = dot_i64(&right_ray, &right_center)?;
    let denominator = a
        .checked_mul(c)
        .and_then(|value| value.checked_sub(b.checked_mul(b)?))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "closest_ray_denominator",
        })?;
    if denominator <= 0 {
        return Ok(TriangulationEvidence {
            candidate_id: candidate.candidate_id,
            match_id: observation.match_id,
            left_depth_numerator: 0,
            right_depth_numerator: 0,
            depth_denominator: 0,
            parallax_sine_ppm: 0,
            disposition: TriangulationDisposition::DegenerateRays,
        });
    }
    let left_depth_numerator = c
        .checked_mul(d)
        .and_then(|value| value.checked_sub(b.checked_mul(e)?))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "left_depth_numerator",
        })?;
    let right_depth_numerator = b
        .checked_mul(d)
        .and_then(|value| value.checked_sub(a.checked_mul(e)?))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "right_depth_numerator",
        })?;
    let raw_parallax = conservative_parallax_sine_ppm(&left_ray, &right_ray)?;
    let uncertainty_penalty = observation.uncertainty_ppm.saturating_mul(2);
    let parallax_sine_ppm = raw_parallax.saturating_sub(uncertainty_penalty);
    let disposition = if left_depth_numerator <= 0 {
        TriangulationDisposition::BehindLeft
    } else if right_depth_numerator <= 0 {
        TriangulationDisposition::BehindRight
    } else if parallax_sine_ppm < min_parallax_sine_ppm {
        TriangulationDisposition::LowParallax
    } else {
        TriangulationDisposition::Supported
    };
    Ok(TriangulationEvidence {
        candidate_id: candidate.candidate_id,
        match_id: observation.match_id,
        left_depth_numerator,
        right_depth_numerator,
        depth_denominator: denominator.unsigned_abs(),
        parallax_sine_ppm,
        disposition,
    })
}

fn rotate_transpose_scaled(
    rotation: &[i64; 9],
    vector: &[i64; 3],
) -> Result<[i64; 3], RelativePoseError> {
    let mut output = [0_i64; 3];
    for column in 0..3 {
        let mut total = 0_i128;
        for row in 0..3 {
            let index = row * 3 + column;
            total = total
                .checked_add(
                    i128::from(rotation[index])
                        .checked_mul(i128::from(vector[row]))
                        .ok_or(RelativePoseError::ArithmeticOverflow {
                            field: "rotation_vector_product",
                        })?,
                )
                .ok_or(RelativePoseError::ArithmeticOverflow {
                    field: "rotation_vector_sum",
                })?;
        }
        output[column] = i64::try_from(divide_round_nearest(
            total,
            i128::from(NANO_SCALE),
            "rotation_vector_scale",
        )?)
        .map_err(|_| RelativePoseError::ArithmeticOverflow {
            field: "rotation_vector_output",
        })?;
    }
    Ok(output)
}

fn conservative_parallax_sine_ppm(
    left: &[i64; 3],
    right: &[i64; 3],
) -> Result<u32, RelativePoseError> {
    let cross = cross_i64(left, right)?;
    let cross_squared = dot_i128_unsigned(&cross, &cross)?;
    let left_squared = dot_i64(left, left)?.unsigned_abs();
    let right_squared = dot_i64(right, right)?.unsigned_abs();
    let product = left_squared
        .checked_mul(right_squared)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "parallax_norm_product",
        })?;
    if product == 0 {
        return Ok(0);
    }
    let numerator_norm = integer_sqrt_floor(cross_squared);
    let denominator_norm = integer_sqrt_ceil(product)?;
    if denominator_norm == 0 {
        return Ok(0);
    }
    let scaled = numerator_norm
        .checked_mul(1_000_000)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "parallax_sine_ppm",
        })?
        / denominator_norm;
    Ok(u32::try_from(scaled.min(1_000_000)).map_err(|_| {
        RelativePoseError::ArithmeticOverflow {
            field: "parallax_sine_ppm",
        }
    })?)
}

fn cross_i64(left: &[i64; 3], right: &[i64; 3]) -> Result<[i128; 3], RelativePoseError> {
    Ok([
        checked_cross_term(left[1], right[2], left[2], right[1])?,
        checked_cross_term(left[2], right[0], left[0], right[2])?,
        checked_cross_term(left[0], right[1], left[1], right[0])?,
    ])
}

fn checked_cross_term(
    a: i64,
    b: i64,
    c: i64,
    d: i64,
) -> Result<i128, RelativePoseError> {
    i128::from(a)
        .checked_mul(i128::from(b))
        .and_then(|value| value.checked_sub(i128::from(c).checked_mul(i128::from(d))?))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "cross_product",
        })
}

fn dot_i64(left: &[i64; 3], right: &[i64; 3]) -> Result<i128, RelativePoseError> {
    let mut total = 0_i128;
    for index in 0..3 {
        total = total
            .checked_add(
                i128::from(left[index])
                    .checked_mul(i128::from(right[index]))
                    .ok_or(RelativePoseError::ArithmeticOverflow {
                        field: "dot_product",
                    })?,
            )
            .ok_or(RelativePoseError::ArithmeticOverflow {
                field: "dot_product",
            })?;
    }
    Ok(total)
}

fn dot_i128_unsigned(
    left: &[i128; 3],
    right: &[i128; 3],
) -> Result<u128, RelativePoseError> {
    let mut total = 0_u128;
    for index in 0..3 {
        let product = left[index]
            .checked_mul(right[index])
            .ok_or(RelativePoseError::ArithmeticOverflow {
                field: "wide_dot_product",
            })?;
        total = total
            .checked_add(product.unsigned_abs())
            .ok_or(RelativePoseError::ArithmeticOverflow {
                field: "wide_dot_product",
            })?;
    }
    Ok(total)
}

fn row_dot(rotation: &[i64; 9], left: usize, right: usize) -> Result<i128, RelativePoseError> {
    let mut total = 0_i128;
    for column in 0..3 {
        total = total
            .checked_add(
                i128::from(rotation[left * 3 + column])
                    .checked_mul(i128::from(rotation[right * 3 + column]))
                    .ok_or(RelativePoseError::ArithmeticOverflow {
                        field: "rotation_row_dot",
                    })?,
            )
            .ok_or(RelativePoseError::ArithmeticOverflow {
                field: "rotation_row_dot",
            })?;
    }
    Ok(total)
}

fn column_dot(rotation: &[i64; 9], left: usize, right: usize) -> Result<i128, RelativePoseError> {
    let mut total = 0_i128;
    for row in 0..3 {
        total = total
            .checked_add(
                i128::from(rotation[row * 3 + left])
                    .checked_mul(i128::from(rotation[row * 3 + right]))
                    .ok_or(RelativePoseError::ArithmeticOverflow {
                        field: "rotation_column_dot",
                    })?,
            )
            .ok_or(RelativePoseError::ArithmeticOverflow {
                field: "rotation_column_dot",
            })?;
    }
    Ok(total)
}

fn determinant(rotation: &[i64; 9]) -> Result<i128, RelativePoseError> {
    let positive = checked_triple(rotation[0], rotation[4], rotation[8])?
        .checked_add(checked_triple(rotation[1], rotation[5], rotation[6])?)
        .and_then(|value| value.checked_add(checked_triple(rotation[2], rotation[3], rotation[7]).ok()?))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_determinant",
        })?;
    let negative = checked_triple(rotation[2], rotation[4], rotation[6])?
        .checked_add(checked_triple(rotation[1], rotation[3], rotation[8])?)
        .and_then(|value| value.checked_add(checked_triple(rotation[0], rotation[5], rotation[7]).ok()?))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_determinant",
        })?;
    positive
        .checked_sub(negative)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_determinant",
        })
}

fn checked_triple(first: i64, second: i64, third: i64) -> Result<i128, RelativePoseError> {
    i128::from(first)
        .checked_mul(i128::from(second))
        .and_then(|value| value.checked_mul(i128::from(third)))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "rotation_determinant",
        })
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

fn integer_sqrt_floor(value: u128) -> u128 {
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

fn integer_sqrt_ceil(value: u128) -> Result<u128, RelativePoseError> {
    let floor = integer_sqrt_floor(value);
    if floor
        .checked_mul(floor)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "integer_sqrt",
        })?
        == value
    {
        Ok(floor)
    } else {
        floor
            .checked_add(1)
            .ok_or(RelativePoseError::ArithmeticOverflow {
                field: "integer_sqrt",
            })
    }
}

fn checked_neg(value: i64, field: &'static str) -> Result<i64, RelativePoseError> {
    value
        .checked_neg()
        .ok_or(RelativePoseError::ArithmeticOverflow { field })
}

const fn abs_i128(value: i128) -> i128 {
    if value < 0 { -value } else { value }
}

fn validate_safe_identity(field: &'static str, value: u64) -> Result<(), RelativePoseError> {
    if value == 0 {
        return Err(RelativePoseError::ZeroValue { field });
    }
    validate_safe_number(field, value)
}

fn validate_safe_number(field: &'static str, value: u64) -> Result<(), RelativePoseError> {
    if value > MAX_SAFE_JSON_INTEGER {
        return Err(RelativePoseError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        });
    }
    Ok(())
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), RelativePoseError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(RelativePoseError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn supported_ratio_ppm(supported: usize, total: usize) -> Result<u32, RelativePoseError> {
    let numerator = usize_to_u128(supported)
        .checked_mul(1_000_000)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "supported_ratio_ppm",
        })?;
    u32::try_from(numerator / usize_to_u128(total)).map_err(|_| {
        RelativePoseError::ArithmeticOverflow {
            field: "supported_ratio_ppm",
        }
    })
}

fn median_u32(values: &mut [u32]) -> u32 {
    values.sort_unstable();
    values.get(values.len() / 2).copied().unwrap_or(0)
}

const fn usize_to_u128(value: usize) -> u128 {
    value as u128
}

fn usize_to_u64(value: usize) -> Result<u64, RelativePoseError> {
    u64::try_from(value).map_err(|_| RelativePoseError::ArithmeticOverflow {
        field: "collection_length",
    })
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn json_rendering(error: std::fmt::Error) -> RelativePoseError {
    RelativePoseError::JsonRendering(error.to_string())
}
