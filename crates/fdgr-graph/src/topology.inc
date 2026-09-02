fn compare_edge_priority(left: &GraphEdge, right: &GraphEdge) -> Ordering {
    right
        .priority
        .primary
        .cmp(&left.priority.primary)
        .then(
            left.priority
                .secondary
                .cmp(&right.priority.secondary),
        )
        .then(left.edge_id.cmp(&right.edge_id))
}

fn forest_adjacency<'a>(
    nodes: &[GraphNode],
    forest_edge_ids: &[u64],
    edge_by_id: &BTreeMap<u64, &'a GraphEdge>,
) -> Result<BTreeMap<u64, Vec<(u64, u64)>>, GraphError> {
    let mut adjacency = nodes
        .iter()
        .map(|node| (node.node_id, Vec::new()))
        .collect::<BTreeMap<_, _>>();
    for edge_id in forest_edge_ids {
        let edge = edge_by_id
            .get(edge_id)
            .copied()
            .ok_or(GraphError::ArithmeticOverflow {
                field: "forest_edge_lookup",
            })?;
        adjacency
            .get_mut(&edge.left_node_id)
            .ok_or(GraphError::UnknownNode {
                edge_id: edge.edge_id,
                node_id: edge.left_node_id,
            })?
            .push((edge.right_node_id, edge.edge_id));
        adjacency
            .get_mut(&edge.right_node_id)
            .ok_or(GraphError::UnknownNode {
                edge_id: edge.edge_id,
                node_id: edge.right_node_id,
            })?
            .push((edge.left_node_id, edge.edge_id));
    }
    for neighbors in adjacency.values_mut() {
        neighbors.sort_unstable();
    }
    Ok(adjacency)
}

fn forest_path(
    start: u64,
    goal: u64,
    adjacency: &BTreeMap<u64, Vec<(u64, u64)>>,
    budget: GraphBudget,
    path_expansions: &mut u64,
) -> Result<Vec<u64>, GraphError> {
    let mut queue = VecDeque::new();
    queue.push_back(start);
    let mut parent = BTreeMap::<u64, (u64, u64)>::new();
    let mut visited = BTreeSet::new();
    visited.insert(start);
    while let Some(node) = queue.pop_front() {
        if node == goal {
            break;
        }
        let neighbors = adjacency
            .get(&node)
            .ok_or(GraphError::ForestPathMissing {
                left_node_id: start,
                right_node_id: goal,
            })?;
        for (neighbor, edge_id) in neighbors {
            let attempted = path_expansions
                .checked_add(1)
                .ok_or(GraphError::ArithmeticOverflow {
                    field: "path_expansions",
                })?;
            if attempted > budget.max_path_expansions {
                return Err(GraphError::PathBudgetExceeded {
                    attempted,
                    maximum: budget.max_path_expansions,
                });
            }
            *path_expansions = attempted;
            if visited.insert(*neighbor) {
                parent.insert(*neighbor, (node, *edge_id));
                queue.push_back(*neighbor);
            }
        }
    }
    if !visited.contains(&goal) {
        return Err(GraphError::ForestPathMissing {
            left_node_id: start,
            right_node_id: goal,
        });
    }
    let mut reversed = Vec::new();
    let mut current = goal;
    while current != start {
        let (previous, edge_id) = parent
            .get(&current)
            .copied()
            .ok_or(GraphError::ForestPathMissing {
                left_node_id: start,
                right_node_id: goal,
            })?;
        reversed.push(edge_id);
        current = previous;
    }
    reversed.reverse();
    Ok(reversed)
}

fn build_components(
    nodes: &[GraphNode],
    edges: &[GraphEdge],
    forest_edge_ids: &[u64],
    union: &mut UnionFind,
    node_indices: &BTreeMap<u64, usize>,
) -> Result<Vec<GraphComponent>, GraphError> {
    let mut node_groups = BTreeMap::<usize, Vec<u64>>::new();
    for node in nodes {
        let index = *node_indices
            .get(&node.node_id)
            .ok_or(GraphError::ArithmeticOverflow {
                field: "component_node_index",
            })?;
        let root = union.find(index)?;
        node_groups.entry(root).or_default().push(node.node_id);
    }
    let mut root_by_node = BTreeMap::new();
    for (root, node_ids) in &node_groups {
        for node_id in node_ids {
            root_by_node.insert(*node_id, *root);
        }
    }
    let forest = forest_edge_ids.iter().copied().collect::<BTreeSet<_>>();
    let mut edge_groups = BTreeMap::<usize, Vec<u64>>::new();
    let mut forest_groups = BTreeMap::<usize, Vec<u64>>::new();
    for edge in edges {
        let root = *root_by_node
            .get(&edge.left_node_id)
            .ok_or(GraphError::ArithmeticOverflow {
                field: "component_edge_root",
            })?;
        edge_groups.entry(root).or_default().push(edge.edge_id);
        if forest.contains(&edge.edge_id) {
            forest_groups.entry(root).or_default().push(edge.edge_id);
        }
    }
    let mut components = Vec::with_capacity(node_groups.len());
    for (root, mut node_ids) in node_groups {
        node_ids.sort_unstable();
        let component_id = *node_ids
            .first()
            .ok_or(GraphError::ArithmeticOverflow {
                field: "component_id",
            })?;
        let mut edge_ids = edge_groups.remove(&root).unwrap_or_default();
        let mut component_forest = forest_groups.remove(&root).unwrap_or_default();
        edge_ids.sort_unstable();
        component_forest.sort_unstable();
        components.push(GraphComponent {
            component_id,
            node_ids,
            edge_ids,
            forest_edge_ids: component_forest,
        });
    }
    components.sort_by_key(|component| component.component_id);
    Ok(components)
}

#[derive(Clone, Debug)]
struct UnionFind {
    parent: Vec<usize>,
    rank: Vec<u8>,
}

impl UnionFind {
    fn new(length: usize) -> Self {
        Self {
            parent: (0..length).collect(),
            rank: vec![0; length],
        }
    }

    fn find(&mut self, mut index: usize) -> Result<usize, GraphError> {
        let mut root = index;
        loop {
            let parent = *self
                .parent
                .get(root)
                .ok_or(GraphError::ArithmeticOverflow {
                    field: "union_find_root",
                })?;
            if parent == root {
                break;
            }
            root = parent;
        }
        while index != root {
            let next = *self
                .parent
                .get(index)
                .ok_or(GraphError::ArithmeticOverflow {
                    field: "union_find_compress",
                })?;
            *self
                .parent
                .get_mut(index)
                .ok_or(GraphError::ArithmeticOverflow {
                    field: "union_find_compress",
                })? = root;
            index = next;
        }
        Ok(root)
    }

    fn union(&mut self, left: usize, right: usize) -> Result<bool, GraphError> {
        let left_root = self.find(left)?;
        let right_root = self.find(right)?;
        if left_root == right_root {
            return Ok(false);
        }
        let left_rank = *self
            .rank
            .get(left_root)
            .ok_or(GraphError::ArithmeticOverflow {
                field: "union_rank",
            })?;
        let right_rank = *self
            .rank
            .get(right_root)
            .ok_or(GraphError::ArithmeticOverflow {
                field: "union_rank",
            })?;
        let (parent, child) = if left_rank > right_rank
            || (left_rank == right_rank && left_root < right_root)
        {
            (left_root, right_root)
        } else {
            (right_root, left_root)
        };
        *self
            .parent
            .get_mut(child)
            .ok_or(GraphError::ArithmeticOverflow {
                field: "union_parent",
            })? = parent;
        if left_rank == right_rank {
            let rank = self
                .rank
                .get_mut(parent)
                .ok_or(GraphError::ArithmeticOverflow {
                    field: "union_rank",
                })?;
            *rank = rank
                .checked_add(1)
                .ok_or(GraphError::ArithmeticOverflow {
                    field: "union_rank",
                })?;
        }
        Ok(true)
    }
}
