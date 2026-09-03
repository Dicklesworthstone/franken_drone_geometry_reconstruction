#[cfg(test)]
mod tests {
    use super::{
        POSE_REFINEMENT_AUTHORITY, PoseFactorDisposition, PoseRefinementBasis,
        PoseRefinementComponentStatus, PoseRefinementDecision, PoseRefinementError,
        PoseRefinementPolicy, UNIT_NANO, refine_pose_translation,
    };
    use fdgr_edge_scale::{
        EdgeRatioSource, EdgeRatioWitness, EdgeScaleBasis, EdgeScalePolicy,
        derive_edge_scale_subjects, edge_witness_table_digest, reconcile_pose_graph_edge_scales,
    };
    use fdgr_global_pose::{
        GlobalPoseBasis, GlobalPoseInitialization, GlobalPosePolicy, initialize_global_pose,
    };
    use fdgr_pose_graph::{
        PoseGraphBasis, PoseGraphPolicy, PoseNode, RelativePoseEdge, build_pose_graph,
    };
    use fdgr_types::EvidenceDigest;
    use std::error::Error as StdError;

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

    fn edge(
        edge_id: u64,
        left_node_id: u64,
        right_node_id: u64,
        left_sample_index: u64,
        right_sample_index: u64,
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
            rotation_right_from_left_nano: identity(),
            translation_right_from_left_direction_nano: [UNIT_NANO, 0, 0],
            supported_match_count: support,
            median_residual_nano: 100,
        }
    }

    const fn identity() -> [i64; 9] {
        [UNIT_NANO, 0, 0, 0, UNIT_NANO, 0, 0, 0, UNIT_NANO]
    }

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

    fn triangle_initialization(
        closing_baseline_matches_chain: bool,
    ) -> Result<GlobalPoseInitialization, Box<dyn StdError>> {
        let graph = build_pose_graph(
            PoseGraphBasis {
                node_basis_digest: digest(40),
                edge_basis_digest: digest(41),
                graph_selection_policy_digest: digest(42),
                rotation_policy_digest: digest(43),
                generation: 1,
            },
            PoseGraphPolicy::default(),
            vec![node(1, 10), node(2, 20), node(3, 30)],
            vec![
                edge(10, 1, 2, 10, 20, 30),
                edge(20, 2, 3, 20, 30, 29),
                edge(30, 1, 3, 10, 30, 10),
            ],
        )?;
        let witnesses = if closing_baseline_matches_chain {
            vec![
                witness(1, 101, 10, 20, 1, 1),
                witness(2, 102, 20, 30, 1, 2),
            ]
        } else {
            vec![
                witness(1, 101, 10, 20, 1, 1),
                witness(2, 102, 20, 30, 1, 1),
            ]
        };
        let (subject_digest, subjects) = derive_edge_scale_subjects(&graph)?;
        let witness_digest = edge_witness_table_digest(&subjects, &witnesses)?;
        let pose_digest = graph.digest()?;
        let scales = reconcile_pose_graph_edge_scales(
            EdgeScaleBasis {
                pose_graph_generation_digest: pose_digest,
                edge_subject_basis_digest: subject_digest,
                witness_basis_digest: witness_digest,
                policy_digest: digest(44),
                generation: 1,
            },
            EdgeScalePolicy {
                min_cross_validation_groups: 1,
                ..EdgeScalePolicy::default()
            },
            &graph,
            witnesses,
        )?;
        let initialization = initialize_global_pose(
            GlobalPoseBasis {
                pose_graph_generation_digest: graph.digest()?,
                edge_scale_generation_digest: scales.digest()?,
                policy_digest: digest(45),
                generation: 1,
            },
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?;
        Ok(initialization)
    }

    fn singleton_initialization() -> Result<GlobalPoseInitialization, Box<dyn StdError>> {
        let graph = build_pose_graph(
            PoseGraphBasis {
                node_basis_digest: digest(50),
                edge_basis_digest: digest(51),
                graph_selection_policy_digest: digest(52),
                rotation_policy_digest: digest(53),
                generation: 1,
            },
            PoseGraphPolicy::default(),
            vec![node(1, 10)],
            Vec::new(),
        )?;
        let (subject_digest, subjects) = derive_edge_scale_subjects(&graph)?;
        let witnesses = Vec::new();
        let scales = reconcile_pose_graph_edge_scales(
            EdgeScaleBasis {
                pose_graph_generation_digest: graph.digest()?,
                edge_subject_basis_digest: subject_digest,
                witness_basis_digest: edge_witness_table_digest(&subjects, &witnesses)?,
                policy_digest: digest(54),
                generation: 1,
            },
            EdgeScalePolicy::default(),
            &graph,
            witnesses,
        )?;
        Ok(initialize_global_pose(
            GlobalPoseBasis {
                pose_graph_generation_digest: graph.digest()?,
                edge_scale_generation_digest: scales.digest()?,
                policy_digest: digest(55),
                generation: 1,
            },
            GlobalPosePolicy::default(),
            graph,
            scales,
        )?)
    }

    fn refinement_basis(
        initialization: &GlobalPoseInitialization,
        generation: u64,
    ) -> Result<PoseRefinementBasis, Box<dyn StdError>> {
        Ok(PoseRefinementBasis {
            initialization_digest: initialization.digest()?,
            policy_digest: digest(60),
            generation,
        })
    }

    #[test]
    fn conflicting_triangle_relaxes_translation_without_upgrading_authority(
    ) -> Result<(), Box<dyn StdError>> {
        let initialization = triangle_initialization(false)?;
        let initial_node_three = initialization
            .poses
            .iter()
            .find(|pose| pose.node_id == 3)
            .map(|pose| pose.camera_center_from_root_nano);
        assert_eq!(initial_node_three, Some([-2 * UNIT_NANO, 0, 0]));
        let value = refine_pose_translation(
            refinement_basis(&initialization, 1)?,
            PoseRefinementPolicy::default(),
            initialization,
        )?;
        assert_eq!(value.components.len(), 1);
        assert_eq!(
            value.components[0].decision,
            PoseRefinementDecision::AcceptRefined
        );
        assert!(matches!(
            value.components[0].status,
            PoseRefinementComponentStatus::Converged
                | PoseRefinementComponentStatus::IterationLimit
                | PoseRefinementComponentStatus::NoImprovement
        ));
        assert!(matches!(
            (
                value.components[0].initial_rms_residual_nano,
                value.components[0].final_rms_residual_nano,
            ),
            (Some(initial), Some(final_value)) if final_value < initial
        ));
        assert!(!value.components[0].do_nothing_dominates);
        assert_eq!(value.components[0].downweighted_edge_ids, vec![30]);
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 1
                && pose.root_pinned
                && pose.refined_camera_center_from_root_nano == [0, 0, 0]
        }));
        assert!(value.poses.iter().any(|pose| {
            pose.node_id == 3
                && pose.refined_camera_center_from_root_nano[0] > -2 * UNIT_NANO
                && pose.refined_camera_center_from_root_nano[0] < -UNIT_NANO
        }));
        assert!(value.factors.iter().any(|factor| {
            factor.edge_id == 30
                && factor.robust_weight < factor.base_weight
                && factor.disposition == PoseFactorDisposition::ActiveDownweighted
                && matches!(
                    (factor.initial_residual_nano, factor.final_residual_nano),
                    (Some(initial), Some(final_value)) if final_value < initial
                )
        }));
        assert!(value.validate().is_ok());
        let json = value.to_json()?;
        assert!(json.contains(POSE_REFINEMENT_AUTHORITY));
        assert!(json.contains("\"decision\":\"accept_refined\""));
        assert!(!json.contains("meter"));
        assert!(!json.contains("bundle_adjusted"));
        Ok(())
    }

    #[test]
    fn exactly_consistent_triangle_retains_initialization(
    ) -> Result<(), Box<dyn StdError>> {
        let initialization = triangle_initialization(true)?;
        let value = refine_pose_translation(
            refinement_basis(&initialization, 2)?,
            PoseRefinementPolicy::default(),
            initialization,
        )?;
        assert_eq!(
            value.components[0].status,
            PoseRefinementComponentStatus::AlreadySatisfied
        );
        assert_eq!(
            value.components[0].decision,
            PoseRefinementDecision::RetainInitialization
        );
        assert!(value.components[0].do_nothing_dominates);
        assert!(value
            .poses
            .iter()
            .all(|pose| pose.adjustment_nano == [0, 0, 0]));
        assert_eq!(value.iteration_count, 0);
        Ok(())
    }

    #[test]
    fn singleton_remains_a_pinned_zero_origin() -> Result<(), Box<dyn StdError>> {
        let initialization = singleton_initialization()?;
        let value = refine_pose_translation(
            refinement_basis(&initialization, 3)?,
            PoseRefinementPolicy::default(),
            initialization,
        )?;
        assert_eq!(value.poses.len(), 1);
        assert!(value.factors.is_empty());
        assert_eq!(
            value.components[0].status,
            PoseRefinementComponentStatus::Singleton
        );
        assert!(value.poses[0].root_pinned);
        assert_eq!(
            value.poses[0].refined_camera_center_from_root_nano,
            [0, 0, 0]
        );
        Ok(())
    }

    #[test]
    fn larger_successful_operation_budget_preserves_identity(
    ) -> Result<(), Box<dyn StdError>> {
        let initialization = triangle_initialization(false)?;
        let basis = refinement_basis(&initialization, 4)?;
        let mut small = PoseRefinementPolicy::default();
        small.max_operations = 100_000;
        let mut large = small;
        large.max_operations = 10_000_000;
        let left = refine_pose_translation(basis.clone(), small, initialization.clone())?;
        let right = refine_pose_translation(basis, large, initialization)?;
        assert_eq!(left.digest()?, right.digest()?);
        assert_ne!(left.policy.max_operations, right.policy.max_operations);
        assert_eq!(left.poses, right.poses);
        assert_eq!(left.factors, right.factors);
        assert_eq!(left.components, right.components);
        Ok(())
    }

    #[test]
    fn stale_initialization_identity_is_refused() -> Result<(), Box<dyn StdError>> {
        let initialization = triangle_initialization(false)?;
        let result = refine_pose_translation(
            PoseRefinementBasis {
                initialization_digest: digest(99),
                policy_digest: digest(60),
                generation: 5,
            },
            PoseRefinementPolicy::default(),
            initialization,
        );
        assert!(matches!(
            result,
            Err(PoseRefinementError::BasisMismatch { .. })
        ));
        Ok(())
    }

    #[test]
    fn position_and_operation_bounds_fail_closed() -> Result<(), Box<dyn StdError>> {
        let initialization = triangle_initialization(false)?;
        let basis = refinement_basis(&initialization, 6)?;
        let mut position_policy = PoseRefinementPolicy::default();
        position_policy.max_camera_center_abs_nano = UNIT_NANO.unsigned_abs();
        assert!(matches!(
            refine_pose_translation(
                basis.clone(),
                position_policy,
                initialization.clone()
            ),
            Err(PoseRefinementError::PositionBoundExceeded {
                node_id: 3,
                ..
            })
        ));
        let mut operation_policy = PoseRefinementPolicy::default();
        operation_policy.max_operations = 1;
        assert!(matches!(
            refine_pose_translation(basis, operation_policy, initialization),
            Err(PoseRefinementError::OperationBudgetExceeded { .. })
        ));
        Ok(())
    }
}
