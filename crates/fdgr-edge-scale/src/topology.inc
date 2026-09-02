fn analyze_relation_graph(
    basis: &EdgeScaleBasis,
    subjects: &[EdgeScaleSubject],
    relations: &[EdgeScaleRelation],
    policy: EdgeScalePolicy,
) -> Result<GraphAnalysis, EdgeScaleError> {
    let nodes = subjects
        .iter()
        .map(|subject| GraphNode {
            node_id: subject.edge_id,
            evidence_digest: subject.pose_verification_digest.clone(),
        })
        .collect::<Vec<_>>();
    let mut edges = Vec::with_capacity(relations.len());
    for relation in relations {
        edges.push(GraphEdge {
            edge_id: relation.relation_id,
            left_node_id: relation.lower_edge_id,
            right_node_id: relation.higher_edge_id,
            priority: graph_priority(relation),
            evidence_digest: relation_evidence_digest(relation)?,
        });
    }
    Ok(analyze_graph(
        GraphBasis {
            node_basis_digest: basis.edge_subject_basis_digest.clone(),
            edge_basis_digest: basis.witness_basis_digest.clone(),
            selection_policy_digest: basis.policy_digest.clone(),
            generation: basis.generation,
        },
        GraphBudget { max_path_expansions: policy.max_path_expansions },
        nodes,
        edges,
    )?)
}
