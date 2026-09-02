/// Verifies a bounded candidate set against exact calibrated correspondences.
///
/// # Errors
///
/// Returns a stable basis, observation, candidate, budget, matrix, residual, or arithmetic error.
pub fn verify_epipolar_candidates(
    basis: EpipolarBasis,
    policy: EpipolarPolicy,
    observations: Vec<NormalizedCorrespondence>,
    candidates: Vec<EssentialCandidate>,
) -> Result<EpipolarVerification, EpipolarError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    let observations = canonicalize_observations(observations, &basis)?;
    let candidates = canonicalize_candidates(candidates)?;
    validate_essential_candidates(&candidates)?;
    let required_evaluations = usize_to_u64(observations.len())?
        .checked_mul(usize_to_u64(candidates.len())?)
        .ok_or(EpipolarError::ArithmeticOverflow {
            field: "evaluation_count",
        })?;
    let maximum = policy.max_evaluations.min(MAX_EPIPOLAR_EVALUATIONS);
    if required_evaluations > maximum {
        return Err(EpipolarError::EvaluationBudgetExceeded {
            required: required_evaluations,
            maximum,
        });
    }

    let mut evaluations = Vec::with_capacity(candidates.len());
    for candidate in &candidates {
        evaluations.push(evaluate_candidate(candidate, policy, &observations)?);
    }
    evaluations.sort_by(|left, right| left.candidate_id.cmp(&right.candidate_id));

    let mut passing = evaluations
        .iter()
        .filter(|evaluation| evaluation.passes_admission_gates)
        .collect::<Vec<_>>();
    passing.sort_by(|left, right| compare_evaluations(left, right));

    let mut all_ranked = evaluations.iter().collect::<Vec<_>>();
    all_ranked.sort_by(|left, right| compare_evaluations(left, right));

    let (decision, best_candidate_id, runner_up_candidate_id, admitted_candidate_id, inlier_margin) =
        if let Some(best) = passing.first().copied() {
            let runner_up = passing.get(1).copied();
            let margin = runner_up.map_or(best.inlier_count, |runner| {
                best.inlier_count.saturating_sub(runner.inlier_count)
            });
            if runner_up.is_some() && margin < policy.min_inlier_margin {
                (
                    VerificationDecision::Ambiguous,
                    Some(best.candidate_id),
                    runner_up.map(|value| value.candidate_id),
                    None,
                    margin,
                )
            } else {
                (
                    VerificationDecision::EpipolarSupportedHypothesis,
                    Some(best.candidate_id),
                    runner_up.map(|value| value.candidate_id),
                    Some(best.candidate_id),
                    margin,
                )
            }
        } else {
            let best = all_ranked.first().copied();
            let runner_up = all_ranked.get(1).copied();
            let margin = best.map_or(0, |leader| {
                runner_up.map_or(leader.inlier_count, |runner| {
                    leader.inlier_count.saturating_sub(runner.inlier_count)
                })
            });
            (
                VerificationDecision::Rejected,
                best.map(|value| value.candidate_id),
                runner_up.map(|value| value.candidate_id),
                None,
                margin,
            )
        };

    Ok(EpipolarVerification {
        basis,
        policy,
        observations,
        candidates,
        evaluations,
        decision,
        best_candidate_id,
        runner_up_candidate_id,
        admitted_candidate_id,
        inlier_margin,
        evaluation_count: required_evaluations,
    })
}

fn evaluate_candidate(
    candidate: &EssentialCandidate,
    policy: EpipolarPolicy,
    observations: &[NormalizedCorrespondence],
) -> Result<CandidateEvaluation, EpipolarError> {
    let determinant_residual_ppm = determinant_residual_ppm(&candidate.matrix)?;
    let determinant_gate_passed =
        determinant_residual_ppm <= policy.max_determinant_residual_ppm;
    let mut residuals = Vec::with_capacity(observations.len());
    let mut inlier_match_ids = Vec::new();
    let mut outlier_match_ids = Vec::new();
    let mut inlier_residuals = Vec::new();
    let mut left_bins = BTreeSet::new();
    let mut right_bins = BTreeSet::new();

    for observation in observations {
        let gate_ppm = policy
            .max_residual_ppm
            .checked_add(u64::from(observation.uncertainty_ppm))
            .ok_or(EpipolarError::ArithmeticOverflow {
                field: "uncertainty_aware_residual_gate",
            })?;
        let residual_ppm = symmetric_l1_residual_ppm(&candidate.matrix, observation)?;
        let disposition = match residual_ppm {
            Some(value) if value <= gate_ppm => {
                inlier_match_ids.push(observation.match_id);
                inlier_residuals.push(value);
                left_bins.insert(observation.left_spatial_bin);
                right_bins.insert(observation.right_spatial_bin);
                ResidualDisposition::Inlier
            }
            Some(_) => {
                outlier_match_ids.push(observation.match_id);
                ResidualDisposition::ResidualExceeded
            }
            None => {
                outlier_match_ids.push(observation.match_id);
                ResidualDisposition::UndefinedLine
            }
        };
        residuals.push(EpipolarResidual {
            candidate_id: candidate.candidate_id,
            match_id: observation.match_id,
            residual_ppm,
            gate_ppm,
            disposition,
        });
    }

    let inlier_count = u32::try_from(inlier_match_ids.len()).map_err(|_| {
        EpipolarError::ArithmeticOverflow {
            field: "inlier_count",
        }
    })?;
    let outlier_count = u32::try_from(outlier_match_ids.len()).map_err(|_| {
        EpipolarError::ArithmeticOverflow {
            field: "outlier_count",
        }
    })?;
    let inlier_ratio_ppm = inlier_ratio_ppm(inlier_match_ids.len(), observations.len())?;
    let left_spatial_bin_count = u16::try_from(left_bins.len()).map_err(|_| {
        EpipolarError::ArithmeticOverflow {
            field: "left_spatial_bin_count",
        }
    })?;
    let right_spatial_bin_count = u16::try_from(right_bins.len()).map_err(|_| {
        EpipolarError::ArithmeticOverflow {
            field: "right_spatial_bin_count",
        }
    })?;
    let maximum_inlier_residual_ppm = inlier_residuals.iter().copied().max().unwrap_or(0);
    let median_inlier_residual_ppm = median_u64(&mut inlier_residuals);
    let passes_admission_gates = determinant_gate_passed
        && inlier_count >= policy.min_inliers
        && inlier_ratio_ppm >= policy.min_inlier_ratio_ppm
        && left_spatial_bin_count >= policy.min_spatial_bins_per_image
        && right_spatial_bin_count >= policy.min_spatial_bins_per_image;

    Ok(CandidateEvaluation {
        candidate_id: candidate.candidate_id,
        determinant_residual_ppm,
        determinant_gate_passed,
        inlier_count,
        outlier_count,
        inlier_ratio_ppm,
        left_spatial_bin_count,
        right_spatial_bin_count,
        median_inlier_residual_ppm,
        maximum_inlier_residual_ppm,
        passes_admission_gates,
        inlier_match_ids,
        outlier_match_ids,
        residuals,
    })
}

fn compare_evaluations(
    left: &CandidateEvaluation,
    right: &CandidateEvaluation,
) -> Ordering {
    right
        .inlier_count
        .cmp(&left.inlier_count)
        .then(
            right
                .left_spatial_bin_count
                .min(right.right_spatial_bin_count)
                .cmp(
                    &left
                        .left_spatial_bin_count
                        .min(left.right_spatial_bin_count),
                ),
        )
        .then(right.inlier_ratio_ppm.cmp(&left.inlier_ratio_ppm))
        .then(
            left.median_inlier_residual_ppm
                .cmp(&right.median_inlier_residual_ppm),
        )
        .then(
            left.maximum_inlier_residual_ppm
                .cmp(&right.maximum_inlier_residual_ppm),
        )
        .then(
            left.determinant_residual_ppm
                .cmp(&right.determinant_residual_ppm),
        )
        .then(left.candidate_id.cmp(&right.candidate_id))
}
