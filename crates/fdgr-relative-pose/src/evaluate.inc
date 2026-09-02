/// Verifies a bounded set of physical relative-pose candidates.
///
/// # Errors
///
/// Returns a stable basis, observation, pose, budget, triangulation, or arithmetic error.
pub fn verify_relative_pose_candidates(
    basis: RelativePoseBasis,
    policy: RelativePosePolicy,
    observations: Vec<NormalizedCorrespondence>,
    candidates: Vec<RelativePoseCandidate>,
) -> Result<RelativePoseVerification, RelativePoseError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    let observations = canonicalize_observations(observations, &basis)?;
    let candidates = canonicalize_candidates(candidates)?;
    let required_evaluations = usize_to_u64(observations.len())?
        .checked_mul(usize_to_u64(candidates.len())?)
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "evaluation_count",
        })?;
    let maximum = policy.max_evaluations.min(MAX_POSE_EVALUATIONS);
    if required_evaluations > maximum {
        return Err(RelativePoseError::EvaluationBudgetExceeded {
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

    let (decision, best_candidate_id, runner_up_candidate_id, admitted_candidate_id, supported_margin) =
        if let Some(best) = passing.first().copied() {
            let runner_up = passing.get(1).copied();
            let margin = runner_up.map_or(best.supported_match_count, |runner| {
                best.supported_match_count
                    .saturating_sub(runner.supported_match_count)
            });
            if runner_up.is_some() && margin < policy.min_supported_margin {
                (
                    RelativePoseDecision::Ambiguous,
                    Some(best.candidate_id),
                    runner_up.map(|value| value.candidate_id),
                    None,
                    margin,
                )
            } else {
                (
                    RelativePoseDecision::RelativePoseSupportedHypothesis,
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
                runner_up.map_or(leader.supported_match_count, |runner| {
                    leader
                        .supported_match_count
                        .saturating_sub(runner.supported_match_count)
                })
            });
            (
                RelativePoseDecision::Rejected,
                best.map(|value| value.candidate_id),
                runner_up.map(|value| value.candidate_id),
                None,
                margin,
            )
        };

    Ok(RelativePoseVerification {
        basis,
        policy,
        observations,
        candidates,
        evaluations,
        decision,
        best_candidate_id,
        runner_up_candidate_id,
        admitted_candidate_id,
        supported_margin,
        evaluation_count: required_evaluations,
    })
}

fn evaluate_candidate(
    candidate: &RelativePoseCandidate,
    policy: RelativePosePolicy,
    observations: &[NormalizedCorrespondence],
) -> Result<RelativePoseEvaluation, RelativePoseError> {
    let mut evidence = Vec::with_capacity(observations.len());
    let mut supported_match_ids = Vec::new();
    let mut rejected_match_ids = Vec::new();
    let mut supported_parallax = Vec::new();
    let mut left_bins = BTreeSet::new();
    let mut right_bins = BTreeSet::new();
    for observation in observations {
        let item = triangulate_signs(candidate, observation, policy.min_parallax_sine_ppm)?;
        if item.disposition == TriangulationDisposition::Supported {
            supported_match_ids.push(observation.match_id);
            supported_parallax.push(item.parallax_sine_ppm);
            left_bins.insert(observation.left_spatial_bin);
            right_bins.insert(observation.right_spatial_bin);
        } else {
            rejected_match_ids.push(observation.match_id);
        }
        evidence.push(item);
    }
    let supported_match_count = u32::try_from(supported_match_ids.len()).map_err(|_| {
        RelativePoseError::ArithmeticOverflow {
            field: "supported_match_count",
        }
    })?;
    let rejected_match_count = u32::try_from(rejected_match_ids.len()).map_err(|_| {
        RelativePoseError::ArithmeticOverflow {
            field: "rejected_match_count",
        }
    })?;
    let supported_ratio_ppm = supported_ratio_ppm(supported_match_ids.len(), observations.len())?;
    let left_spatial_bin_count = u16::try_from(left_bins.len()).map_err(|_| {
        RelativePoseError::ArithmeticOverflow {
            field: "left_spatial_bin_count",
        }
    })?;
    let right_spatial_bin_count = u16::try_from(right_bins.len()).map_err(|_| {
        RelativePoseError::ArithmeticOverflow {
            field: "right_spatial_bin_count",
        }
    })?;
    let minimum_supported_parallax_sine_ppm =
        supported_parallax.iter().copied().min().unwrap_or(0);
    let median_supported_parallax_sine_ppm = median_u32(&mut supported_parallax);
    let passes_admission_gates = supported_match_count >= policy.min_supported_matches
        && supported_ratio_ppm >= policy.min_supported_ratio_ppm
        && left_spatial_bin_count >= policy.min_spatial_bins_per_image
        && right_spatial_bin_count >= policy.min_spatial_bins_per_image;
    Ok(RelativePoseEvaluation {
        candidate_id: candidate.candidate_id,
        supported_match_count,
        rejected_match_count,
        supported_ratio_ppm,
        left_spatial_bin_count,
        right_spatial_bin_count,
        minimum_supported_parallax_sine_ppm,
        median_supported_parallax_sine_ppm,
        passes_admission_gates,
        supported_match_ids,
        rejected_match_ids,
        evidence,
    })
}

fn compare_evaluations(
    left: &RelativePoseEvaluation,
    right: &RelativePoseEvaluation,
) -> Ordering {
    right
        .supported_match_count
        .cmp(&left.supported_match_count)
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
        .then(right.supported_ratio_ppm.cmp(&left.supported_ratio_ppm))
        .then(
            right
                .minimum_supported_parallax_sine_ppm
                .cmp(&left.minimum_supported_parallax_sine_ppm),
        )
        .then(
            right
                .median_supported_parallax_sine_ppm
                .cmp(&left.median_supported_parallax_sine_ppm),
        )
        .then(left.candidate_id.cmp(&right.candidate_id))
}
