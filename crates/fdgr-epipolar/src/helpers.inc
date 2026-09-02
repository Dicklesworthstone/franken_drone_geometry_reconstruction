fn validate_basis(basis: &EpipolarBasis) -> Result<(), EpipolarError> {
    for (field, digest) in [
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
        return Err(EpipolarError::NonCanonicalFramePair);
    }
    Ok(())
}

fn validate_policy(policy: EpipolarPolicy) -> Result<(), EpipolarError> {
    if policy.max_residual_ppm == 0 {
        return Err(EpipolarError::ZeroValue {
            field: "max_residual_ppm",
        });
    }
    validate_residual_bound("max_residual_ppm", policy.max_residual_ppm)?;
    if policy.min_inliers == 0 {
        return Err(EpipolarError::ZeroValue {
            field: "min_inliers",
        });
    }
    if usize::try_from(policy.min_inliers).map_or(true, |value| value > MAX_EPIPOLAR_OBSERVATIONS)
    {
        return Err(EpipolarError::BoundExceeded {
            field: "min_inliers",
            actual: u128::from(policy.min_inliers),
            maximum: usize_to_u128(MAX_EPIPOLAR_OBSERVATIONS),
        });
    }
    if policy.min_inlier_ratio_ppm == 0 || policy.min_inlier_ratio_ppm > 1_000_000 {
        return Err(EpipolarError::BoundExceeded {
            field: "min_inlier_ratio_ppm",
            actual: u128::from(policy.min_inlier_ratio_ppm),
            maximum: 1_000_000,
        });
    }
    if policy.min_spatial_bins_per_image == 0 {
        return Err(EpipolarError::ZeroValue {
            field: "min_spatial_bins_per_image",
        });
    }
    if policy.max_determinant_residual_ppm > 1_000_000 {
        return Err(EpipolarError::BoundExceeded {
            field: "max_determinant_residual_ppm",
            actual: u128::from(policy.max_determinant_residual_ppm),
            maximum: 1_000_000,
        });
    }
    if policy.max_evaluations == 0 {
        return Err(EpipolarError::ZeroValue {
            field: "max_evaluations",
        });
    }
    if policy.max_evaluations > MAX_EPIPOLAR_EVALUATIONS {
        return Err(EpipolarError::BoundExceeded {
            field: "max_evaluations",
            actual: u128::from(policy.max_evaluations),
            maximum: u128::from(MAX_EPIPOLAR_EVALUATIONS),
        });
    }
    Ok(())
}

fn canonicalize_observations(
    mut observations: Vec<NormalizedCorrespondence>,
    basis: &EpipolarBasis,
) -> Result<Vec<NormalizedCorrespondence>, EpipolarError> {
    if observations.is_empty() {
        return Err(EpipolarError::EmptyObservations);
    }
    if observations.len() > MAX_EPIPOLAR_OBSERVATIONS {
        return Err(EpipolarError::BoundExceeded {
            field: "observation_count",
            actual: usize_to_u128(observations.len()),
            maximum: usize_to_u128(MAX_EPIPOLAR_OBSERVATIONS),
        });
    }
    observations.sort_by(|left, right| {
        left.match_id
            .cmp(&right.match_id)
            .then(left.left_observation_id.cmp(&right.left_observation_id))
            .then(left.right_observation_id.cmp(&right.right_observation_id))
    });
    let mut match_ids = BTreeSet::new();
    let mut endpoint_pairs = BTreeSet::new();
    for observation in &observations {
        validate_safe_identity("match_id", observation.match_id)?;
        validate_safe_identity("left_observation_id", observation.left_observation_id)?;
        validate_safe_identity("right_observation_id", observation.right_observation_id)?;
        if observation.pair_id != basis.pair_id {
            return Err(EpipolarError::PairIdentityMismatch {
                match_id: observation.match_id,
                expected: basis.pair_id,
                observed: observation.pair_id,
            });
        }
        if !match_ids.insert(observation.match_id) {
            return Err(EpipolarError::DuplicateMatchId {
                match_id: observation.match_id,
            });
        }
        if !endpoint_pairs.insert((
            observation.left_observation_id,
            observation.right_observation_id,
        )) {
            return Err(EpipolarError::DuplicateObservationPair {
                left_observation_id: observation.left_observation_id,
                right_observation_id: observation.right_observation_id,
            });
        }
        for (field, value) in [
            ("left_x_ppm", observation.left_x_ppm),
            ("left_y_ppm", observation.left_y_ppm),
            ("right_x_ppm", observation.right_x_ppm),
            ("right_y_ppm", observation.right_y_ppm),
        ] {
            if value.unsigned_abs() > MAX_ABS_COORDINATE_PPM.unsigned_abs() {
                return Err(EpipolarError::BoundExceeded {
                    field,
                    actual: u128::from(value.unsigned_abs()),
                    maximum: u128::from(MAX_ABS_COORDINATE_PPM.unsigned_abs()),
                });
            }
        }
        validate_residual_bound("uncertainty_ppm", u64::from(observation.uncertainty_ppm))?;
        if observation.left_spatial_bin == 0 {
            return Err(EpipolarError::ZeroValue {
                field: "left_spatial_bin",
            });
        }
        if observation.right_spatial_bin == 0 {
            return Err(EpipolarError::ZeroValue {
                field: "right_spatial_bin",
            });
        }
    }
    Ok(observations)
}

fn canonicalize_candidates(
    mut candidates: Vec<EssentialCandidate>,
) -> Result<Vec<EssentialCandidate>, EpipolarError> {
    if candidates.is_empty() {
        return Err(EpipolarError::EmptyCandidates);
    }
    if candidates.len() > MAX_EPIPOLAR_CANDIDATES {
        return Err(EpipolarError::BoundExceeded {
            field: "candidate_count",
            actual: usize_to_u128(candidates.len()),
            maximum: usize_to_u128(MAX_EPIPOLAR_CANDIDATES),
        });
    }
    candidates.sort_by(|left, right| left.candidate_id.cmp(&right.candidate_id));
    let mut candidate_ids = BTreeSet::new();
    let mut matrices = BTreeMap::<[i64; 9], u64>::new();
    for candidate in &mut candidates {
        validate_safe_identity("candidate_id", candidate.candidate_id)?;
        if !candidate_ids.insert(candidate.candidate_id) {
            return Err(EpipolarError::DuplicateCandidateId {
                candidate_id: candidate.candidate_id,
            });
        }
        candidate.matrix = normalize_matrix(candidate.candidate_id, candidate.matrix)?;
        if let Some(first_candidate_id) = matrices.insert(candidate.matrix, candidate.candidate_id) {
            return Err(EpipolarError::DuplicateCandidateMatrix {
                first_candidate_id,
                duplicate_candidate_id: candidate.candidate_id,
            });
        }
    }
    Ok(candidates)
}

fn normalize_matrix(
    candidate_id: u64,
    matrix: [i64; 9],
) -> Result<[i64; 9], EpipolarError> {
    let mut divisor = 0_u64;
    let mut nonzero = false;
    for coefficient in matrix {
        let magnitude = coefficient.unsigned_abs();
        if magnitude > MAX_RAW_MATRIX_COEFFICIENT.unsigned_abs() {
            return Err(EpipolarError::MatrixCoefficientExceeded {
                candidate_id,
                stage: "raw",
                actual: u128::from(magnitude),
                maximum: u128::from(MAX_RAW_MATRIX_COEFFICIENT.unsigned_abs()),
            });
        }
        if magnitude != 0 {
            nonzero = true;
            divisor = if divisor == 0 {
                magnitude
            } else {
                greatest_common_divisor(divisor, magnitude)
            };
        }
    }
    if !nonzero || divisor == 0 {
        return Err(EpipolarError::ZeroMatrix { candidate_id });
    }
    let divisor_i64 = i64::try_from(divisor).map_err(|_| EpipolarError::ArithmeticOverflow {
        field: "matrix_gcd",
    })?;
    let mut normalized = [0_i64; 9];
    for (index, coefficient) in matrix.iter().enumerate() {
        normalized[index] = coefficient.checked_div(divisor_i64).ok_or(
            EpipolarError::ArithmeticOverflow {
                field: "matrix_normalization",
            },
        )?;
    }
    let first_nonzero = normalized
        .iter()
        .copied()
        .find(|coefficient| *coefficient != 0)
        .ok_or(EpipolarError::ZeroMatrix { candidate_id })?;
    if first_nonzero < 0 {
        for coefficient in &mut normalized {
            *coefficient = coefficient.checked_neg().ok_or(
                EpipolarError::ArithmeticOverflow {
                    field: "matrix_sign_normalization",
                },
            )?;
        }
    }
    for coefficient in normalized {
        let magnitude = coefficient.unsigned_abs();
        if magnitude > MAX_MATRIX_COEFFICIENT.unsigned_abs() {
            return Err(EpipolarError::MatrixCoefficientExceeded {
                candidate_id,
                stage: "normalized",
                actual: u128::from(magnitude),
                maximum: u128::from(MAX_MATRIX_COEFFICIENT.unsigned_abs()),
            });
        }
    }
    Ok(normalized)
}

const fn greatest_common_divisor(mut left: u64, mut right: u64) -> u64 {
    while right != 0 {
        let remainder = left % right;
        left = right;
        right = remainder;
    }
    left
}

fn determinant_residual_ppm(matrix: &[i64; 9]) -> Result<u64, EpipolarError> {
    let determinant = determinant(matrix)?.unsigned_abs();
    let maximum = matrix
        .iter()
        .map(|coefficient| coefficient.unsigned_abs())
        .max()
        .unwrap_or(0);
    if maximum == 0 {
        return Ok(1_000_000);
    }
    let denominator = u128::from(maximum)
        .checked_mul(u128::from(maximum))
        .and_then(|value| value.checked_mul(u128::from(maximum)))
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "determinant_normalizer",
        })?;
    let numerator = determinant
        .checked_mul(1_000_000)
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "determinant_residual_ppm",
        })?;
    let value = divide_u128_ceil(numerator, denominator, "determinant_residual_ppm")?;
    u64::try_from(value).map_err(|_| EpipolarError::ArithmeticOverflow {
        field: "determinant_residual_ppm",
    })
}

fn determinant(matrix: &[i64; 9]) -> Result<i128, EpipolarError> {
    let value = |index: usize| -> Result<i128, EpipolarError> {
        matrix
            .get(index)
            .copied()
            .map(i128::from)
            .ok_or(EpipolarError::ArithmeticOverflow {
                field: "matrix_index",
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
    let positive = checked_triple(a, e, i, "determinant")?
        .checked_add(checked_triple(b, f, g, "determinant")?)
        .and_then(|value| value.checked_add(checked_triple(c, d, h, "determinant").ok()?))
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "determinant",
        })?;
    let negative = checked_triple(c, e, g, "determinant")?
        .checked_add(checked_triple(b, d, i, "determinant")?)
        .and_then(|value| value.checked_add(checked_triple(a, f, h, "determinant").ok()?))
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "determinant",
        })?;
    positive
        .checked_sub(negative)
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "determinant",
        })
}

fn checked_triple(
    first: i128,
    second: i128,
    third: i128,
    field: &'static str,
) -> Result<i128, EpipolarError> {
    first
        .checked_mul(second)
        .and_then(|value| value.checked_mul(third))
        .ok_or(EpipolarError::ArithmeticOverflow { field })
}

fn symmetric_l1_residual_ppm(
    matrix: &[i64; 9],
    observation: &NormalizedCorrespondence,
) -> Result<Option<u64>, EpipolarError> {
    let left = [
        i128::from(observation.left_x_ppm),
        i128::from(observation.left_y_ppm),
        i128::from(COORDINATE_SCALE_PPM),
    ];
    let right = [
        i128::from(observation.right_x_ppm),
        i128::from(observation.right_y_ppm),
        i128::from(COORDINATE_SCALE_PPM),
    ];
    let right_line = multiply_matrix_vector(matrix, &left, false)?;
    let left_line = multiply_matrix_vector(matrix, &right, true)?;
    let numerator = dot3(&right, &right_line)?.unsigned_abs();
    let denominator = right_line[0]
        .unsigned_abs()
        .checked_add(right_line[1].unsigned_abs())
        .and_then(|value| value.checked_add(left_line[0].unsigned_abs()))
        .and_then(|value| value.checked_add(left_line[1].unsigned_abs()))
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "epipolar_line_normalizer",
        })?;
    if denominator == 0 {
        return Ok(None);
    }
    let residual = divide_u128_ceil(numerator, denominator, "epipolar_residual")?;
    Ok(Some(u64::try_from(residual).map_err(|_| {
        EpipolarError::ArithmeticOverflow {
            field: "epipolar_residual",
        }
    })?))
}

fn multiply_matrix_vector(
    matrix: &[i64; 9],
    vector: &[i128; 3],
    transpose: bool,
) -> Result<[i128; 3], EpipolarError> {
    let mut output = [0_i128; 3];
    for row in 0..3 {
        let mut total = 0_i128;
        for column in 0..3 {
            let index = if transpose {
                column * 3 + row
            } else {
                row * 3 + column
            };
            let coefficient = i128::from(*matrix.get(index).ok_or(
                EpipolarError::ArithmeticOverflow {
                    field: "matrix_vector_index",
                },
            )?);
            total = total
                .checked_add(coefficient.checked_mul(vector[column]).ok_or(
                    EpipolarError::ArithmeticOverflow {
                        field: "matrix_vector_product",
                    },
                )?)
                .ok_or(EpipolarError::ArithmeticOverflow {
                    field: "matrix_vector_sum",
                })?;
        }
        output[row] = total;
    }
    Ok(output)
}

fn dot3(left: &[i128; 3], right: &[i128; 3]) -> Result<i128, EpipolarError> {
    let mut total = 0_i128;
    for index in 0..3 {
        total = total
            .checked_add(left[index].checked_mul(right[index]).ok_or(
                EpipolarError::ArithmeticOverflow {
                    field: "epipolar_dot_product",
                },
            )?)
            .ok_or(EpipolarError::ArithmeticOverflow {
                field: "epipolar_dot_sum",
            })?;
    }
    Ok(total)
}

fn divide_u128_ceil(
    numerator: u128,
    denominator: u128,
    field: &'static str,
) -> Result<u128, EpipolarError> {
    if denominator == 0 {
        return Err(EpipolarError::ZeroValue { field });
    }
    numerator
        .checked_add(denominator.saturating_sub(1))
        .map(|value| value / denominator)
        .ok_or(EpipolarError::ArithmeticOverflow { field })
}

fn inlier_ratio_ppm(inliers: usize, total: usize) -> Result<u32, EpipolarError> {
    let numerator = usize_to_u128(inliers)
        .checked_mul(1_000_000)
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "inlier_ratio_ppm",
        })?;
    let denominator = usize_to_u128(total);
    let value = numerator / denominator;
    u32::try_from(value).map_err(|_| EpipolarError::ArithmeticOverflow {
        field: "inlier_ratio_ppm",
    })
}

fn median_u64(values: &mut [u64]) -> u64 {
    values.sort_unstable();
    values.get(values.len() / 2).copied().unwrap_or(0)
}

fn validate_residual_bound(field: &'static str, value: u64) -> Result<(), EpipolarError> {
    if value > MAX_EPIPOLAR_RESIDUAL_PPM {
        return Err(EpipolarError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_EPIPOLAR_RESIDUAL_PPM),
        });
    }
    Ok(())
}

fn validate_safe_identity(field: &'static str, value: u64) -> Result<(), EpipolarError> {
    if value == 0 {
        return Err(EpipolarError::ZeroValue { field });
    }
    validate_safe_number(field, value)
}

fn validate_safe_number(field: &'static str, value: u64) -> Result<(), EpipolarError> {
    if value > MAX_SAFE_JSON_INTEGER {
        return Err(EpipolarError::BoundExceeded {
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
) -> Result<(), EpipolarError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(EpipolarError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

const fn usize_to_u128(value: usize) -> u128 {
    value as u128
}

fn usize_to_u64(value: usize) -> Result<u64, EpipolarError> {
    u64::try_from(value).map_err(|_| EpipolarError::ArithmeticOverflow {
        field: "collection_length",
    })
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn json_rendering(error: std::fmt::Error) -> EpipolarError {
    EpipolarError::JsonRendering(error.to_string())
}
