#[cfg(test)]
mod tests {
    use super::*;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis() -> GraphBasis {
        GraphBasis {
            node_basis_digest: digest(1),
            edge_basis_digest: digest(2),
            selection_policy_digest: digest(3),
            generation: 1,
        }
    }

    fn node(id: u64) -> GraphNode {
        GraphNode {
            node_id: id,
            evidence_digest: digest(u8::try_from(id).map_or(200, |value| value)),
        }
    }

    fn edge(id: u64, left: u64, right: u64, primary: u64) -> GraphEdge {
        GraphEdge {
            edge_id: id,
            left_node_id: left,
            right_node_id: right,
            priority: EdgePriority {
                primary,
                secondary: id,
            },
            evidence_digest: digest(
                u8::try_from(id.saturating_add(20)).map_or(220, |value| value),
            ),
        }
    }

    #[test]
    fn triangle_has_one_fundamental_cycle_and_no_bridges() {
        let value = analyze_graph(
            basis(),
            GraphBudget::default(),
            vec![node(3), node(1), node(2)],
            vec![
                edge(3, 1, 3, 5),
                edge(1, 1, 2, 10),
                edge(2, 2, 3, 9),
            ],
        );
        assert!(matches!(
            value,
            Ok(ref analysis)
                if analysis.components.len() == 1
                    && analysis.forest_edge_ids == vec![1, 2]
                    && analysis.non_forest_edge_ids == vec![3]
                    && analysis.fundamental_cycles[0].forest_path_edge_ids == vec![1, 2]
                    && analysis.bridge_edge_ids.is_empty()
                    && analysis.cycle_rank == 1
                    && analysis.validate().is_ok()
                    && analysis.digest().is_ok()
        ));
    }

    #[test]
    fn tree_edges_are_bridges_and_disconnected_nodes_remain_explicit() {
        let value = analyze_graph(
            basis(),
            GraphBudget::default(),
            vec![node(1), node(2), node(3), node(4)],
            vec![edge(1, 1, 2, 1), edge(2, 2, 3, 1)],
        );
        assert!(matches!(
            value,
            Ok(ref analysis)
                if analysis.components.len() == 2
                    && analysis.bridge_edge_ids == vec![1, 2]
                    && analysis.cycle_rank == 0
                    && analysis.components[1].node_ids == vec![4]
        ));
    }

    #[test]
    fn input_order_and_larger_successful_budget_preserve_identity() {
        let nodes = vec![node(1), node(2), node(3)];
        let edges = vec![
            edge(1, 1, 2, 10),
            edge(2, 2, 3, 9),
            edge(3, 1, 3, 5),
        ];
        let mut reversed_nodes = nodes.clone();
        reversed_nodes.reverse();
        let mut reversed_edges = edges.clone();
        reversed_edges.reverse();
        let left = analyze_graph(
            basis(),
            GraphBudget {
                max_path_expansions: 10,
            },
            nodes,
            edges,
        );
        let right = analyze_graph(
            basis(),
            GraphBudget {
                max_path_expansions: 1_000,
            },
            reversed_nodes,
            reversed_edges,
        );
        assert!(matches!(
            (left, right),
            (Ok(left), Ok(right))
                if left.digest().is_ok() && left.digest() == right.digest()
        ));
    }

    #[test]
    fn duplicate_endpoint_and_budget_fail_closed() {
        assert!(matches!(
            analyze_graph(
                basis(),
                GraphBudget::default(),
                vec![node(1), node(2)],
                vec![edge(1, 1, 2, 1), edge(2, 1, 2, 2)]
            ),
            Err(GraphError::DuplicateEndpointPair { .. })
        ));
        assert!(matches!(
            analyze_graph(
                basis(),
                GraphBudget {
                    max_path_expansions: 1,
                },
                vec![node(1), node(2), node(3)],
                vec![
                    edge(1, 1, 2, 10),
                    edge(2, 2, 3, 9),
                    edge(3, 1, 3, 5),
                ]
            ),
            Err(GraphError::PathBudgetExceeded { .. })
        ));
    }
}
