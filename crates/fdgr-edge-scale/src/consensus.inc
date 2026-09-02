fn collapse_correlation_group(
    correlation_group_id: u64,
    witness_ids: &[u64],
    witness_by_id: &BTreeMap<u64, &EdgeRatioWitness>,
    policy: EdgeScalePolicy,
    assessments: &mut BTreeMap<u64, WitnessAssessment>,
) -> Result<Option<CleanGroup>, EdgeScaleError> {
    let mut group = witness_ids
        .iter()
        .map(|witness_id| {
            witness_by_id
                .get(witness_id)
                .copied()
                .ok_or(EdgeScaleError::InternalLookup {
                    field: "group_witness",
                })
        })
        .collect::<Result<Vec<_>, _>>()?;
    group.sort_by(|left, right| {
        left.uncertainty_ppm
            .cmp(&right.uncertainty_ppm)
            .then(right.source.rank().cmp(&left.source.rank()))
            .then(right.support_count.cmp(&left.support_count))
            .then(left.witness_id.cmp(&right.witness_id))
    });
    let representative = group
        .first()
        .copied()
        .ok_or(EdgeScaleError::InternalLookup {
            field: "group_representative",
        })?;
    let mut ratio_order = group.clone();
    ratio_order.sort_by(|left, right| {
        compare_fraction(
            left.ratio_numerator,
            left.ratio_denominator,
            right.ratio_numerator,
            right.ratio_denominator,
        )
        .then(left.witness_id.cmp(&right.witness_id))
    });
    let lowest = ratio_order
        .first()
        .copied()
        .ok_or(EdgeScaleError::InternalLookup {
            field: "group_lowest_ratio",
        })?;
    let highest = ratio_order
        .last()
        .copied()
        .ok_or(EdgeScaleError::InternalLookup {
            field: "group_highest_ratio",
        })?;
    let internal_residual_ppm = fraction_residual_ppm(
        lowest.ratio_numerator,
        lowest.ratio_denominator,
        highest.ratio_numerator,
        highest.ratio_denominator,
    )?;
    let mut residuals = BTreeMap::new();
    for witness in &group {
        residuals.insert(
            witness.witness_id,
            fraction_residual_ppm(
                representative.ratio_numerator,
                representative.ratio_denominator,
                witness.ratio_numerator,
                witness.ratio_denominator,
            )?,
        );
    }
    if internal_residual_ppm > policy.max_within_group_residual_ppm {
        for witness in group {
            assessments.insert(
                witness.witness_id,
                WitnessAssessment {
                    witness_id: witness.witness_id,
                    relation_id: None,
                    disposition: WitnessDisposition::CorrelatedConflict,
                    residual_ppm: residuals.get(&witness.witness_id).copied(),
                },
            );
        }
        return Ok(None);
    }
    Ok(Some(CleanGroup {
        correlation_group_id,
        representative_witness_id: representative.witness_id,
        ratio_numerator: representative.ratio_numerator,
        ratio_denominator: representative.ratio_denominator,
        witness_ids: group.iter().map(|witness| witness.witness_id).collect(),
        uncertainty_ppm: group
            .iter()
            .map(|witness| witness.uncertainty_ppm)
            .max()
            .unwrap_or(0),
        evidence_rank: group
            .iter()
            .map(|witness| witness.source.rank())
            .max()
            .unwrap_or(0),
        support_count: group
            .iter()
            .map(|witness| u64::from(witness.support_count))
            .max()
            .unwrap_or(0),
        internal_residual_ppm,
    }))
}

fn score_consensus_candidates(
    groups: &[CleanGroup],
    policy: EdgeScalePolicy,
) -> Result<Vec<ConsensusCandidate>, EdgeScaleError> {
    let mut candidates = Vec::with_capacity(groups.len());
    for (candidate_index, candidate) in groups.iter().enumerate() {
        let mut inlier_indices = Vec::new();
        let mut rank_sum = 0_u64;
        let mut support_sum = 0_u64;
        let mut residual_sum = 0_u64;
        for (group_index, group) in groups.iter().enumerate() {
            let residual = fraction_residual_ppm(
                candidate.ratio_numerator,
                candidate.ratio_denominator,
                group.ratio_numerator,
                group.ratio_denominator,
            )?;
            if residual <= policy.max_consensus_residual_ppm {
                inlier_indices.push(group_index);
                rank_sum = rank_sum.checked_add(group.evidence_rank).ok_or(
                    EdgeScaleError::ArithmeticOverflow {
                        field: "consensus_rank_sum",
                    },
                )?;
                support_sum = support_sum.checked_add(group.support_count).ok_or(
                    EdgeScaleError::ArithmeticOverflow {
                        field: "consensus_support_sum",
                    },
                )?;
                residual_sum = residual_sum.checked_add(residual).ok_or(
                    EdgeScaleError::ArithmeticOverflow {
                        field: "consensus_residual_sum",
                    },
                )?;
            }
        }
        candidates.push(ConsensusCandidate {
            group_index: candidate_index,
            inlier_indices,
            rank_sum,
            support_sum,
            residual_sum,
        });
    }
    candidates.sort_by(|left, right| compare_consensus(left, right, groups));
    Ok(candidates)
}

fn compare_consensus(
    left: &ConsensusCandidate,
    right: &ConsensusCandidate,
    groups: &[CleanGroup],
) -> Ordering {
    right
        .inlier_indices
        .len()
        .cmp(&left.inlier_indices.len())
        .then(right.rank_sum.cmp(&left.rank_sum))
        .then(right.support_sum.cmp(&left.support_sum))
        .then(left.residual_sum.cmp(&right.residual_sum))
        .then(
            groups[left.group_index]
                .uncertainty_ppm
                .cmp(&groups[right.group_index].uncertainty_ppm),
        )
        .then(compare_fraction(
            groups[left.group_index].ratio_numerator,
            groups[left.group_index].ratio_denominator,
            groups[right.group_index].ratio_numerator,
            groups[right.group_index].ratio_denominator,
        ))
        .then(
            groups[left.group_index]
                .representative_witness_id
                .cmp(&groups[right.group_index].representative_witness_id),
        )
}

fn consensus_score_equal(left: &ConsensusCandidate, right: &ConsensusCandidate) -> bool {
    left.inlier_indices.len() == right.inlier_indices.len()
        && left.rank_sum == right.rank_sum
        && left.support_sum == right.support_sum
        && left.residual_sum == right.residual_sum
}
