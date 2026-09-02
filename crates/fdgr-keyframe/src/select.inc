/// Selects a deterministic marginal-coverage and diversity-aware keyframe set.
///
/// Hard quality gates are applied first. Remaining candidates are greedily ranked by exact
/// marginal visibility, aggregate quality, new view sectors, and new baseline bins. Stable sample
/// and frame identities break all ties. Temporal-spacing conflicts, redundant evidence, and
/// capacity exhaustion remain explicit rejection records.
///
/// # Errors
///
/// Returns a stable basis, policy, candidate, bound, or arithmetic error.
pub fn select_keyframes(
    basis: KeyframeBasis,
    policy: KeyframePolicy,
    candidates: Vec<KeyframeCandidate>,
) -> Result<KeyframeSelection, KeyframeError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    let candidates = canonicalize_candidates(candidates)?;
    let mut eligible_indices = Vec::new();
    let mut rejected = Vec::new();

    for (index, candidate) in candidates.iter().enumerate() {
        let quality = quality_score(candidate);
        if let Some(reason) = hard_rejection(candidate, policy) {
            rejected.push(RejectedKeyframe {
                sample_index: candidate.sample_index,
                frame_digest: candidate.frame_digest.clone(),
                reason,
                quality_score_ppm: quality,
                marginal_coverage_cells: u32::try_from(candidate.coverage_cells.len()).map_err(
                    |_| KeyframeError::ArithmeticOverflow {
                        field: "candidate_coverage_cells",
                    },
                )?,
                conflicts_with_sample_index: None,
            });
        } else {
            eligible_indices.push(index);
        }
    }

    let mut selected = Vec::new();
    let mut covered = BTreeSet::new();
    let mut view_sectors = BTreeSet::new();
    let mut baseline_bins = BTreeSet::new();
    let selection_limit = policy.max_selected as usize;

    while selected.len() < selection_limit {
        let mut best: Option<CandidateEvaluation> = None;
        for candidate_index in &eligible_indices {
            let candidate = candidates.get(*candidate_index).ok_or(
                KeyframeError::ArithmeticOverflow {
                    field: "candidate_index",
                },
            )?;
            if spacing_conflict(
                candidate,
                &selected,
                policy.min_presentation_spacing_ticks,
            )?
            .is_some()
            {
                continue;
            }
            let evaluation = evaluate_candidate(
                *candidate_index,
                candidate,
                &covered,
                &view_sectors,
                &baseline_bins,
                policy,
            )?;
            if !is_useful(&evaluation) {
                continue;
            }
            let replace = best
                .as_ref()
                .is_none_or(|current| better_evaluation(&evaluation, current));
            if replace {
                best = Some(evaluation);
            }
        }
        let Some(best) = best else {
            break;
        };
        let candidate = candidates.get(best.candidate_index).ok_or(
            KeyframeError::ArithmeticOverflow {
                field: "selected_candidate_index",
            },
        )?;
        for cell in &candidate.coverage_cells {
            covered.insert(*cell);
        }
        view_sectors.insert(candidate.view_sector);
        baseline_bins.insert(candidate.baseline_bin);
        let rank = u32::try_from(selected.len().saturating_add(1)).map_err(|_| {
            KeyframeError::ArithmeticOverflow {
                field: "selection_rank",
            }
        })?;
        selected.push(SelectedKeyframe {
            rank,
            sample_index: candidate.sample_index,
            frame_digest: candidate.frame_digest.clone(),
            presentation_tick: candidate.presentation_tick,
            quality_score_ppm: best.quality_score_ppm,
            marginal_coverage_cells: best.marginal_coverage_cells,
            new_view_sector: best.new_view_sector,
            new_baseline_bin: best.new_baseline_bin,
            total_score: best.total_score,
        });
        eligible_indices.retain(|index| *index != best.candidate_index);
    }

    for candidate_index in eligible_indices {
        let candidate = candidates.get(candidate_index).ok_or(
            KeyframeError::ArithmeticOverflow {
                field: "rejected_candidate_index",
            },
        )?;
        let conflict = spacing_conflict(
            candidate,
            &selected,
            policy.min_presentation_spacing_ticks,
        )?;
        let evaluation = evaluate_candidate(
            candidate_index,
            candidate,
            &covered,
            &view_sectors,
            &baseline_bins,
            policy,
        )?;
        let reason = if conflict.is_some() {
            KeyframeRejectionReason::TemporalSpacing
        } else if !is_useful(&evaluation) {
            KeyframeRejectionReason::RedundantEvidence
        } else {
            KeyframeRejectionReason::Capacity
        };
        rejected.push(RejectedKeyframe {
            sample_index: candidate.sample_index,
            frame_digest: candidate.frame_digest.clone(),
            reason,
            quality_score_ppm: evaluation.quality_score_ppm,
            marginal_coverage_cells: evaluation.marginal_coverage_cells,
            conflicts_with_sample_index: conflict,
        });
    }

    rejected.sort_by(|left, right| {
        left.sample_index
            .cmp(&right.sample_index)
            .then(left.frame_digest.cmp(&right.frame_digest))
    });
    Ok(KeyframeSelection {
        basis,
        policy,
        candidates,
        selected,
        rejected,
        covered_cells: covered.into_iter().collect(),
        selected_view_sectors: view_sectors.into_iter().collect(),
        selected_baseline_bins: baseline_bins.into_iter().collect(),
    })
}
