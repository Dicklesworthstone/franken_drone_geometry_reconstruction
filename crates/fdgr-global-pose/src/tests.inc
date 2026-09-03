#[cfg(test)]
mod tests {
    use super::{
        GLOBAL_POSE_AUTHORITY, GlobalPoseBasis, GlobalPoseComponentStatus, GlobalPoseError,
        GlobalPosePolicy, TranslationCycleStatus, UNIT_NANO, initialize_global_pose,
    };
    use fdgr_edge_scale::{
        EdgeRatioSource, EdgeRatioWitness, EdgeScaleBasis, EdgeScalePolicy,
        derive_edge_scale_subjects, edge_witness_table_digest, reconcile_pose_graph_edge_scales,
    };
    use fdgr_pose_graph::{
        PoseGraphBasis, PoseGraphPolicy, PoseNode, RelativePoseEdge, build_pose_graph,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn node(node_id: u64, sample_index: u64) -> PoseNode {
        PoseNode {
            node_id,
            sample_index,
            keyframe_digest: digest(u8::try_from(node_id).map_or(250, |value| value)),
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn edge(
        edge_id: u64,
        left_node_id: u64,
        right_node_id: u64,
        left_sample_index: u64,
        right_sample_index: u64,
        rotation: [i64; 9],
        translation: [i64; 3],
        support: u32,
    ) -> RelativePoseEdge {
        RelativePoseEdge {
            edge_id,
            verification_digest: digest(u8::try_from(edge_id).map_or(249, |value| value)),
            admitted_candidate_id: 1,
            left_node_id,
            right_node_id,
            left_sample_index,
            right_sample_index,
            rotation_right_from_left_nano: rotation,
            translation_right_from_left_direction_nano: translation,
            supported_match_count: support,
            median_residual_nano: 100,
        }
    }

    const fn identity() -> [i64; 9] {
        [UNIT_NANO, 0, 0, 0, UNIT_NANO, 0, 0, 0, UNIT_NANO]
    }

    fn pose_graph(
        nodes: Vec<PoseNode>,
        edges: Vec<RelativePoseEdge>,
    ) -> Result<fdgr_pose_graph::PoseGraphGeneration, GlobalPoseError> {
        Ok(build_pose_graph(
            PoseGraphBasis {
                node_basis_digest: digest(40),
                edge_basis_digest: digest(41),
                graph_selection_policy_digest: digest(42),
                rotation_policy_digest: digest(43),
                generation: 1,
            },
            PoseGraphPolicy::default(),
            nodes,
            edges,
        )?)
    }

    #[allow(clippy::too_many_arguments)]
    fn witness(
        witness_id: u64,
        group: u64,
        lower_edge_id: u64,
        higher_edge_id: u64,
        numerator: u64,
        denominator: u64,
    ) -> EdgeRatioWitness {
        EdgeRatioWitness {
            witness_id,
            evidence_digest: digest(
                u8::try_from(witness_id.saturating_add(80)).map_or(248, |value| value),
            ),
            correlation_group_id: group,
            lower_edge_id,
            higher_edge_id,
            ratio_numerator: numerator,
            ratio_denominator: denominator,
            uncertainty_ppm: 1_000,
            support_count: 20,
            source: EdgeRatioSource::SharedTrackGeometry,
        }
    }

    fn edge_scale(
        graph: &fdgr_pose_graph::PoseGraphGeneration,
        witnesses: Vec<EdgeRatioWitness>,
    ) -> Result<fdgr_edge_scale::EdgeScaleGeneration, GlobalPoseError> {
        let (subject_digest, subjects) = derive_edge_scale_subjects(graph)?;
        let witness_digest = edge_witness_table_digest(&subjects, &witnesses)?;
        let pose_digest = graph.digest()?;
        let policy = EdgeScalePolicy {
            min_cross_validation_groups: 1,
            ..EdgeScalePolicy::default()
        };
        Ok(reconcile_pose_graph_edge_scales(
            EdgeScaleBasis {
                pose_graph_generation_digest: pose_digest,
                edge_subject_basis_digest: subject_digest,
                witness_basis_digest: witness_digest,
                policy_digest: digest(44),
                generation: 1,
            },
            policy,
            graph,
            witnesses,
        )?)
    }

    fn global_basis(
        graph: &fdgr_pose_graph::PoseGraphGeneration,
        scales: &fdgr_edge_scale::EdgeScaleGeneration,
        generation: u64,
    ) -> Result<GlobalPoseBasis, GlobalPoseError> {
        Ok(GlobalPoseBasis {
            pose_graph_generation_digest: graph.digest()?,
            edge_scale_generation_digest: scales.digest()?,
            policy_digest: digest(45),
            generation,
        })
    }

    #[test]
    fn consistent_triangle_initializes_one_relative_component() -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20), node(3, 30)],
            vec![
                edge(10, 1, 2, 10, 20, identity(), [UNIT_NANO, 0, 0], 30),
                edge(20, 2, 3, 20, 30, identity(), [UNIT_NANO, 0, 0], 29),
                edge(30, 1, 3, 10, 30, identity(), [UNIT_NANO, 0, 0], 10),
            ],
        )?;
        let scales = edge_scale(
            &graph,
            vec![
                witness(1, 101, 10, 20, 1, 1),
                witness(2, 102, 20, 30, 1, 2),
            ],
        )?;
        let value = initialize_global_pose(
            global_basis(&graph, &scales, 1)?,
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        assert_eq!(value.poses.len(), 3);
        assert_eq!(value.components.len(), 1);
        assert_eq!(
            value.components[0].status,
            GlobalPoseComponentStatus::CycleConsistent
        );
        assert_eq!(value.translation_cycles.len(), 1);
        assert_eq!(
            value.translation_cycles[0].status,
            TranslationCycleStatus::Consistent
        );
        assert!(
            value.poses.iter().any(|pose| {
                pose.node_id == 1 && pose.camera_center_from_root_nano == [0, 0, 0]
            })
        );
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 2 && pose.camera_center_from_root_nano == [-UNIT_NANO, 0, 0]
        }));
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 3
                && pose.camera_center_from_root_nano == [-2 * UNIT_NANO, 0, 0]
        }));
        assert!(value.validate().is_ok());
        assert!(matches!(
            value.to_json(),
            Ok(ref json)
                if json.contains(GLOBAL_POSE_AUTHORITY)
                    && json.contains("component_edge_scale_unit_nano")
                    && !json.contains("meter")
                    && !json.contains("bundle_adjusted")
        ));
        Ok(())
    }

    #[test]
    fn contradictory_closing_baseline_remains_a_translation_conflict()
    -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20), node(3, 30)],
            vec![
                edge(10, 1, 2, 10, 20, identity(), [UNIT_NANO, 0, 0], 30),
                edge(20, 2, 3, 20, 30, identity(), [UNIT_NANO, 0, 0], 29),
                edge(30, 1, 3, 10, 30, identity(), [UNIT_NANO, 0, 0], 10),
            ],
        )?;
        let scales = edge_scale(
            &graph,
            vec![
                witness(1, 101, 10, 20, 1, 1),
                witness(2, 102, 20, 30, 1, 1),
            ],
        )?;
        let value = initialize_global_pose(
            global_basis(&graph, &scales, 2)?,
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        assert_eq!(
            value.components[0].status,
            GlobalPoseComponentStatus::TranslationConflicted
        );
        assert_eq!(
            value.translation_cycles[0].status,
            TranslationCycleStatus::Conflicting
        );
        assert!(
            value.translation_cycles[0]
                .residual_ppm
                .is_some_and(|value| value > 50_000)
        );
        Ok(())
    }

    #[test]
    fn unrelated_forest_scale_gauges_are_refused() -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20), node(3, 30)],
            vec![
                edge(10, 1, 2, 10, 20, identity(), [UNIT_NANO, 0, 0], 30),
                edge(20, 2, 3, 20, 30, identity(), [UNIT_NANO, 0, 0], 29),
            ],
        )?;
        let scales = edge_scale(&graph, Vec::new())?;
        let basis = global_basis(&graph, &scales, 3)?;
        assert!(matches!(
            initialize_global_pose(basis, GlobalPosePolicy::default(), graph, scales),
            Err(GlobalPoseError::IncomparableScaleComponents {
                component_root_node_id: 1,
                ..
            })
        ));
        Ok(())
    }

    #[test]
    fn incomparable_closing_gauge_remains_explicitly_unresolved() -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20), node(3, 30)],
            vec![
                edge(10, 1, 2, 10, 20, identity(), [UNIT_NANO, 0, 0], 30),
                edge(20, 2, 3, 20, 30, identity(), [UNIT_NANO, 0, 0], 29),
                edge(30, 1, 3, 10, 30, identity(), [UNIT_NANO, 0, 0], 10),
            ],
        )?;
        let scales = edge_scale(&graph, vec![witness(1, 101, 10, 20, 1, 1)])?;
        let value = initialize_global_pose(
            global_basis(&graph, &scales, 4)?,
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        assert_eq!(value.translation_cycles.len(), 1);
        assert_eq!(
            value.translation_cycles[0].status,
            TranslationCycleStatus::IncomparableScaleGauge
        );
        assert_eq!(value.translation_cycles[0].measured_displacement_nano, None);
        assert_eq!(value.translation_cycles[0].residual_ppm, None);
        assert_eq!(
            value.components[0].status,
            GlobalPoseComponentStatus::TreeInitialized
        );
        assert_eq!(value.components[0].incomparable_cycle_edge_ids, vec![30]);
        assert!(value.components[0].conflicting_edge_ids.is_empty());
        assert!(value.validate().is_ok());
        Ok(())
    }

    #[test]
    fn camera_center_uses_negative_transposed_rotation() -> Result<(), GlobalPoseError> {
        let quarter_turn_z = [0, -UNIT_NANO, 0, UNIT_NANO, 0, 0, 0, 0, UNIT_NANO];
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20)],
            vec![edge(
                10,
                1,
                2,
                10,
                20,
                quarter_turn_z,
                [UNIT_NANO, 0, 0],
                30,
            )],
        )?;
        let scales = edge_scale(&graph, Vec::new())?;
        let value = initialize_global_pose(
            global_basis(&graph, &scales, 5)?,
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 2 && pose.camera_center_from_root_nano == [0, UNIT_NANO, 0]
        }));
        Ok(())
    }

    #[test]
    fn graph_node_order_does_not_reverse_physical_transform_direction()
    -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(9, 10), node(2, 20)],
            vec![edge(
                10,
                9,
                2,
                10,
                20,
                identity(),
                [UNIT_NANO, 0, 0],
                30,
            )],
        )?;
        let scales = edge_scale(&graph, Vec::new())?;
        let value = initialize_global_pose(
            global_basis(&graph, &scales, 6)?,
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        assert!(
            value.poses.iter().any(|pose| {
                pose.node_id == 2 && pose.camera_center_from_root_nano == [0, 0, 0]
            })
        );
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 9 && pose.camera_center_from_root_nano == [UNIT_NANO, 0, 0]
        }));
        Ok(())
    }

    #[test]
    fn disconnected_components_keep_independent_origins_and_scale_gauges()
    -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20), node(10, 100), node(11, 110)],
            vec![
                edge(10, 1, 2, 10, 20, identity(), [UNIT_NANO, 0, 0], 30),
                edge(
                    20,
                    10,
                    11,
                    100,
                    110,
                    identity(),
                    [0, UNIT_NANO, 0],
                    29,
                ),
            ],
        )?;
        let scales = edge_scale(&graph, Vec::new())?;
        let value = initialize_global_pose(
            global_basis(&graph, &scales, 7)?,
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        assert_eq!(value.components.len(), 2);
        assert_eq!(value.poses.len(), 4);
        assert!(value.translation_cycles.is_empty());
        assert!(value.components.iter().any(|component| {
            component.component_root_node_id == 1
                && component.scale_component_root_edge_id == Some(10)
                && component.status == GlobalPoseComponentStatus::TreeInitialized
        }));
        assert!(value.components.iter().any(|component| {
            component.component_root_node_id == 10
                && component.scale_component_root_edge_id == Some(20)
                && component.status == GlobalPoseComponentStatus::TreeInitialized
        }));
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 1
                && pose.component_root_node_id == 1
                && pose.camera_center_from_root_nano == [0, 0, 0]
        }));
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 10
                && pose.component_root_node_id == 10
                && pose.camera_center_from_root_nano == [0, 0, 0]
        }));
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 2
                && pose.scale_component_root_edge_id == Some(10)
                && pose.camera_center_from_root_nano == [-UNIT_NANO, 0, 0]
        }));
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 11
                && pose.scale_component_root_edge_id == Some(20)
                && pose.camera_center_from_root_nano == [0, -UNIT_NANO, 0]
        }));
        assert!(value.validate().is_ok());
        Ok(())
    }

    #[test]
    fn singleton_component_has_an_explicit_zero_origin() -> Result<(), GlobalPoseError> {
        let graph = pose_graph(vec![node(1, 10)], Vec::new())?;
        let scales = edge_scale(&graph, Vec::new())?;
        let value = initialize_global_pose(
            global_basis(&graph, &scales, 8)?,
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        assert_eq!(value.poses.len(), 1);
        assert_eq!(value.components.len(), 1);
        assert_eq!(value.components[0].status, GlobalPoseComponentStatus::Singleton);
        assert_eq!(value.components[0].scale_component_root_edge_id, None);
        assert_eq!(value.poses[0].camera_center_from_root_nano, [0, 0, 0]);
        assert_eq!(value.poses[0].scale_component_root_edge_id, None);
        assert_eq!(value.poses[0].parent_node_id, None);
        assert_eq!(value.poses[0].parent_edge_id, None);
        assert!(value.validate().is_ok());
        Ok(())
    }

    #[test]
    fn position_bound_refuses_an_overextended_relative_component() -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20)],
            vec![edge(
                10,
                1,
                2,
                10,
                20,
                identity(),
                [UNIT_NANO, 0, 0],
                30,
            )],
        )?;
        let scales = edge_scale(&graph, Vec::new())?;
        let basis = global_basis(&graph, &scales, 9)?;
        assert!(matches!(
            initialize_global_pose(
                basis,
                GlobalPosePolicy {
                    max_camera_center_abs_nano: 999_999_999,
                    ..GlobalPosePolicy::default()
                },
                graph,
                scales,
            ),
            Err(GlobalPoseError::PositionBoundExceeded {
                node_id: 2,
                actual: 1_000_000_000,
                maximum: 999_999_999,
            })
        ));
        Ok(())
    }

    #[test]
    fn operation_budget_refuses_before_emitting_partial_output() -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20)],
            vec![edge(
                10,
                1,
                2,
                10,
                20,
                identity(),
                [UNIT_NANO, 0, 0],
                30,
            )],
        )?;
        let scales = edge_scale(&graph, Vec::new())?;
        let basis = global_basis(&graph, &scales, 10)?;
        assert!(matches!(
            initialize_global_pose(
                basis,
                GlobalPosePolicy {
                    max_operations: 2,
                    ..GlobalPosePolicy::default()
                },
                graph,
                scales,
            ),
            Err(GlobalPoseError::OperationBudgetExceeded {
                attempted: 3,
                maximum: 2,
            })
        ));
        Ok(())
    }

    #[test]
    fn retained_output_tampering_is_detected_by_deterministic_replay()
    -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20)],
            vec![edge(
                10,
                1,
                2,
                10,
                20,
                identity(),
                [UNIT_NANO, 0, 0],
                30,
            )],
        )?;
        let scales = edge_scale(&graph, Vec::new())?;
        let mut value = initialize_global_pose(
            global_basis(&graph, &scales, 11)?,
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        let Some(pose) = value.poses.iter_mut().find(|pose| pose.node_id == 2) else {
            return Err(GlobalPoseError::InternalLookup {
                field: "tamper_test_pose",
            });
        };
        pose.camera_center_from_root_nano[0] =
            pose.camera_center_from_root_nano[0].saturating_add(1);
        assert!(matches!(
            value.validate(),
            Err(GlobalPoseError::DerivedMismatch {
                field: "global_pose_initialization",
            })
        ));
        assert!(matches!(
            value.digest(),
            Err(GlobalPoseError::DerivedMismatch {
                field: "global_pose_initialization",
            })
        ));
        Ok(())
    }

    #[test]
    fn successful_operation_ceiling_does_not_change_semantic_identity() -> Result<(), GlobalPoseError> {
        let graph = pose_graph(
            vec![node(1, 10), node(2, 20)],
            vec![edge(
                10,
                1,
                2,
                10,
                20,
                identity(),
                [UNIT_NANO, 0, 0],
                30,
            )],
        )?;
        let scales = edge_scale(&graph, Vec::new())?;
        let basis = global_basis(&graph, &scales, 12)?;
        let left = initialize_global_pose(
            basis.clone(),
            GlobalPosePolicy {
                max_operations: 100,
                ..GlobalPosePolicy::default()
            },
            graph.clone(),
            scales.clone(),
        )?;
        let right = initialize_global_pose(
            basis,
            GlobalPosePolicy {
                max_operations: 1_000_000,
                ..GlobalPosePolicy::default()
            },
            graph,
            scales,
        )?;
        assert_eq!(left.poses, right.poses);
        assert_eq!(left.translation_cycles, right.translation_cycles);
        assert_eq!(left.components, right.components);
        assert_eq!(left.digest()?, right.digest()?);
        Ok(())
    }

    #[test]
    fn exact_upstream_identity_is_recomputed() -> Result<(), GlobalPoseError> {
        let graph = pose_graph(vec![node(1, 10)], Vec::new())?;
        let scales = edge_scale(&graph, Vec::new())?;
        let mut basis = global_basis(&graph, &scales, 13)?;
        basis.pose_graph_generation_digest = digest(250);
        assert!(matches!(
            initialize_global_pose(basis, GlobalPosePolicy::default(), graph, scales),
            Err(GlobalPoseError::BasisMismatch {
                field: "pose_graph_generation_digest",
                ..
            })
        ));
        Ok(())
    }
}
