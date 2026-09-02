fn aggregate_relations(
    subjects: &[EdgeScaleSubject],
    witnesses: &[EdgeRatioWitness],
    policy: EdgeScalePolicy,
) -> Result<(Vec<EdgeScaleRelation>, Vec<WitnessAssessment>), EdgeScaleError> {
    let subject_by_id = subjects
        .iter()
        .map(|subject| (subject.edge_id, subject))
        .collect::<BTreeMap<_, _>>();
    let witness_by_id = witnesses
        .iter()
        .map(|witness| (witness.witness_id, witness))
        .collect::<BTreeMap<_, _>>();
    let mut pair_groups = BTreeMap::<(u64, u64), BTreeMap<u64, Vec<u64>>>::new();
    for witness in witnesses {
        pair_groups
            .entry((witness.lower_edge_id, witness.higher_edge_id))
            .or_default()
            .entry(witness.correlation_group_id)
            .or_default()
            .push(witness.witness_id);
    }
    let mut assessments = BTreeMap::<u64, WitnessAssessment>::new();
    let mut relations = Vec::new();
    for ((lower_edge_id, higher_edge_id), groups) in pair_groups {
        let lower_subject = subject_by_id.get(&lower_edge_id).copied().ok_or(
            EdgeScaleError::InternalLookup { field: "lower_scale_subject" },
        )?;
        let higher_subject = subject_by_id.get(&higher_edge_id).copied().ok_or(
            EdgeScaleError::InternalLookup { field: "higher_scale_subject" },
        )?;
        if lower_subject.pose_component_root_node_id != higher_subject.pose_component_root_node_id {
            for witness_ids in groups.values() {
                for witness_id in witness_ids {
                    assessments.insert(
                        *witness_id,
                        WitnessAssessment {
                            witness_id: *witness_id,
                            relation_id: None,
                            disposition: WitnessDisposition::CrossPoseComponent,
                            residual_ppm: None,
                        },
                    );
                }
            }
            continue;
        }
        let mut clean_groups = Vec::new();
        for (correlation_group_id, witness_ids) in groups {
            if let Some(clean) = collapse_correlation_group(
                correlation_group_id,
                &witness_ids,
                &witness_by_id,
                policy,
                &mut assessments,
            )? {
                clean_groups.push(clean);
            }
        }
        clean_groups.sort_by_key(|group| group.correlation_group_id);
        if clean_groups.is_empty() {
            continue;
        }
        let candidates = score_consensus_candidates(&clean_groups, policy)?;
        let Some(best) = candidates.first() else {
            continue;
        };
        let ambiguous = candidates.get(1).is_some_and(|runner| {
            consensus_score_equal(best, runner)
                && fraction_residual_ppm(
                    clean_groups[best.group_index].ratio_numerator,
                    clean_groups[best.group_index].ratio_denominator,
                    clean_groups[runner.group_index].ratio_numerator,
                    clean_groups[runner.group_index].ratio_denominator,
                )
                .is_ok_and(|residual| residual > policy.max_consensus_residual_ppm)
        });
        if ambiguous {
            for group in &clean_groups {
                for witness_id in &group.witness_ids {
                    assessments.insert(
                        *witness_id,
                        WitnessAssessment {
                            witness_id: *witness_id,
                            relation_id: None,
                            disposition: WitnessDisposition::ConsensusAmbiguous,
                            residual_ppm: None,
                        },
                    );
                }
            }
            continue;
        }
        let relation_id = usize_to_u64(relations.len().saturating_add(1))?;
        let selected = &clean_groups[best.group_index];
        let mut supporting_witness_ids = Vec::new();
        let mut supporting_correlation_group_ids = Vec::new();
        let mut rejected_witness_ids = Vec::new();
        let mut uncertainty_ppm = 0_u32;
        let mut evidence_rank = 0_u64;
        let mut independent_support_count = 0_u64;
        let inlier_indices = best.inlier_indices.iter().copied().collect::<BTreeSet<_>>();
        for (index, group) in clean_groups.iter().enumerate() {
            let residual = fraction_residual_ppm(
                selected.ratio_numerator,
                selected.ratio_denominator,
                group.ratio_numerator,
                group.ratio_denominator,
            )?;
            if inlier_indices.contains(&index) {
                supporting_correlation_group_ids.push(group.correlation_group_id);
                supporting_witness_ids.extend(group.witness_ids.iter().copied());
                let observed_uncertainty = u32::try_from(
                    residual.max(group.internal_residual_ppm),
                )
                .map_err(|_| EdgeScaleError::ArithmeticOverflow {
                    field: "relation_uncertainty_ppm",
                })?;
                uncertainty_ppm = uncertainty_ppm
                    .max(group.uncertainty_ppm)
                    .max(observed_uncertainty);
                evidence_rank = evidence_rank.max(group.evidence_rank);
                independent_support_count = independent_support_count
                    .checked_add(group.support_count)
                    .ok_or(EdgeScaleError::ArithmeticOverflow {
                        field: "independent_support_count",
                    })?;
                for witness_id in &group.witness_ids {
                    let disposition = if *witness_id == group.representative_witness_id {
                        WitnessDisposition::Supporting
                    } else {
                        WitnessDisposition::CorrelatedDuplicate
                    };
                    assessments.insert(
                        *witness_id,
                        WitnessAssessment {
                            witness_id: *witness_id,
                            relation_id: Some(relation_id),
                            disposition,
                            residual_ppm: Some(residual),
                        },
                    );
                }
            } else {
                rejected_witness_ids.extend(group.witness_ids.iter().copied());
                for witness_id in &group.witness_ids {
                    assessments.insert(
                        *witness_id,
                        WitnessAssessment {
                            witness_id: *witness_id,
                            relation_id: Some(relation_id),
                            disposition: WitnessDisposition::ConsensusOutlier,
                            residual_ppm: Some(residual),
                        },
                    );
                }
            }
        }
        supporting_witness_ids.sort_unstable();
        supporting_correlation_group_ids.sort_unstable();
        rejected_witness_ids.sort_unstable();
        relations.push(EdgeScaleRelation {
            relation_id,
            lower_edge_id,
            higher_edge_id,
            ratio_lower_over_higher_nano: fraction_to_nano(
                selected.ratio_numerator,
                selected.ratio_denominator,
            )?,
            supporting_witness_ids,
            supporting_correlation_group_ids,
            rejected_witness_ids,
            uncertainty_ppm,
            evidence_rank,
            independent_support_count,
        });
    }
    let witness_assessments = witnesses
        .iter()
        .map(|witness| {
            assessments
                .remove(&witness.witness_id)
                .ok_or(EdgeScaleError::InternalLookup { field: "witness_assessment" })
        })
        .collect::<Result<Vec<_>, _>>()?;
    Ok((relations, witness_assessments))
}
