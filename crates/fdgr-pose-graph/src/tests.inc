#[cfg(test)]
mod tests {
    use super::*;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis() -> PoseGraphBasis {
        PoseGraphBasis {
            node_basis_digest: digest(1),
            edge_basis_digest: digest(2),
            graph_selection_policy_digest: digest(3),
            rotation_policy_digest: digest(4),
            generation: 1,
        }
    }

    fn node(id: u64, sample: u64) -> PoseNode {
        PoseNode {
            node_id: id,
            sample_index: sample,
            keyframe_digest: digest(u8::try_from(id.saturating_add(10)).map_or(200, |v| v)),
        }
    }

    fn edge(
        id: u64,
        left: u64,
        right: u64,
        left_sample: u64,
        right_sample: u64,
        rotation: [i64; 9],
        support: u32,
    ) -> RelativePoseEdge {
        RelativePoseEdge {
            edge_id: id,
            verification_digest: digest(u8::try_from(id.saturating_add(40)).map_or(220, |v| v)),
            admitted_candidate_id: id,
            left_node_id: left,
            right_node_id: right,
            left_sample_index: left_sample,
            right_sample_index: right_sample,
            rotation_right_from_left_nano: rotation,
            translation_right_from_left_direction_nano: [UNIT_NANO, 0, 0],
            supported_match_count: support,
            median_residual_nano: u64::from(100_u32.saturating_sub(support.min(100))),
        }
    }

    fn identity() -> [i64; 9] {
        identity_rotation()
    }

    fn half_turn_z() -> [i64; 9] {
        [-UNIT_NANO, 0, 0, 0, -UNIT_NANO, 0, 0, 0, UNIT_NANO]
    }

    #[test]
    fn consistent_triangle_yields_cycle_consistent_orientation_only() {
        let value = build_pose_graph(
            basis(),
            PoseGraphPolicy::default(),
            vec![node(3, 30), node(1, 10), node(2, 20)],
            vec![
                edge(3, 1, 3, 10, 30, identity(), 80),
                edge(1, 1, 2, 10, 20, identity(), 100),
                edge(2, 2, 3, 20, 30, identity(), 90),
            ],
        );
        assert!(matches!(
            value,
            Ok(ref generation)
                if generation.components.len() == 1
                    && generation.components[0].orientation_status == OrientationStatus::CycleConsistent
                    && generation.cycle_assessments.len() == 1
                    && generation.cycle_assessments[0].status == RotationCycleStatus::Consistent
                    && generation.cycle_assessments[0].rotation_residual_ppm == 0
                    && generation.translation_status == TranslationStatus::DirectionOnlyScaleUnderdetermined
                    && generation.validate().is_ok()
                    && generation.digest().is_ok()
                    && matches!(generation.to_json(), Ok(ref json)
                        if json.contains("direction_only_scale_underdetermined")
                            && !json.contains("camera_center")
                            && !json.contains("position_nano"))
        ));
    }

    #[test]
    fn inconsistent_redundant_edge_remains_explicit_conflict() {
        let value = build_pose_graph(
            basis(),
            PoseGraphPolicy::default(),
            vec![node(1, 10), node(2, 20), node(3, 30)],
            vec![
                edge(1, 1, 2, 10, 20, identity(), 100),
                edge(2, 2, 3, 20, 30, identity(), 90),
                edge(3, 1, 3, 10, 30, half_turn_z(), 80),
            ],
        );
        assert!(matches!(
            value,
            Ok(ref generation)
                if generation.components[0].orientation_status == OrientationStatus::Conflicted
                    && generation.components[0].conflicting_edge_ids == vec![3]
                    && generation.cycle_assessments[0].status == RotationCycleStatus::Conflicting
        ));
    }

    #[test]
    fn disconnected_nodes_have_independent_gauges() {
        let value = build_pose_graph(
            basis(),
            PoseGraphPolicy::default(),
            vec![node(1, 10), node(2, 20), node(4, 40)],
            vec![edge(1, 1, 2, 10, 20, identity(), 10)],
        );
        assert!(matches!(
            value,
            Ok(ref generation)
                if generation.components.len() == 2
                    && generation.components.iter().all(|component|
                        component.orientation_status == OrientationStatus::TreeOnly)
                    && generation.orientations.iter().find(|value| value.node_id == 4)
                        .is_some_and(|value| value.component_root_node_id == 4
                            && value.parent_node_id.is_none()
                            && value.rotation_node_from_root_nano == identity())
        ));
    }

    #[test]
    fn reverse_forest_traversal_uses_inverse_rotation() {
        let quarter_turn_z = [0, -UNIT_NANO, 0, UNIT_NANO, 0, 0, 0, 0, UNIT_NANO];
        let value = build_pose_graph(
            basis(),
            PoseGraphPolicy::default(),
            vec![node(1, 10), node(2, 20), node(3, 30)],
            vec![
                edge(1, 2, 3, 20, 30, quarter_turn_z, 100),
                edge(2, 1, 3, 10, 30, identity(), 90),
            ],
        );
        assert!(matches!(
            value,
            Ok(ref generation)
                if generation.orientations.iter().find(|value| value.node_id == 2)
                    .is_some_and(|value| value.rotation_node_from_root_nano == transpose_rotation(quarter_turn_z))
        ));
    }

    #[test]
    fn input_order_and_larger_successful_budget_preserve_identity() {
        let nodes = vec![node(1, 10), node(2, 20), node(3, 30)];
        let edges = vec![
            edge(1, 1, 2, 10, 20, identity(), 100),
            edge(2, 2, 3, 20, 30, identity(), 90),
            edge(3, 1, 3, 10, 30, identity(), 80),
        ];
        let mut reversed_nodes = nodes.clone();
        reversed_nodes.reverse();
        let mut reversed_edges = edges.clone();
        reversed_edges.reverse();
        let mut low = PoseGraphPolicy::default();
        low.max_path_expansions = 100;
        let mut high = low;
        high.max_path_expansions = 100_000;
        let left = build_pose_graph(basis(), low, nodes, edges);
        let right = build_pose_graph(basis(), high, reversed_nodes, reversed_edges);
        assert!(matches!(
            (left, right),
            (Ok(left), Ok(right))
                if left.digest().is_ok() && left.digest() == right.digest()
        ));
    }

    #[test]
    fn sample_mismatch_and_nonunit_direction_fail_closed() {
        let mismatch = build_pose_graph(
            basis(),
            PoseGraphPolicy::default(),
            vec![node(1, 10), node(2, 20)],
            vec![edge(1, 1, 2, 11, 20, identity(), 10)],
        );
        assert!(matches!(mismatch, Err(PoseGraphError::SampleMismatch { .. })));
        let mut invalid = edge(1, 1, 2, 10, 20, identity(), 10);
        invalid.translation_right_from_left_direction_nano = [1, 0, 0];
        assert!(matches!(
            build_pose_graph(
                basis(),
                PoseGraphPolicy::default(),
                vec![node(1, 10), node(2, 20)],
                vec![invalid]
            ),
            Err(PoseGraphError::TranslationDirectionInvalid { .. })
        ));
    }
}
