fn relation_evidence_digest(relation: &EdgeScaleRelation) -> Result<EvidenceDigest, EdgeScaleError> {
    let mut encoder = Encoder::with_capacity(512);
    encoder.put_str("fdgr.edge_scale_relation/1")?;
    encoder.put_u64(relation.relation_id);
    encoder.put_u64(relation.lower_edge_id);
    encoder.put_u64(relation.higher_edge_id);
    encoder.put_u64(relation.ratio_lower_over_higher_nano);
    encode_u64_sequence(&mut encoder, &relation.supporting_witness_ids)?;
    encode_u64_sequence(
        &mut encoder,
        &relation.supporting_correlation_group_ids,
    )?;
    encode_u64_sequence(&mut encoder, &relation.rejected_witness_ids)?;
    encoder.put_u32(relation.uncertainty_ppm);
    encoder.put_u64(relation.evidence_rank);
    encoder.put_u64(relation.independent_support_count);
    let domain = DigestDomain::parse("fdgr.edge_scale_relation/1")?;
    Ok(hash_domain(&domain, &encoder.into_bytes())?)
}

fn propagate_scales(
    subjects: &[EdgeScaleSubject],
    relations: &[EdgeScaleRelation],
    topology: &GraphAnalysis,
    policy: EdgeScalePolicy,
) -> Result<Vec<ReconciledEdgeScale>, EdgeScaleError> {
    let subject_by_id = subjects
        .iter()
        .map(|subject| (subject.edge_id, subject))
        .collect::<BTreeMap<_, _>>();
    let relation_by_id = relations
        .iter()
        .map(|relation| (relation.relation_id, relation))
        .collect::<BTreeMap<_, _>>();
    let mut adjacency = subjects
        .iter()
        .map(|subject| (subject.edge_id, Vec::<(u64, u64, bool)>::new()))
        .collect::<BTreeMap<_, _>>();
    for relation_id in &topology.forest_edge_ids {
        let relation = relation_by_id.get(relation_id).copied().ok_or(
            EdgeScaleError::InternalLookup { field: "forest_scale_relation" },
        )?;
        adjacency
            .get_mut(&relation.lower_edge_id)
            .ok_or(EdgeScaleError::InternalLookup { field: "lower_scale_adjacency" })?
            .push((relation.higher_edge_id, relation.relation_id, true));
        adjacency
            .get_mut(&relation.higher_edge_id)
            .ok_or(EdgeScaleError::InternalLookup { field: "higher_scale_adjacency" })?
            .push((relation.lower_edge_id, relation.relation_id, false));
    }
    for neighbors in adjacency.values_mut() {
        neighbors.sort_unstable();
    }
    let mut output = BTreeMap::<u64, ReconciledEdgeScale>::new();
    for component in &topology.components {
        let root = component.component_id;
        let root_subject = subject_by_id.get(&root).copied().ok_or(
            EdgeScaleError::InternalLookup { field: "scale_component_root" },
        )?;
        output.insert(
            root,
            ReconciledEdgeScale {
                edge_id: root,
                pose_component_root_node_id: root_subject.pose_component_root_node_id,
                scale_component_root_edge_id: root,
                relative_scale_nano: UNIT_NANO,
                parent_edge_id: None,
                parent_relation_id: None,
            },
        );
        let mut queue = VecDeque::new();
        queue.push_back(root);
        while let Some(parent_edge_id) = queue.pop_front() {
            let parent_scale = output
                .get(&parent_edge_id)
                .ok_or(EdgeScaleError::InternalLookup { field: "parent_edge_scale" })?
                .relative_scale_nano;
            let neighbors = adjacency.get(&parent_edge_id).ok_or(
                EdgeScaleError::InternalLookup { field: "scale_adjacency" },
            )?;
            for (child_edge_id, relation_id, parent_is_lower) in neighbors {
                if output.contains_key(child_edge_id) {
                    continue;
                }
                let relation = relation_by_id.get(relation_id).copied().ok_or(
                    EdgeScaleError::InternalLookup { field: "propagation_relation" },
                )?;
                let child_scale = if *parent_is_lower {
                    mul_div_round(
                        parent_scale,
                        UNIT_NANO,
                        relation.ratio_lower_over_higher_nano,
                        "higher_edge_scale",
                    )?
                } else {
                    mul_div_round(
                        parent_scale,
                        relation.ratio_lower_over_higher_nano,
                        UNIT_NANO,
                        "lower_edge_scale",
                    )?
                };
                if child_scale == 0 || child_scale > policy.max_relative_scale_nano {
                    return Err(EdgeScaleError::BoundExceeded {
                        field: "relative_scale_nano",
                        actual: u128::from(child_scale),
                        maximum: u128::from(policy.max_relative_scale_nano),
                    });
                }
                let subject = subject_by_id.get(child_edge_id).copied().ok_or(
                    EdgeScaleError::InternalLookup { field: "child_scale_subject" },
                )?;
                output.insert(
                    *child_edge_id,
                    ReconciledEdgeScale {
                        edge_id: *child_edge_id,
                        pose_component_root_node_id: subject.pose_component_root_node_id,
                        scale_component_root_edge_id: root,
                        relative_scale_nano: child_scale,
                        parent_edge_id: Some(parent_edge_id),
                        parent_relation_id: Some(*relation_id),
                    },
                );
                queue.push_back(*child_edge_id);
            }
        }
    }
    if output.len() != subjects.len() {
        return Err(EdgeScaleError::InternalLookup { field: "complete_scale_coverage" });
    }
    Ok(output.into_values().collect())
}
