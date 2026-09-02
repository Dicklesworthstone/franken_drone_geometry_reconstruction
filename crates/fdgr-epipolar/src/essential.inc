/// Maximum scale-invariant residual of the essential cubic constraint.
///
/// A rank-two matrix is not necessarily essential. This bound applies to
/// `2 E E^T E - tr(E E^T) E`, normalized by the cube of the largest homogeneous
/// coefficient.
pub const MAX_ESSENTIAL_CONSTRAINT_RESIDUAL_PPM: u64 = 10_000;

fn validate_essential_candidates(
    candidates: &[EssentialCandidate],
) -> Result<(), EpipolarError> {
    for candidate in candidates {
        let residual_ppm = essential_constraint_residual_ppm(&candidate.matrix)?;
        if residual_ppm > MAX_ESSENTIAL_CONSTRAINT_RESIDUAL_PPM {
            return Err(EpipolarError::EssentialConstraintExceeded {
                candidate_id: candidate.candidate_id,
                residual_ppm,
                maximum_ppm: MAX_ESSENTIAL_CONSTRAINT_RESIDUAL_PPM,
            });
        }
    }
    Ok(())
}

fn essential_constraint_residual_ppm(
    matrix: &[i64; 9],
) -> Result<u64, EpipolarError> {
    let maximum = matrix
        .iter()
        .map(|coefficient| coefficient.unsigned_abs())
        .max()
        .unwrap_or(0);
    if maximum == 0 {
        return Ok(u64::MAX);
    }
    let mut gram = [0_i128; 9];
    for row in 0..3 {
        for column in 0..3 {
            let mut total = 0_i128;
            for inner in 0..3 {
                let left = i128::from(*matrix.get(row * 3 + inner).ok_or(
                    EpipolarError::ArithmeticOverflow {
                        field: "essential_gram_left_index",
                    },
                )?);
                let right = i128::from(*matrix.get(column * 3 + inner).ok_or(
                    EpipolarError::ArithmeticOverflow {
                        field: "essential_gram_right_index",
                    },
                )?);
                total = total
                    .checked_add(left.checked_mul(right).ok_or(
                        EpipolarError::ArithmeticOverflow {
                            field: "essential_gram_product",
                        },
                    )?)
                    .ok_or(EpipolarError::ArithmeticOverflow {
                        field: "essential_gram_sum",
                    })?;
            }
            *gram.get_mut(row * 3 + column).ok_or(
                EpipolarError::ArithmeticOverflow {
                    field: "essential_gram_output_index",
                },
            )? = total;
        }
    }
    let trace = gram[0]
        .checked_add(gram[4])
        .and_then(|value| value.checked_add(gram[8]))
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "essential_gram_trace",
        })?;
    let mut maximum_residual = 0_u128;
    for row in 0..3 {
        for column in 0..3 {
            let mut cubic = 0_i128;
            for inner in 0..3 {
                let left = *gram.get(row * 3 + inner).ok_or(
                    EpipolarError::ArithmeticOverflow {
                        field: "essential_cubic_left_index",
                    },
                )?;
                let right = i128::from(*matrix.get(inner * 3 + column).ok_or(
                    EpipolarError::ArithmeticOverflow {
                        field: "essential_cubic_right_index",
                    },
                )?);
                cubic = cubic
                    .checked_add(left.checked_mul(right).ok_or(
                        EpipolarError::ArithmeticOverflow {
                            field: "essential_cubic_product",
                        },
                    )?)
                    .ok_or(EpipolarError::ArithmeticOverflow {
                        field: "essential_cubic_sum",
                    })?;
            }
            let coefficient = i128::from(*matrix.get(row * 3 + column).ok_or(
                EpipolarError::ArithmeticOverflow {
                    field: "essential_constraint_matrix_index",
                },
            )?);
            let residual = cubic
                .checked_mul(2)
                .and_then(|value| {
                    trace
                        .checked_mul(coefficient)
                        .and_then(|term| value.checked_sub(term))
                })
                .ok_or(EpipolarError::ArithmeticOverflow {
                    field: "essential_constraint_residual",
                })?
                .unsigned_abs();
            maximum_residual = maximum_residual.max(residual);
        }
    }
    let maximum = u128::from(maximum);
    let denominator = maximum
        .checked_mul(maximum)
        .and_then(|value| value.checked_mul(maximum))
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "essential_constraint_normalizer",
        })?;
    let numerator = maximum_residual
        .checked_mul(1_000_000)
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "essential_constraint_residual_ppm",
        })?;
    let value = divide_u128_ceil(
        numerator,
        denominator,
        "essential_constraint_residual_ppm",
    )?;
    u64::try_from(value).map_err(|_| EpipolarError::ArithmeticOverflow {
        field: "essential_constraint_residual_ppm",
    })
}

#[cfg(test)]
mod essential_constraint_tests {
    use super::{
        EpipolarError, EssentialCandidate, MAX_ESSENTIAL_CONSTRAINT_RESIDUAL_PPM,
        essential_constraint_residual_ppm, validate_essential_candidates,
    };
    use crate::CandidateSource;

    #[test]
    fn true_essential_matrix_satisfies_cubic_constraint() {
        let matrix = [0, 0, 0, 0, 0, -1, 0, 1, 0];
        assert_eq!(essential_constraint_residual_ppm(&matrix), Ok(0));
    }

    #[test]
    fn arbitrary_rank_two_matrix_is_not_accepted_as_essential() {
        let candidate = EssentialCandidate {
            candidate_id: 7,
            source: CandidateSource::ExternalOracle,
            matrix: [1, 0, 0, 0, 2, 0, 0, 0, 0],
        };
        assert!(matches!(
            validate_essential_candidates(&[candidate]),
            Err(EpipolarError::EssentialConstraintExceeded {
                candidate_id: 7,
                residual_ppm,
                maximum_ppm: MAX_ESSENTIAL_CONSTRAINT_RESIDUAL_PPM,
            }) if residual_ppm > maximum_ppm
        ));
    }
}
