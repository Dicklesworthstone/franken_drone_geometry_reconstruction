/// Analyzes one exact canonical undirected graph.
///
/// # Errors
///
/// Returns a stable basis, node, edge, budget, connectivity, or arithmetic error.
pub fn analyze_graph(
    basis: GraphBasis,
    budget: GraphBudget,
    nodes: Vec<GraphNode>,
    edges: Vec<GraphEdge>,
) -> Result<GraphAnalysis, GraphError> {
    validate_basis(&basis)?;
    validate_budget(budget)?;
    let nodes = canonicalize_nodes(nodes)?;
    let edges = canonicalize_edges(edges, &nodes)?;
    let node_indices = nodes
        .iter()
        .enumerate()
        .map(|(index, node)| (node.node_id, index))
        .collect::<BTreeMap<_, _>>();
    let mut union = UnionFind::new(nodes.len());
    let mut ranked = edges.iter().collect::<Vec<_>>();
    ranked.sort_by(|left, right| compare_edge_priority(left, right));
    let mut forest_edge_ids = Vec::new();
    let mut non_forest_edge_ids = Vec::new();
    for edge in ranked {
        let left = *node_indices
            .get(&edge.left_node_id)
            .ok_or(GraphError::UnknownNode {
                edge_id: edge.edge_id,
                node_id: edge.left_node_id,
            })?;
        let right = *node_indices
            .get(&edge.right_node_id)
            .ok_or(GraphError::UnknownNode {
                edge_id: edge.edge_id,
                node_id: edge.right_node_id,
            })?;
        if union.union(left, right)? {
            forest_edge_ids.push(edge.edge_id);
        } else {
            non_forest_edge_ids.push(edge.edge_id);
        }
    }
    forest_edge_ids.sort_unstable();
    non_forest_edge_ids.sort_unstable();
    let edge_by_id = edges
        .iter()
        .map(|edge| (edge.edge_id, edge))
        .collect::<BTreeMap<_, _>>();
    let adjacency = forest_adjacency(&nodes, &forest_edge_ids, &edge_by_id)?;
    let mut path_expansions = 0_u64;
    let mut fundamental_cycles = Vec::with_capacity(non_forest_edge_ids.len());
    let mut cycle_edges = BTreeSet::new();
    for edge_id in &non_forest_edge_ids {
        let edge = edge_by_id
            .get(edge_id)
            .copied()
            .ok_or(GraphError::ArithmeticOverflow {
                field: "non_forest_edge_lookup",
            })?;
        let path = forest_path(
            edge.left_node_id,
            edge.right_node_id,
            &adjacency,
            budget,
            &mut path_expansions,
        )?;
        cycle_edges.insert(edge.edge_id);
        cycle_edges.extend(path.iter().copied());
        fundamental_cycles.push(FundamentalCycle {
            closing_edge_id: edge.edge_id,
            left_node_id: edge.left_node_id,
            right_node_id: edge.right_node_id,
            forest_path_edge_ids: path,
        });
    }
    let bridge_edge_ids = edges
        .iter()
        .filter(|edge| !cycle_edges.contains(&edge.edge_id))
        .map(|edge| edge.edge_id)
        .collect::<Vec<_>>();
    let components = build_components(
        &nodes,
        &edges,
        &forest_edge_ids,
        &mut union,
        &node_indices,
    )?;
    let edge_component_total = usize_to_u64(edges.len())?
        .checked_add(usize_to_u64(components.len())?)
        .ok_or(GraphError::ArithmeticOverflow {
            field: "cycle_rank",
        })?;
    let cycle_rank = edge_component_total
        .checked_sub(usize_to_u64(nodes.len())?)
        .ok_or(GraphError::ArithmeticOverflow {
            field: "cycle_rank",
        })?;
    Ok(GraphAnalysis {
        basis,
        nodes,
        edges,
        components,
        forest_edge_ids,
        non_forest_edge_ids,
        fundamental_cycles,
        bridge_edge_ids,
        cycle_rank,
        path_expansions,
    })
}

fn validate_basis(basis: &GraphBasis) -> Result<(), GraphError> {
    for (field, digest) in [
        ("node_basis_digest", &basis.node_basis_digest),
        ("edge_basis_digest", &basis.edge_basis_digest),
        (
            "selection_policy_digest",
            &basis.selection_policy_digest,
        ),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_nonzero("generation", basis.generation)
}

fn validate_budget(budget: GraphBudget) -> Result<(), GraphError> {
    if budget.max_path_expansions == 0 {
        return Err(GraphError::ZeroValue {
            field: "max_path_expansions",
        });
    }
    if budget.max_path_expansions > MAX_PATH_EXPANSIONS {
        return Err(GraphError::BoundExceeded {
            field: "max_path_expansions",
            actual: u128::from(budget.max_path_expansions),
            maximum: u128::from(MAX_PATH_EXPANSIONS),
        });
    }
    Ok(())
}

fn canonicalize_nodes(mut nodes: Vec<GraphNode>) -> Result<Vec<GraphNode>, GraphError> {
    if nodes.is_empty() {
        return Err(GraphError::EmptyNodes);
    }
    if nodes.len() > MAX_GRAPH_NODES {
        return Err(GraphError::BoundExceeded {
            field: "node_count",
            actual: nodes.len() as u128,
            maximum: MAX_GRAPH_NODES as u128,
        });
    }
    nodes.sort_by_key(|node| node.node_id);
    let mut ids = BTreeSet::new();
    for node in &nodes {
        validate_safe_nonzero("node_id", node.node_id)?;
        reject_zero_digest("node_evidence_digest", &node.evidence_digest)?;
        if !ids.insert(node.node_id) {
            return Err(GraphError::DuplicateNode {
                node_id: node.node_id,
            });
        }
    }
    Ok(nodes)
}

fn canonicalize_edges(
    mut edges: Vec<GraphEdge>,
    nodes: &[GraphNode],
) -> Result<Vec<GraphEdge>, GraphError> {
    if edges.len() > MAX_GRAPH_EDGES {
        return Err(GraphError::BoundExceeded {
            field: "edge_count",
            actual: edges.len() as u128,
            maximum: MAX_GRAPH_EDGES as u128,
        });
    }
    edges.sort_by_key(|edge| edge.edge_id);
    let known = nodes
        .iter()
        .map(|node| node.node_id)
        .collect::<BTreeSet<_>>();
    let mut ids = BTreeSet::new();
    let mut pairs = BTreeSet::new();
    for edge in &edges {
        validate_safe_nonzero("edge_id", edge.edge_id)?;
        validate_safe_number("edge_priority_primary", edge.priority.primary)?;
        validate_safe_number("edge_priority_secondary", edge.priority.secondary)?;
        reject_zero_digest("edge_evidence_digest", &edge.evidence_digest)?;
        if edge.left_node_id >= edge.right_node_id {
            return Err(GraphError::NonCanonicalEdge {
                edge_id: edge.edge_id,
            });
        }
        if !ids.insert(edge.edge_id) {
            return Err(GraphError::DuplicateEdge {
                edge_id: edge.edge_id,
            });
        }
        if !pairs.insert((edge.left_node_id, edge.right_node_id)) {
            return Err(GraphError::DuplicateEndpointPair {
                left_node_id: edge.left_node_id,
                right_node_id: edge.right_node_id,
            });
        }
        for node_id in [edge.left_node_id, edge.right_node_id] {
            if !known.contains(&node_id) {
                return Err(GraphError::UnknownNode {
                    edge_id: edge.edge_id,
                    node_id,
                });
            }
        }
    }
    Ok(edges)
}
