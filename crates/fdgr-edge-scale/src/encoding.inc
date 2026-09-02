fn encode_basis(encoder: &mut Encoder, basis: &EdgeScaleBasis) {
    encoder.put_digest(&basis.pose_graph_generation_digest);
    encoder.put_digest(&basis.edge_subject_basis_digest);
    encoder.put_digest(&basis.witness_basis_digest);
    encoder.put_digest(&basis.policy_digest);
    encoder.put_u64(basis.generation);
}

fn encode_semantic_policy(encoder: &mut Encoder, policy: EdgeScalePolicy) {
    encoder.put_u64(policy.max_within_group_residual_ppm);
    encoder.put_u64(policy.max_consensus_residual_ppm);
    encoder.put_u64(policy.max_cycle_residual_ppm);
    encoder.put_u16(policy.min_cross_validation_groups);
    encoder.put_u64(policy.max_relative_scale_nano);
}

fn encode_subjects(
    encoder: &mut Encoder,
    subjects: &[EdgeScaleSubject],
) -> Result<(), EdgeScaleError> {
    encoder.put_u64(usize_to_u64(subjects.len())?);
    for subject in subjects {
        encoder.put_u64(subject.edge_id);
        encoder.put_digest(&subject.pose_verification_digest);
        encoder.put_u64(subject.pose_component_root_node_id);
    }
    Ok(())
}

fn encode_witnesses(
    encoder: &mut Encoder,
    witnesses: &[EdgeRatioWitness],
) -> Result<(), EdgeScaleError> {
    encoder.put_u64(usize_to_u64(witnesses.len())?);
    for witness in witnesses {
        encoder.put_u64(witness.witness_id);
        encoder.put_digest(&witness.evidence_digest);
        encoder.put_u64(witness.correlation_group_id);
        encoder.put_u64(witness.lower_edge_id);
        encoder.put_u64(witness.higher_edge_id);
        encoder.put_u64(witness.ratio_numerator);
        encoder.put_u64(witness.ratio_denominator);
        encoder.put_u32(witness.uncertainty_ppm);
        encoder.put_u32(witness.support_count);
        encoder.put_u8(witness.source.code());
    }
    Ok(())
}

fn encode_witness_assessments(
    encoder: &mut Encoder,
    assessments: &[WitnessAssessment],
) -> Result<(), EdgeScaleError> {
    encoder.put_u64(usize_to_u64(assessments.len())?);
    for assessment in assessments {
        encoder.put_u64(assessment.witness_id);
        encode_optional_u64(encoder, assessment.relation_id);
        encoder.put_u8(assessment.disposition.code());
        encode_optional_u64(encoder, assessment.residual_ppm);
    }
    Ok(())
}

fn encode_relations(
    encoder: &mut Encoder,
    relations: &[EdgeScaleRelation],
) -> Result<(), EdgeScaleError> {
    encoder.put_u64(usize_to_u64(relations.len())?);
    for relation in relations {
        encoder.put_u64(relation.relation_id);
        encoder.put_u64(relation.lower_edge_id);
        encoder.put_u64(relation.higher_edge_id);
        encoder.put_u64(relation.ratio_lower_over_higher_nano);
        encode_u64_sequence(encoder, &relation.supporting_witness_ids)?;
        encode_u64_sequence(
            encoder,
            &relation.supporting_correlation_group_ids,
        )?;
        encode_u64_sequence(encoder, &relation.rejected_witness_ids)?;
        encoder.put_u32(relation.uncertainty_ppm);
        encoder.put_u64(relation.evidence_rank);
        encoder.put_u64(relation.independent_support_count);
    }
    Ok(())
}

fn encode_scales(
    encoder: &mut Encoder,
    scales: &[ReconciledEdgeScale],
) -> Result<(), EdgeScaleError> {
    encoder.put_u64(usize_to_u64(scales.len())?);
    for scale in scales {
        encoder.put_u64(scale.edge_id);
        encoder.put_u64(scale.pose_component_root_node_id);
        encoder.put_u64(scale.scale_component_root_edge_id);
        encoder.put_u64(scale.relative_scale_nano);
        encode_optional_u64(encoder, scale.parent_edge_id);
        encode_optional_u64(encoder, scale.parent_relation_id);
    }
    Ok(())
}

fn encode_relation_assessments(
    encoder: &mut Encoder,
    assessments: &[RelationAssessment],
) -> Result<(), EdgeScaleError> {
    encoder.put_u64(usize_to_u64(assessments.len())?);
    for assessment in assessments {
        encoder.put_u64(assessment.relation_id);
        encoder.put_u64(assessment.implied_ratio_nano);
        encoder.put_u64(assessment.residual_ppm);
        encoder.put_u8(assessment.status.code());
    }
    Ok(())
}

fn encode_components(
    encoder: &mut Encoder,
    components: &[EdgeScaleComponent],
) -> Result<(), EdgeScaleError> {
    encoder.put_u64(usize_to_u64(components.len())?);
    for component in components {
        encoder.put_u64(component.scale_component_root_edge_id);
        encoder.put_u64(component.pose_component_root_node_id);
        encode_u64_sequence(encoder, &component.edge_ids)?;
        encode_u64_sequence(encoder, &component.relation_ids)?;
        encode_u64_sequence(encoder, &component.forest_relation_ids)?;
        encode_u64_sequence(
            encoder,
            &component.independent_correlation_group_ids,
        )?;
        encoder.put_u8(component.status.code());
        encoder.put_bool(component.cross_validated);
        encode_u64_sequence(encoder, &component.conflicting_relation_ids)?;
    }
    Ok(())
}

fn encode_u64_sequence(
    encoder: &mut Encoder,
    values: &[u64],
) -> Result<(), EdgeScaleError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn encode_optional_u64(encoder: &mut Encoder, value: Option<u64>) {
    match value {
        Some(value) => {
            encoder.put_bool(true);
            encoder.put_u64(value);
        }
        None => encoder.put_bool(false),
    }
}

fn push_u64_json_array(
    output: &mut String,
    values: &[u64],
) -> Result<(), EdgeScaleError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}
