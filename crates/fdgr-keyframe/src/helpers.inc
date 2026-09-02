fn validate_basis(basis: &KeyframeBasis) -> Result<(), KeyframeError> {
    for (field, digest) in [
        ("timeline_digest", &basis.timeline_digest),
        (
            "decoded_frame_generation_digest",
            &basis.decoded_frame_generation_digest,
        ),
        ("calibration_digest", &basis.calibration_digest),
        ("candidate_basis_digest", &basis.candidate_basis_digest),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    if basis.selection_generation == 0 {
        return Err(KeyframeError::ZeroValue {
            field: "selection_generation",
        });
    }
    Ok(())
}

fn validate_policy(policy: KeyframePolicy) -> Result<(), KeyframeError> {
    if policy.max_selected == 0 {
        return Err(KeyframeError::ZeroValue {
            field: "max_selected",
        });
    }
    if policy.max_selected > MAX_SELECTED_KEYFRAMES {
        return Err(KeyframeError::BoundExceeded {
            field: "max_selected",
            actual: u128::from(policy.max_selected),
            maximum: u128::from(MAX_SELECTED_KEYFRAMES),
        });
    }
    for (field, value) in [
        ("min_sharpness_ppm", policy.min_sharpness_ppm),
        ("min_texture_ppm", policy.min_texture_ppm),
        ("max_dark_clipped_ppm", policy.max_dark_clipped_ppm),
        ("max_bright_clipped_ppm", policy.max_bright_clipped_ppm),
        (
            "max_dynamic_content_ppm",
            policy.max_dynamic_content_ppm,
        ),
        ("min_overlap_ppm", policy.min_overlap_ppm),
    ] {
        validate_ppm(field, value)?;
    }
    if u128::from(policy.min_presentation_spacing_ticks)
        > MAX_ABS_PRESENTATION_TICK.unsigned_abs()
    {
        return Err(KeyframeError::BoundExceeded {
            field: "min_presentation_spacing_ticks",
            actual: u128::from(policy.min_presentation_spacing_ticks),
            maximum: MAX_ABS_PRESENTATION_TICK.unsigned_abs(),
        });
    }
    for (field, value) in [
        ("quality_weight", policy.quality_weight),
        ("coverage_weight", policy.coverage_weight),
        ("view_diversity_weight", policy.view_diversity_weight),
        (
            "baseline_diversity_weight",
            policy.baseline_diversity_weight,
        ),
    ] {
        if value > MAX_SELECTION_WEIGHT {
            return Err(KeyframeError::BoundExceeded {
                field,
                actual: u128::from(value),
                maximum: u128::from(MAX_SELECTION_WEIGHT),
            });
        }
    }
    if policy.quality_weight == 0
        && policy.coverage_weight == 0
        && policy.view_diversity_weight == 0
        && policy.baseline_diversity_weight == 0
    {
        return Err(KeyframeError::ZeroSelectionWeights);
    }
    Ok(())
}

fn canonicalize_candidates(
    mut candidates: Vec<KeyframeCandidate>,
) -> Result<Vec<KeyframeCandidate>, KeyframeError> {
    if candidates.is_empty() {
        return Err(KeyframeError::EmptyCandidates);
    }
    if candidates.len() > MAX_KEYFRAME_CANDIDATES {
        return Err(KeyframeError::BoundExceeded {
            field: "candidate_count",
            actual: candidates.len() as u128,
            maximum: MAX_KEYFRAME_CANDIDATES as u128,
        });
    }
    candidates.sort_by(|left, right| {
        left.sample_index
            .cmp(&right.sample_index)
            .then(left.frame_digest.cmp(&right.frame_digest))
    });
    let mut sample_indices = BTreeSet::new();
    let mut frame_digests = BTreeSet::new();
    let mut total_references = 0_usize;
    for candidate in &candidates {
        if !sample_indices.insert(candidate.sample_index) {
            return Err(KeyframeError::DuplicateSampleIndex {
                sample_index: candidate.sample_index,
            });
        }
        if !frame_digests.insert(candidate.frame_digest.clone()) {
            return Err(KeyframeError::DuplicateFrameDigest {
                frame_digest: candidate.frame_digest.clone(),
            });
        }
        validate_candidate(candidate)?;
        total_references = total_references
            .checked_add(candidate.coverage_cells.len())
            .ok_or(KeyframeError::ArithmeticOverflow {
                field: "total_coverage_references",
            })?;
        if total_references > MAX_TOTAL_COVERAGE_REFERENCES {
            return Err(KeyframeError::CoverageReferenceLimitExceeded {
                actual: total_references,
                maximum: MAX_TOTAL_COVERAGE_REFERENCES,
            });
        }
    }
    Ok(candidates)
}

fn validate_candidate(candidate: &KeyframeCandidate) -> Result<(), KeyframeError> {
    reject_zero_digest("frame_digest", &candidate.frame_digest)?;
    if candidate.presentation_tick.unsigned_abs() > MAX_ABS_PRESENTATION_TICK.unsigned_abs() {
        return Err(KeyframeError::BoundExceeded {
            field: "presentation_tick",
            actual: candidate.presentation_tick.unsigned_abs(),
            maximum: MAX_ABS_PRESENTATION_TICK.unsigned_abs(),
        });
    }
    for (field, value) in [
        ("sharpness_ppm", candidate.sharpness_ppm),
        ("texture_ppm", candidate.texture_ppm),
        ("dark_clipped_ppm", candidate.dark_clipped_ppm),
        ("bright_clipped_ppm", candidate.bright_clipped_ppm),
        ("dynamic_content_ppm", candidate.dynamic_content_ppm),
        ("overlap_ppm", candidate.overlap_ppm),
    ] {
        validate_ppm(field, value)?;
    }
    if candidate.view_sector == 0 {
        return Err(KeyframeError::ZeroValue {
            field: "view_sector",
        });
    }
    if candidate.baseline_bin == 0 {
        return Err(KeyframeError::ZeroValue {
            field: "baseline_bin",
        });
    }
    if candidate.coverage_cells.len() > MAX_COVERAGE_CELLS_PER_CANDIDATE {
        return Err(KeyframeError::BoundExceeded {
            field: "coverage_cells",
            actual: candidate.coverage_cells.len() as u128,
            maximum: MAX_COVERAGE_CELLS_PER_CANDIDATE as u128,
        });
    }
    if candidate.coverage_cells.iter().any(|value| *value == 0)
        || candidate
            .coverage_cells
            .windows(2)
            .any(|pair| matches!(pair, [left, right] if left >= right))
    {
        return Err(KeyframeError::NonCanonicalCoverageCells {
            sample_index: candidate.sample_index,
        });
    }
    Ok(())
}

fn validate_ppm(field: &'static str, value: u32) -> Result<(), KeyframeError> {
    if value > PPM_SCALE {
        return Err(KeyframeError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(PPM_SCALE),
        });
    }
    Ok(())
}

fn hard_rejection(
    candidate: &KeyframeCandidate,
    policy: KeyframePolicy,
) -> Option<KeyframeRejectionReason> {
    if candidate.sharpness_ppm < policy.min_sharpness_ppm {
        Some(KeyframeRejectionReason::SharpnessBelowMinimum)
    } else if candidate.texture_ppm < policy.min_texture_ppm {
        Some(KeyframeRejectionReason::TextureBelowMinimum)
    } else if candidate.dark_clipped_ppm > policy.max_dark_clipped_ppm {
        Some(KeyframeRejectionReason::DarkClippingExceeded)
    } else if candidate.bright_clipped_ppm > policy.max_bright_clipped_ppm {
        Some(KeyframeRejectionReason::BrightClippingExceeded)
    } else if candidate.dynamic_content_ppm > policy.max_dynamic_content_ppm {
        Some(KeyframeRejectionReason::DynamicContentExceeded)
    } else if candidate.overlap_ppm < policy.min_overlap_ppm {
        Some(KeyframeRejectionReason::OverlapBelowMinimum)
    } else if candidate.coverage_cells.is_empty() {
        Some(KeyframeRejectionReason::EmptyCoverage)
    } else {
        None
    }
}

fn quality_score(candidate: &KeyframeCandidate) -> u32 {
    let retained = u64::from(candidate.sharpness_ppm)
        + u64::from(candidate.texture_ppm)
        + u64::from(PPM_SCALE.saturating_sub(candidate.dark_clipped_ppm))
        + u64::from(PPM_SCALE.saturating_sub(candidate.bright_clipped_ppm))
        + u64::from(PPM_SCALE.saturating_sub(candidate.dynamic_content_ppm))
        + u64::from(candidate.overlap_ppm);
    match u32::try_from(retained / 6) {
        Ok(value) => value,
        Err(_) => PPM_SCALE,
    }
}

fn marginal_coverage(candidate: &KeyframeCandidate, covered: &BTreeSet<u64>) -> usize {
    candidate
        .coverage_cells
        .iter()
        .filter(|cell| !covered.contains(cell))
        .count()
}

fn tick_distance(left: i128, right: i128) -> Result<u128, KeyframeError> {
    let difference = if left >= right {
        left.checked_sub(right)
    } else {
        right.checked_sub(left)
    }
    .ok_or(KeyframeError::ArithmeticOverflow {
        field: "presentation_tick_distance",
    })?;
    Ok(difference.unsigned_abs())
}

fn spacing_conflict(
    candidate: &KeyframeCandidate,
    selected: &[SelectedKeyframe],
    minimum_ticks: u64,
) -> Result<Option<u64>, KeyframeError> {
    if minimum_ticks == 0 {
        return Ok(None);
    }
    let threshold = u128::from(minimum_ticks);
    let mut best: Option<(u128, u64)> = None;
    for selected_frame in selected {
        let distance = tick_distance(candidate.presentation_tick, selected_frame.presentation_tick)?;
        if distance < threshold {
            let replacement = match best {
                None => true,
                Some((best_distance, best_sample)) => {
                    distance < best_distance
                        || (distance == best_distance
                            && selected_frame.sample_index < best_sample)
                }
            };
            if replacement {
                best = Some((distance, selected_frame.sample_index));
            }
        }
    }
    Ok(best.map(|(_, sample_index)| sample_index))
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct CandidateEvaluation {
    candidate_index: usize,
    sample_index: u64,
    frame_digest: EvidenceDigest,
    quality_score_ppm: u32,
    marginal_coverage_cells: u32,
    new_view_sector: bool,
    new_baseline_bin: bool,
    total_score: u128,
}

fn evaluate_candidate(
    candidate_index: usize,
    candidate: &KeyframeCandidate,
    covered: &BTreeSet<u64>,
    view_sectors: &BTreeSet<u32>,
    baseline_bins: &BTreeSet<u32>,
    policy: KeyframePolicy,
) -> Result<CandidateEvaluation, KeyframeError> {
    let marginal = marginal_coverage(candidate, covered);
    let marginal_u32 = u32::try_from(marginal).map_err(|_| KeyframeError::ArithmeticOverflow {
        field: "marginal_coverage_cells",
    })?;
    let quality = quality_score(candidate);
    let new_view_sector = !view_sectors.contains(&candidate.view_sector);
    let new_baseline_bin = !baseline_bins.contains(&candidate.baseline_bin);
    let quality_component = u128::from(quality)
        .checked_mul(u128::from(policy.quality_weight))
        .ok_or(KeyframeError::ArithmeticOverflow {
            field: "quality_score_component",
        })?;
    let coverage_component = (marginal as u128)
        .checked_mul(u128::from(PPM_SCALE))
        .and_then(|value| value.checked_mul(u128::from(policy.coverage_weight)))
        .ok_or(KeyframeError::ArithmeticOverflow {
            field: "coverage_score_component",
        })?;
    let view_component = if new_view_sector {
        u128::from(PPM_SCALE)
            .checked_mul(u128::from(policy.view_diversity_weight))
            .ok_or(KeyframeError::ArithmeticOverflow {
                field: "view_score_component",
            })?
    } else {
        0
    };
    let baseline_component = if new_baseline_bin {
        u128::from(PPM_SCALE)
            .checked_mul(u128::from(policy.baseline_diversity_weight))
            .ok_or(KeyframeError::ArithmeticOverflow {
                field: "baseline_score_component",
            })?
    } else {
        0
    };
    let total_score = quality_component
        .checked_add(coverage_component)
        .and_then(|value| value.checked_add(view_component))
        .and_then(|value| value.checked_add(baseline_component))
        .ok_or(KeyframeError::ArithmeticOverflow {
            field: "total_selection_score",
        })?;
    Ok(CandidateEvaluation {
        candidate_index,
        sample_index: candidate.sample_index,
        frame_digest: candidate.frame_digest.clone(),
        quality_score_ppm: quality,
        marginal_coverage_cells: marginal_u32,
        new_view_sector,
        new_baseline_bin,
        total_score,
    })
}

fn better_evaluation(left: &CandidateEvaluation, right: &CandidateEvaluation) -> bool {
    left.total_score > right.total_score
        || (left.total_score == right.total_score
            && (left.marginal_coverage_cells > right.marginal_coverage_cells
                || (left.marginal_coverage_cells == right.marginal_coverage_cells
                    && (left.quality_score_ppm > right.quality_score_ppm
                        || (left.quality_score_ppm == right.quality_score_ppm
                            && (left.sample_index < right.sample_index
                                || (left.sample_index == right.sample_index
                                    && left.frame_digest < right.frame_digest)))))))
}

fn is_useful(evaluation: &CandidateEvaluation) -> bool {
    evaluation.marginal_coverage_cells > 0
        || evaluation.new_view_sector
        || evaluation.new_baseline_bin
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), KeyframeError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(KeyframeError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn encode_i128(encoder: &mut Encoder, value: i128) -> Result<(), KeyframeError> {
    encoder.put_bytes(&value.to_be_bytes())?;
    Ok(())
}

fn encode_u128(encoder: &mut Encoder, value: u128) -> Result<(), KeyframeError> {
    encoder.put_bytes(&value.to_be_bytes())?;
    Ok(())
}

fn usize_to_u64(value: usize) -> Result<u64, KeyframeError> {
    u64::try_from(value).map_err(|_| KeyframeError::ArithmeticOverflow {
        field: "collection_length",
    })
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn json_rendering(error: std::fmt::Error) -> KeyframeError {
    KeyframeError::JsonRendering(error.to_string())
}
