fn assess_relations(
    relations: &[EdgeScaleRelation],
    topology: &GraphAnalysis,
    scales: &[ReconciledEdgeScale],
    policy: EdgeScalePolicy,
) -> Result<Vec<RelationAssessment>, EdgeScaleError> {
    let scale_by_edge = scales
        .iter()
        .map(|scale| (scale.edge_id, scale.relative_scale_nano))
        .collect::<BTreeMap<_, _>>();
    let forest = topology.forest_edge_ids.iter().copied().collect::<BTreeSet<_>>();
    let mut assessments = Vec::with_capacity(relations.len());
    for relation in relations {
        let lower = *scale_by_edge.get(&relation.lower_edge_id).ok_or(
            EdgeScaleError::InternalLookup { field: "relation_lower_scale" },
        )?;
        let higher = *scale_by_edge.get(&relation.higher_edge_id).ok_or(
            EdgeScaleError::InternalLookup { field: "relation_higher_scale" },
        )?;
        let implied = implied_ratio_nano(lower, higher)?;
        let residual = relation_residual_ppm(implied, relation.ratio_lower_over_higher_nano)?;
        let status = if forest.contains(&relation.relation_id) {
            RelationStatus::Forest
        } else if residual <= policy.max_cycle_residual_ppm {
            RelationStatus::Consistent
        } else {
            RelationStatus::Conflicting
        };
        assessments.push(RelationAssessment {
            relation_id: relation.relation_id,
            implied_ratio_nano: implied,
            residual_ppm: residual,
            status,
        });
    }
    Ok(assessments)
}

fn build_scale_components(
    subjects: &[EdgeScaleSubject],
    relations: &[EdgeScaleRelation],
    topology: &GraphAnalysis,
    assessments: &[RelationAssessment],
    policy: EdgeScalePolicy,
) -> Result<Vec<EdgeScaleComponent>, EdgeScaleError> {
    let subject_by_id = subjects
        .iter()
        .map(|subject| (subject.edge_id, subject))
        .collect::<BTreeMap<_, _>>();
    let relation_by_id = relations
        .iter()
        .map(|relation| (relation.relation_id, relation))
        .collect::<BTreeMap<_, _>>();
    let assessment_by_id = assessments
        .iter()
        .map(|assessment| (assessment.relation_id, assessment))
        .collect::<BTreeMap<_, _>>();
    let mut output = Vec::with_capacity(topology.components.len());
    for component in &topology.components {
        let root_subject = subject_by_id.get(&component.component_id).copied().ok_or(
            EdgeScaleError::InternalLookup { field: "component_pose_root" },
        )?;
        let mut groups = BTreeSet::new();
        let mut conflicting_relation_ids = Vec::new();
        for relation_id in &component.edge_ids {
            let relation = relation_by_id.get(relation_id).copied().ok_or(
                EdgeScaleError::InternalLookup { field: "component_relation" },
            )?;
            groups.extend(relation.supporting_correlation_group_ids.iter().copied());
            let assessment = assessment_by_id.get(relation_id).copied().ok_or(
                EdgeScaleError::InternalLookup { field: "component_relation_assessment" },
            )?;
            if assessment.status == RelationStatus::Conflicting {
                conflicting_relation_ids.push(*relation_id);
            }
        }
        let status = if component.edge_ids.is_empty() {
            EdgeScaleComponentStatus::Isolated
        } else if !conflicting_relation_ids.is_empty() {
            EdgeScaleComponentStatus::Conflicted
        } else if component.edge_ids.len() == component.forest_edge_ids.len() {
            EdgeScaleComponentStatus::TreeOnly
        } else {
            EdgeScaleComponentStatus::CycleConsistent
        };
        let required_groups = usize::from(policy.min_cross_validation_groups);
        let cross_validated = !component.forest_edge_ids.is_empty()
            && conflicting_relation_ids.is_empty()
            && component.forest_edge_ids.iter().all(|relation_id| {
                relation_by_id
                    .get(relation_id)
                    .is_some_and(|relation| {
                        relation.supporting_correlation_group_ids.len() >= required_groups
                    })
            });
        output.push(EdgeScaleComponent {
            scale_component_root_edge_id: component.component_id,
            pose_component_root_node_id: root_subject.pose_component_root_node_id,
            edge_ids: component.node_ids.clone(),
            relation_ids: component.edge_ids.clone(),
            forest_relation_ids: component.forest_edge_ids.clone(),
            independent_correlation_group_ids: groups.iter().copied().collect(),
            status,
            cross_validated,
            conflicting_relation_ids,
        });
    }
    Ok(output)
}
