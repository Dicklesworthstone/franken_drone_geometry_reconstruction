/// Builds deterministic component-local orientations and rotation-cycle evidence.
///
/// # Errors
///
/// Returns a stable basis, policy, node, edge, graph, rotation, or arithmetic error.
pub fn build_pose_graph(
    basis: PoseGraphBasis,
    policy: PoseGraphPolicy,
    nodes: Vec<PoseNode>,
    edges: Vec<RelativePoseEdge>,
) -> Result<PoseGraphGeneration, PoseGraphError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    let nodes = canonicalize_nodes(nodes)?;
    let edges = canonicalize_edges(edges, &nodes)?;
    let graph_nodes = nodes
        .iter()
        .map(|node| GraphNode {
            node_id: node.node_id,
            evidence_digest: node.keyframe_digest.clone(),
        })
        .collect::<Vec<_>>();
    let graph_edges = edges
        .iter()
        .map(|edge| GraphEdge {
            edge_id: edge.edge_id,
            left_node_id: edge.left_node_id.min(edge.right_node_id),
            right_node_id: edge.left_node_id.max(edge.right_node_id),
            priority: EdgePriority {
                primary: u64::from(edge.supported_match_count),
                secondary: edge.median_residual_nano,
            },
            evidence_digest: edge.verification_digest.clone(),
        })
        .collect::<Vec<_>>();
    let topology = analyze_graph(
        GraphBasis {
            node_basis_digest: basis.node_basis_digest.clone(),
            edge_basis_digest: basis.edge_basis_digest.clone(),
            selection_policy_digest: basis.graph_selection_policy_digest.clone(),
            generation: basis.generation,
        },
        GraphBudget {
            max_path_expansions: policy.max_path_expansions,
        },
        graph_nodes,
        graph_edges,
    )?;
    let orientations = propagate_orientations(&nodes, &edges, &topology, policy)?;
    let cycle_assessments = assess_rotation_cycles(&edges, &topology, &orientations, policy)?;
    let components = build_pose_components(&topology, &cycle_assessments);
    Ok(PoseGraphGeneration {
        basis,
        policy,
        nodes,
        edges,
        topology,
        orientations,
        cycle_assessments,
        components,
        translation_status: TranslationStatus::DirectionOnlyScaleUnderdetermined,
    })
}

fn validate_basis(basis: &PoseGraphBasis) -> Result<(), PoseGraphError> {
    for (field, digest) in [
        ("node_basis_digest", &basis.node_basis_digest),
        ("edge_basis_digest", &basis.edge_basis_digest),
        (
            "graph_selection_policy_digest",
            &basis.graph_selection_policy_digest,
        ),
        ("rotation_policy_digest", &basis.rotation_policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_nonzero("generation", basis.generation)
}

fn validate_policy(policy: PoseGraphPolicy) -> Result<(), PoseGraphError> {
    for (field, value) in [
        (
            "max_rotation_cycle_residual_ppm",
            policy.max_rotation_cycle_residual_ppm,
        ),
        (
            "max_orientation_drift_ppm",
            policy.max_orientation_drift_ppm,
        ),
    ] {
        if value > MAX_ROTATION_ERROR_PPM {
            return Err(PoseGraphError::BoundExceeded {
                field,
                actual: u128::from(value),
                maximum: u128::from(MAX_ROTATION_ERROR_PPM),
            });
        }
    }
    if policy.max_path_expansions == 0 {
        return Err(PoseGraphError::ZeroValue {
            field: "max_path_expansions",
        });
    }
    Ok(())
}

fn canonicalize_nodes(mut nodes: Vec<PoseNode>) -> Result<Vec<PoseNode>, PoseGraphError> {
    if nodes.is_empty() {
        return Err(PoseGraphError::ZeroValue {
            field: "node_count",
        });
    }
    nodes.sort_by_key(|node| node.node_id);
    let mut ids = BTreeSet::new();
    let mut samples = BTreeSet::new();
    for node in &nodes {
        validate_safe_nonzero("node_id", node.node_id)?;
        validate_safe_number("sample_index", node.sample_index)?;
        reject_zero_digest("keyframe_digest", &node.keyframe_digest)?;
        if !ids.insert(node.node_id) {
            return Err(PoseGraphError::DuplicateNode {
                field: "node_id",
                value: node.node_id,
            });
        }
        if !samples.insert(node.sample_index) {
            return Err(PoseGraphError::DuplicateNode {
                field: "sample_index",
                value: node.sample_index,
            });
        }
    }
    Ok(nodes)
}

fn canonicalize_edges(
    mut edges: Vec<RelativePoseEdge>,
    nodes: &[PoseNode],
) -> Result<Vec<RelativePoseEdge>, PoseGraphError> {
    edges.sort_by_key(|edge| edge.edge_id);
    let node_by_id = nodes
        .iter()
        .map(|node| (node.node_id, node))
        .collect::<BTreeMap<_, _>>();
    let mut ids = BTreeSet::new();
    let mut endpoint_pairs = BTreeSet::new();
    let mut verification_candidates = BTreeSet::new();
    for edge in &edges {
        validate_safe_nonzero("edge_id", edge.edge_id)?;
        validate_safe_nonzero("admitted_candidate_id", edge.admitted_candidate_id)?;
        validate_safe_number("left_sample_index", edge.left_sample_index)?;
        validate_safe_number("right_sample_index", edge.right_sample_index)?;
        reject_zero_digest("verification_digest", &edge.verification_digest)?;
        if edge.supported_match_count == 0 {
            return Err(PoseGraphError::ZeroValue {
                field: "supported_match_count",
            });
        }
        if edge.median_residual_nano > UNIT_NANO.unsigned_abs() {
            return Err(PoseGraphError::BoundExceeded {
                field: "median_residual_nano",
                actual: u128::from(edge.median_residual_nano),
                maximum: u128::from(UNIT_NANO.unsigned_abs()),
            });
        }
        if edge.left_node_id == edge.right_node_id
            || edge.left_sample_index >= edge.right_sample_index
        {
            return Err(PoseGraphError::NonCanonicalEdge {
                edge_id: edge.edge_id,
            });
        }
        if !ids.insert(edge.edge_id) {
            return Err(PoseGraphError::DuplicateEdge {
                field: "edge_id",
                value: edge.edge_id,
            });
        }
        let endpoint_pair = (
            edge.left_node_id.min(edge.right_node_id),
            edge.left_node_id.max(edge.right_node_id),
        );
        if !endpoint_pairs.insert(endpoint_pair) {
            return Err(PoseGraphError::DuplicateEdge {
                field: "endpoint_pair",
                value: edge.edge_id,
            });
        }
        if !verification_candidates.insert((
            edge.verification_digest.clone(),
            edge.admitted_candidate_id,
        )) {
            return Err(PoseGraphError::DuplicateEdge {
                field: "verification_candidate",
                value: edge.admitted_candidate_id,
            });
        }
        for (node_id, observed_sample) in [
            (edge.left_node_id, edge.left_sample_index),
            (edge.right_node_id, edge.right_sample_index),
        ] {
            let node = node_by_id
                .get(&node_id)
                .copied()
                .ok_or(PoseGraphError::UnknownNode {
                    edge_id: edge.edge_id,
                    node_id,
                })?;
            if node.sample_index != observed_sample {
                return Err(PoseGraphError::SampleMismatch {
                    edge_id: edge.edge_id,
                    node_id,
                    expected: node.sample_index,
                    observed: observed_sample,
                });
            }
        }
        validate_rotation(
            "rotation_right_from_left_nano",
            edge.rotation_right_from_left_nano,
            MAX_INPUT_ROTATION_ERROR_PPM,
        )?;
        validate_unit_direction(
            edge.edge_id,
            edge.translation_right_from_left_direction_nano,
        )?;
    }
    Ok(edges)
}

fn propagate_orientations(
    nodes: &[PoseNode],
    edges: &[RelativePoseEdge],
    topology: &GraphAnalysis,
    policy: PoseGraphPolicy,
) -> Result<Vec<NodeOrientation>, PoseGraphError> {
    let node_by_id = nodes
        .iter()
        .map(|node| (node.node_id, node))
        .collect::<BTreeMap<_, _>>();
    let edge_by_id = edges
        .iter()
        .map(|edge| (edge.edge_id, edge))
        .collect::<BTreeMap<_, _>>();
    let mut adjacency = node_by_id
        .keys()
        .copied()
        .map(|node_id| (node_id, Vec::<(u64, u64, bool)>::new()))
        .collect::<BTreeMap<_, _>>();
    for edge_id in &topology.forest_edge_ids {
        let edge = edge_by_id
            .get(edge_id)
            .copied()
            .ok_or(PoseGraphError::InternalLookup {
                field: "forest_relative_pose_edge",
            })?;
        adjacency
            .get_mut(&edge.left_node_id)
            .ok_or(PoseGraphError::InternalLookup {
                field: "left_forest_adjacency",
            })?
            .push((edge.right_node_id, edge.edge_id, true));
        adjacency
            .get_mut(&edge.right_node_id)
            .ok_or(PoseGraphError::InternalLookup {
                field: "right_forest_adjacency",
            })?
            .push((edge.left_node_id, edge.edge_id, false));
    }
    for neighbors in adjacency.values_mut() {
        neighbors.sort_unstable();
    }
    let mut output = BTreeMap::<u64, NodeOrientation>::new();
    for component in &topology.components {
        let root = component.component_id;
        let root_node = node_by_id
            .get(&root)
            .copied()
            .ok_or(PoseGraphError::InternalLookup {
                field: "component_root_node",
            })?;
        output.insert(
            root,
            NodeOrientation {
                node_id: root,
                sample_index: root_node.sample_index,
                component_root_node_id: root,
                rotation_node_from_root_nano: identity_rotation(),
                parent_node_id: None,
                parent_edge_id: None,
            },
        );
        let mut queue = VecDeque::new();
        queue.push_back(root);
        while let Some(parent_id) = queue.pop_front() {
            let parent_rotation = output
                .get(&parent_id)
                .ok_or(PoseGraphError::InternalLookup {
                    field: "parent_orientation",
                })?
                .rotation_node_from_root_nano;
            let neighbors = adjacency
                .get(&parent_id)
                .ok_or(PoseGraphError::InternalLookup {
                    field: "forest_adjacency",
                })?;
            for (child_id, edge_id, forward) in neighbors {
                if output.contains_key(child_id) {
                    continue;
                }
                let edge = edge_by_id
                    .get(edge_id)
                    .copied()
                    .ok_or(PoseGraphError::InternalLookup {
                        field: "orientation_edge",
                    })?;
                let relative = if *forward {
                    edge.rotation_right_from_left_nano
                } else {
                    transpose_rotation(edge.rotation_right_from_left_nano)
                };
                let rotation = multiply_rotations(relative, parent_rotation)?;
                let drift = rotation_error_ppm(rotation)?;
                if drift > policy.max_orientation_drift_ppm {
                    return Err(PoseGraphError::OrientationDriftExceeded {
                        node_id: *child_id,
                        error_ppm: drift,
                        maximum_ppm: policy.max_orientation_drift_ppm,
                    });
                }
                let child = node_by_id
                    .get(child_id)
                    .copied()
                    .ok_or(PoseGraphError::InternalLookup {
                        field: "child_node",
                    })?;
                output.insert(
                    *child_id,
                    NodeOrientation {
                        node_id: *child_id,
                        sample_index: child.sample_index,
                        component_root_node_id: root,
                        rotation_node_from_root_nano: rotation,
                        parent_node_id: Some(parent_id),
                        parent_edge_id: Some(*edge_id),
                    },
                );
                queue.push_back(*child_id);
            }
        }
    }
    if output.len() != nodes.len() {
        return Err(PoseGraphError::InternalLookup {
            field: "complete_orientation_coverage",
        });
    }
    Ok(output.into_values().collect())
}

fn assess_rotation_cycles(
    edges: &[RelativePoseEdge],
    topology: &GraphAnalysis,
    orientations: &[NodeOrientation],
    policy: PoseGraphPolicy,
) -> Result<Vec<RotationCycleAssessment>, PoseGraphError> {
    let edge_by_id = edges
        .iter()
        .map(|edge| (edge.edge_id, edge))
        .collect::<BTreeMap<_, _>>();
    let orientation_by_id = orientations
        .iter()
        .map(|orientation| (orientation.node_id, orientation))
        .collect::<BTreeMap<_, _>>();
    let mut assessments = Vec::with_capacity(topology.fundamental_cycles.len());
    for cycle in &topology.fundamental_cycles {
        let edge = edge_by_id
            .get(&cycle.closing_edge_id)
            .copied()
            .ok_or(PoseGraphError::InternalLookup {
                field: "cycle_closing_edge",
            })?;
        let left = orientation_by_id
            .get(&edge.left_node_id)
            .copied()
            .ok_or(PoseGraphError::InternalLookup {
                field: "cycle_left_orientation",
            })?;
        let right = orientation_by_id
            .get(&edge.right_node_id)
            .copied()
            .ok_or(PoseGraphError::InternalLookup {
                field: "cycle_right_orientation",
            })?;
        if left.component_root_node_id != right.component_root_node_id {
            return Err(PoseGraphError::InternalLookup {
                field: "cycle_component",
            });
        }
        let implied = multiply_rotations(
            right.rotation_node_from_root_nano,
            transpose_rotation(left.rotation_node_from_root_nano),
        )?;
        let residual = rotation_difference_ppm(
            edge.rotation_right_from_left_nano,
            implied,
        )?;
        assessments.push(RotationCycleAssessment {
            closing_edge_id: edge.edge_id,
            forest_path_edge_ids: cycle.forest_path_edge_ids.clone(),
            rotation_residual_ppm: residual,
            status: if residual <= policy.max_rotation_cycle_residual_ppm {
                RotationCycleStatus::Consistent
            } else {
                RotationCycleStatus::Conflicting
            },
        });
    }
    assessments.sort_by_key(|assessment| assessment.closing_edge_id);
    Ok(assessments)
}

fn build_pose_components(
    topology: &GraphAnalysis,
    cycles: &[RotationCycleAssessment],
) -> Vec<PoseGraphComponent> {
    let closing_edges = cycles
        .iter()
        .map(|cycle| (cycle.closing_edge_id, cycle.status))
        .collect::<BTreeMap<_, _>>();
    let mut components = Vec::with_capacity(topology.components.len());
    for component in &topology.components {
        let edge_set = component.edge_ids.iter().copied().collect::<BTreeSet<_>>();
        let mut cycle_count = 0_usize;
        let mut conflicting_edge_ids = Vec::new();
        for (edge_id, status) in &closing_edges {
            if edge_set.contains(edge_id) {
                cycle_count = cycle_count.saturating_add(1);
                if *status == RotationCycleStatus::Conflicting {
                    conflicting_edge_ids.push(*edge_id);
                }
            }
        }
        let orientation_status = if !conflicting_edge_ids.is_empty() {
            OrientationStatus::Conflicted
        } else if cycle_count == 0 {
            OrientationStatus::TreeOnly
        } else {
            OrientationStatus::CycleConsistent
        };
        components.push(PoseGraphComponent {
            component_root_node_id: component.component_id,
            node_ids: component.node_ids.clone(),
            edge_ids: component.edge_ids.clone(),
            forest_edge_ids: component.forest_edge_ids.clone(),
            orientation_status,
            conflicting_edge_ids,
        });
    }
    components
}
