/// Evaluates an exact candidate set against calibrated bearing matches.
///
/// # Errors
///
/// Returns a stable basis, policy, input, budget, geometry, or arithmetic error.
pub fn verify_relative_pose(
    basis: RelativePoseBasis,
    policy: RelativePosePolicy,
    matches: Vec<BearingMatch>,
    candidates: Vec<RelativePoseCandidate>,
) -> Result<RelativePoseVerification, RelativePoseError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    let matches = canonicalize_matches(matches)?;
    let candidates = canonicalize_candidates(candidates)?;
    let required = u64::try_from(matches.len())
        .ok()
        .and_then(|left| {
            u64::try_from(candidates.len())
                .ok()
                .and_then(|right| left.checked_mul(right))
        })
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "candidate_match_evaluations",
        })?;
    if required > policy.max_evaluations {
        return Err(RelativePoseError::EvaluationBudgetExceeded {
            required,
            maximum: policy.max_evaluations,
        });
    }

    let mut evaluations = Vec::with_capacity(candidates.len());
    for candidate in &candidates {
        evaluations.push(evaluate_candidate(candidate, &matches, policy)?);
    }
    let (status, selected_candidate_id, ambiguous_candidate_ids) = resolve_candidate_set(&evaluations);
    Ok(RelativePoseVerification {
        basis,
        policy,
        matches,
        candidates,
        evaluations,
        status,
        selected_candidate_id,
        ambiguous_candidate_ids,
        evaluation_count: required,
    })
}

fn validate_basis(basis: &RelativePoseBasis) -> Result<(), RelativePoseError> {
    for (field, digest) in [
        (
            "correspondence_generation_digest",
            &basis.correspondence_generation_digest,
        ),
        ("calibration_digest", &basis.calibration_digest),
        ("bearing_basis_digest", &basis.bearing_basis_digest),
        ("candidate_basis_digest", &basis.candidate_basis_digest),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_nonzero("generation", basis.generation)?;
    validate_safe_number("left_sample_index", basis.left_sample_index)?;
    validate_safe_number("right_sample_index", basis.right_sample_index)?;
    if basis.left_sample_index >= basis.right_sample_index {
        return Err(RelativePoseError::NonCanonicalFramePair);
    }
    Ok(())
}

fn validate_policy(policy: RelativePosePolicy) -> Result<(), RelativePoseError> {
    for (field, value) in [
        (
            "max_epipolar_residual_nano",
            policy.max_epipolar_residual_nano,
        ),
        (
            "min_epipolar_normal_nano",
            policy.min_epipolar_normal_nano,
        ),
        ("min_parallax_nano", policy.min_parallax_nano),
        (
            "max_median_residual_nano",
            policy.max_median_residual_nano,
        ),
    ] {
        if value > UNIT_NANO.unsigned_abs() {
            return Err(RelativePoseError::BoundExceeded {
                field,
                actual: u128::from(value),
                maximum: u128::from(UNIT_NANO.unsigned_abs()),
            });
        }
    }
    if policy.min_epipolar_normal_nano == 0 {
        return Err(RelativePoseError::ZeroValue {
            field: "min_epipolar_normal_nano",
        });
    }
    if policy.min_inlier_matches == 0 {
        return Err(RelativePoseError::ZeroValue {
            field: "min_inlier_matches",
        });
    }
    if usize::try_from(policy.min_inlier_matches).map_or(true, |value| value > MAX_BEARING_MATCHES) {
        return Err(RelativePoseError::BoundExceeded {
            field: "min_inlier_matches",
            actual: u128::from(policy.min_inlier_matches),
            maximum: MAX_BEARING_MATCHES as u128,
        });
    }
    for (field, value) in [
        ("min_inlier_ratio_ppm", policy.min_inlier_ratio_ppm),
        (
            "min_positive_depth_ratio_ppm",
            policy.min_positive_depth_ratio_ppm,
        ),
    ] {
        if value > PPM_SCALE {
            return Err(RelativePoseError::BoundExceeded {
                field,
                actual: u128::from(value),
                maximum: u128::from(PPM_SCALE),
            });
        }
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

fn canonicalize_matches(
    mut matches: Vec<BearingMatch>,
) -> Result<Vec<BearingMatch>, RelativePoseError> {
    if matches.is_empty() {
        return Err(RelativePoseError::EmptyMatches);
    }
    if matches.len() > MAX_BEARING_MATCHES {
        return Err(RelativePoseError::BoundExceeded {
            field: "bearing_match_count",
            actual: matches.len() as u128,
            maximum: MAX_BEARING_MATCHES as u128,
        });
    }
    matches.sort_by(|left, right| left.match_id.cmp(&right.match_id));
    let mut ids = BTreeSet::new();
    let mut observation_pairs = BTreeSet::new();
    for value in &matches {
        validate_safe_nonzero("match_id", value.match_id)?;
        validate_safe_nonzero("left_observation_id", value.left_observation_id)?;
        validate_safe_nonzero("right_observation_id", value.right_observation_id)?;
        if !ids.insert(value.match_id) {
            return Err(RelativePoseError::DuplicateMatchId {
                match_id: value.match_id,
            });
        }
        if !observation_pairs.insert((value.left_observation_id, value.right_observation_id)) {
            return Err(RelativePoseError::DuplicateObservationPair {
                left_observation_id: value.left_observation_id,
                right_observation_id: value.right_observation_id,
            });
        }
        validate_unit_vector("left_bearing_nano", value.left_bearing_nano)?;
        validate_unit_vector("right_bearing_nano", value.right_bearing_nano)?;
        if value.uncertainty_nano > MAX_BEARING_UNCERTAINTY_NANO {
            return Err(RelativePoseError::BoundExceeded {
                field: "bearing_uncertainty_nano",
                actual: u128::from(value.uncertainty_nano),
                maximum: u128::from(MAX_BEARING_UNCERTAINTY_NANO),
            });
        }
    }
    Ok(matches)
}

fn canonicalize_candidates(
    mut candidates: Vec<RelativePoseCandidate>,
) -> Result<Vec<RelativePoseCandidate>, RelativePoseError> {
    if candidates.is_empty() {
        return Err(RelativePoseError::EmptyCandidates);
    }
    if candidates.len() > MAX_POSE_CANDIDATES {
        return Err(RelativePoseError::BoundExceeded {
            field: "pose_candidate_count",
            actual: candidates.len() as u128,
            maximum: MAX_POSE_CANDIDATES as u128,
        });
    }
    candidates.sort_by(|left, right| left.candidate_id.cmp(&right.candidate_id));
    let mut ids = BTreeSet::new();
    for candidate in &candidates {
        validate_safe_nonzero("candidate_id", candidate.candidate_id)?;
        reject_zero_digest("candidate_evidence_digest", &candidate.evidence_digest)?;
        if !ids.insert(candidate.candidate_id) {
            return Err(RelativePoseError::DuplicateCandidateId {
                candidate_id: candidate.candidate_id,
            });
        }
        validate_rotation(candidate.rotation_left_to_right_nano)?;
        validate_unit_vector(
            "translation_left_origin_in_right_nano",
            candidate.translation_left_origin_in_right_nano,
        )?;
    }
    Ok(candidates)
}

fn evaluate_candidate(
    candidate: &RelativePoseCandidate,
    matches: &[BearingMatch],
    policy: RelativePosePolicy,
) -> Result<PoseCandidateEvaluation, RelativePoseError> {
    let mut assessments = Vec::with_capacity(matches.len());
    for bearing_match in matches {
        assessments.push(assess_match(candidate, bearing_match, policy)?);
    }
    let match_count = u32::try_from(assessments.len()).map_err(|_| {
        RelativePoseError::ArithmeticOverflow {
            field: "match_count",
        }
    })?;
    let inlier_count = u32::try_from(assessments.iter().filter(|value| value.inlier).count())
        .map_err(|_| RelativePoseError::ArithmeticOverflow {
            field: "inlier_count",
        })?;
    let positive_depth_count = u32::try_from(
        assessments
            .iter()
            .filter(|value| value.left_depth_positive && value.right_depth_positive)
            .count(),
    )
    .map_err(|_| RelativePoseError::ArithmeticOverflow {
        field: "positive_depth_count",
    })?;
    let inlier_ratio_ppm = ratio_ppm(inlier_count, match_count)?;
    let positive_depth_ratio_ppm = ratio_ppm(positive_depth_count, match_count)?;
    let inlier_residuals = assessments
        .iter()
        .filter(|value| value.inlier)
        .map(|value| value.epipolar_residual_nano)
        .collect::<Vec<_>>();
    let inlier_parallax = assessments
        .iter()
        .filter(|value| value.inlier)
        .map(|value| value.parallax_nano)
        .collect::<Vec<_>>();
    let median_inlier_residual_nano = median_u64(
        inlier_residuals.clone(),
        UNIT_NANO.unsigned_abs(),
    );
    let maximum_inlier_residual_nano = inlier_residuals
        .into_iter()
        .max()
        .unwrap_or(UNIT_NANO.unsigned_abs());
    let median_inlier_parallax_nano = median_u64(inlier_parallax, 0);
    let accepted = inlier_count >= policy.min_inlier_matches
        && inlier_ratio_ppm >= policy.min_inlier_ratio_ppm
        && positive_depth_ratio_ppm >= policy.min_positive_depth_ratio_ppm
        && median_inlier_residual_nano <= policy.max_median_residual_nano;
    Ok(PoseCandidateEvaluation {
        candidate_id: candidate.candidate_id,
        evidence_digest: candidate.evidence_digest.clone(),
        source: candidate.source,
        match_count,
        inlier_count,
        inlier_ratio_ppm,
        positive_depth_count,
        positive_depth_ratio_ppm,
        median_inlier_residual_nano,
        maximum_inlier_residual_nano,
        median_inlier_parallax_nano,
        accepted,
        assessments,
    })
}

fn assess_match(
    candidate: &RelativePoseCandidate,
    bearing_match: &BearingMatch,
    policy: RelativePosePolicy,
) -> Result<PoseMatchAssessment, RelativePoseError> {
    let rotated_left = rotate_bearing(
        candidate.rotation_left_to_right_nano,
        bearing_match.left_bearing_nano,
    )?;
    let epipolar_normal = cross_scaled(
        candidate.translation_left_origin_in_right_nano,
        rotated_left,
    )?;
    let normal_norm = vector_norm_nano(epipolar_normal)?;
    let dot = checked_dot(
        bearing_match.right_bearing_nano,
        epipolar_normal,
        "epipolar_residual_dot",
    )?
    .unsigned_abs();
    let epipolar_residual_nano = if normal_norm == 0 {
        UNIT_NANO.unsigned_abs()
    } else {
        let denominator = u128::from(normal_norm);
        u64::try_from(
            dot.checked_add(denominator.saturating_sub(1))
                .ok_or(RelativePoseError::ArithmeticOverflow {
                    field: "epipolar_residual_rounding",
                })?
                / denominator,
        )
        .map_err(|_| RelativePoseError::ArithmeticOverflow {
            field: "epipolar_residual_nano",
        })?
    };
    let bearing_dot = checked_dot(
        rotated_left,
        bearing_match.right_bearing_nano,
        "parallax_dot",
    )?;
    let cosine_nano = divide_round_nearest(
        bearing_dot,
        i128::from(UNIT_NANO),
        "parallax_cosine",
    )?
    .unsigned_abs()
    .min(UNIT_NANO.unsigned_abs().into());
    let parallax_nano = UNIT_NANO
        .unsigned_abs()
        .saturating_sub(u64::try_from(cosine_nano).map_err(|_| {
            RelativePoseError::ArithmeticOverflow {
                field: "parallax_nano",
            }
        })?);
    let (left_depth_positive, right_depth_positive) = depth_signs(
        rotated_left,
        bearing_match.right_bearing_nano,
        candidate.translation_left_origin_in_right_nano,
    )?;
    let uncertainty_gate = policy
        .max_epipolar_residual_nano
        .saturating_add(bearing_match.uncertainty_nano)
        .min(UNIT_NANO.unsigned_abs());
    let rejection_reason = if normal_norm < policy.min_epipolar_normal_nano {
        Some(PoseMatchRejectionReason::EpipolarPlaneDegenerate)
    } else if epipolar_residual_nano > uncertainty_gate {
        Some(PoseMatchRejectionReason::EpipolarResidualExceeded)
    } else if parallax_nano < policy.min_parallax_nano {
        Some(PoseMatchRejectionReason::ParallaxInsufficient)
    } else if policy.require_cheirality && !(left_depth_positive && right_depth_positive) {
        Some(PoseMatchRejectionReason::CheiralityFailed)
    } else {
        None
    };
    Ok(PoseMatchAssessment {
        match_id: bearing_match.match_id,
        epipolar_residual_nano,
        parallax_nano,
        left_depth_positive,
        right_depth_positive,
        inlier: rejection_reason.is_none(),
        rejection_reason,
    })
}

fn depth_signs(
    rotated_left: [i64; 3],
    right: [i64; 3],
    translation: [i64; 3],
) -> Result<(bool, bool), RelativePoseError> {
    let a = checked_dot(rotated_left, rotated_left, "depth_a")?;
    let b = checked_dot(rotated_left, right, "depth_b")?;
    let c = checked_dot(right, right, "depth_c")?;
    let left_translation = checked_dot(rotated_left, translation, "depth_left_translation")?;
    let right_translation = checked_dot(right, translation, "depth_right_translation")?;
    let determinant = a
        .checked_mul(c)
        .and_then(|value| b.checked_mul(b).and_then(|square| value.checked_sub(square)))
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "depth_determinant",
        })?;
    if determinant <= 0 {
        return Ok((false, false));
    }
    let left_rhs = left_translation
        .checked_neg()
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "depth_left_rhs",
        })?;
    let left_numerator = c
        .checked_mul(left_rhs)
        .and_then(|value| {
            b.checked_mul(right_translation)
                .and_then(|term| value.checked_add(term))
        })
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "left_depth_numerator",
        })?;
    let right_numerator = b
        .checked_mul(left_rhs)
        .and_then(|value| {
            a.checked_mul(right_translation)
                .and_then(|term| value.checked_add(term))
        })
        .ok_or(RelativePoseError::ArithmeticOverflow {
            field: "right_depth_numerator",
        })?;
    Ok((left_numerator > 0, right_numerator > 0))
}

fn resolve_candidate_set(
    evaluations: &[PoseCandidateEvaluation],
) -> (RelativePoseStatus, Option<u64>, Vec<u64>) {
    let accepted = evaluations
        .iter()
        .filter(|value| value.accepted)
        .collect::<Vec<_>>();
    let Some(mut best) = accepted.first().copied() else {
        return (RelativePoseStatus::NoAcceptedCandidate, None, Vec::new());
    };
    let mut tied = vec![best.candidate_id];
    for candidate in accepted.into_iter().skip(1) {
        match compare_candidate_quality(candidate, best) {
            Ordering::Greater => {
                best = candidate;
                tied.clear();
                tied.push(candidate.candidate_id);
            }
            Ordering::Equal => tied.push(candidate.candidate_id),
            Ordering::Less => {}
        }
    }
    tied.sort_unstable();
    if tied.len() == 1 {
        (
            RelativePoseStatus::GeometricallyVerified,
            Some(best.candidate_id),
            Vec::new(),
        )
    } else {
        (RelativePoseStatus::Ambiguous, None, tied)
    }
}

fn compare_candidate_quality(
    left: &PoseCandidateEvaluation,
    right: &PoseCandidateEvaluation,
) -> Ordering {
    left.inlier_count
        .cmp(&right.inlier_count)
        .then(left.positive_depth_count.cmp(&right.positive_depth_count))
        .then(
            right
                .median_inlier_residual_nano
                .cmp(&left.median_inlier_residual_nano),
        )
        .then(
            right
                .maximum_inlier_residual_nano
                .cmp(&left.maximum_inlier_residual_nano),
        )
        .then(
            left.median_inlier_parallax_nano
                .cmp(&right.median_inlier_parallax_nano),
        )
}

fn validate_safe_nonzero(field: &'static str, value: u64) -> Result<(), RelativePoseError> {
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
