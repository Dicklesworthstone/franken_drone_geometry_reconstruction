#[cfg(test)]
mod tests {
    use super::{
        BUNDLE_PROBLEM_AUTHORITY, BundleCameraBinding, BundleLandmarkSeed,
        BundleObservation, BundleObservationDisposition, BundleObservationRole,
        BundleProblemBasis, BundleProblemComponentStatus, BundleProblemDecision,
        BundleProblemError, BundleProblemPolicy, bundle_observation_table_digest,
        bundle_problem_policy_digest, camera_binding_table_digest, compile_bundle_problem,
        landmark_seed_table_digest,
    };
    use fdgr_edge_scale::{
        EdgeRatioSource, EdgeRatioWitness, EdgeScaleBasis, EdgeScalePolicy,
        derive_edge_scale_subjects, edge_witness_table_digest,
        reconcile_pose_graph_edge_scales,
    };
    use fdgr_global_pose::{
        GlobalPoseBasis, GlobalPosePolicy, initialize_global_pose,
    };
    use fdgr_pose_graph::{
        PoseGraphBasis, PoseGraphPolicy, PoseNode, RelativePoseEdge, build_pose_graph,
    };
    use fdgr_pose_refinement::{
        PoseRefinementBasis, PoseRefinementPolicy, UNIT_NANO, refine_pose_translation,
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

    fn refinement() -> Result<fdgr_pose_refinement::PoseRefinementGeneration, Box<dyn StdError>> {
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
        let witnesses = vec![
            witness(1, 101, 10, 20, 1, 1),
            witness(2, 102, 20, 30, 1, 2),
        ];
        let (subject_digest, subjects) = derive_edge_scale_subjects(&graph)?;
        let scales = reconcile_pose_graph_edge_scales(
            EdgeScaleBasis {
                pose_graph_generation_digest: graph.digest()?,
                edge_subject_basis_digest: subject_digest,
                witness_basis_digest: edge_witness_table_digest(&subjects, &witnesses)?,
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
        Ok(refine_pose_translation(
            PoseRefinementBasis {
                initialization_digest: initialization.digest()?,
                policy_digest: digest(46),
                generation: 1,
            },
            PoseRefinementPolicy::default(),
            initialization,
        )?)
    }

    fn bindings() -> Vec<BundleCameraBinding> {
        vec![
            BundleCameraBinding {
                camera_node_id: 1,
                sample_index: 10,
                frame_digest: digest(101),
                effective_calibration_digest: digest(111),
            },
            BundleCameraBinding {
                camera_node_id: 2,
                sample_index: 20,
                frame_digest: digest(102),
                effective_calibration_digest: digest(112),
            },
            BundleCameraBinding {
                camera_node_id: 3,
                sample_index: 30,
                frame_digest: digest(103),
                effective_calibration_digest: digest(113),
            },
        ]
    }

    fn seed(
        landmark_id: u64,
        source_track_id: u64,
        scale_root: Option<u64>,
    ) -> BundleLandmarkSeed {
        BundleLandmarkSeed {
            landmark_id,
            source_track_id,
            component_root_node_id: 1,
            scale_component_root_edge_id: scale_root,
            seed_evidence_digest: digest(u8::try_from(landmark_id).map_or(200, |value| value)),
            seed_position_from_root_nano: [0, 0, 5 * UNIT_NANO],
            seed_uncertainty_nano: 100_000,
        }
    }

    fn observation(
        observation_id: u64,
        landmark_id: u64,
        camera_node_id: u64,
        role: BundleObservationRole,
    ) -> BundleObservation {
        let (sample_index, frame_byte) = match camera_node_id {
            1 => (10, 101),
            2 => (20, 102),
            _ => (30, 103),
        };
        BundleObservation {
            observation_id,
            landmark_id,
            camera_node_id,
            sample_index,
            frame_digest: digest(frame_byte),
            source_feature_observation_id: observation_id.saturating_add(1_000),
            evidence_digest: digest(u8::try_from(observation_id).map_or(201, |value| value)),
            x_nano_pixels: 100 * UNIT_NANO,
            y_nano_pixels: 80 * UNIT_NANO,
            localization_uncertainty_nano_pixels: 100_000,
            dynamic_masked: false,
            role,
        }
    }

    fn basis(
        refinement: &fdgr_pose_refinement::PoseRefinementGeneration,
        policy: BundleProblemPolicy,
        bindings: &[BundleCameraBinding],
        seeds: &[BundleLandmarkSeed],
        observations: &[BundleObservation],
        generation: u64,
    ) -> Result<BundleProblemBasis, BundleProblemError> {
        Ok(BundleProblemBasis {
            pose_refinement_digest: refinement.digest()?,
            camera_binding_basis_digest: camera_binding_table_digest(bindings)?,
            landmark_seed_basis_digest: landmark_seed_table_digest(seeds)?,
            observation_basis_digest: bundle_observation_table_digest(observations)?,
            policy_digest: bundle_problem_policy_digest(policy)?,
            generation,
        })
    }

    fn redundant_inputs(
        refinement: &fdgr_pose_refinement::PoseRefinementGeneration,
    ) -> Result<
        (
            BundleProblemPolicy,
            Vec<BundleCameraBinding>,
            Vec<BundleLandmarkSeed>,
            Vec<BundleObservation>,
        ),
        BundleProblemError,
    > {
        let scale_root = refinement
            .components
            .first()
            .and_then(|component| component.scale_component_root_edge_id);
        let policy = BundleProblemPolicy::default();
        let bindings = bindings();
        let seeds = vec![
            seed(100, 500, scale_root),
            seed(200, 600, scale_root),
            seed(300, 700, scale_root),
        ];
        let observations = vec![
            observation(1, 100, 1, BundleObservationRole::Optimize),
            observation(2, 100, 2, BundleObservationRole::Optimize),
            observation(3, 200, 1, BundleObservationRole::Optimize),
            observation(4, 200, 2, BundleObservationRole::Optimize),
            observation(5, 300, 1, BundleObservationRole::Optimize),
            observation(6, 300, 2, BundleObservationRole::Optimize),
            observation(7, 100, 3, BundleObservationRole::HeldOut),
            observation(8, 200, 3, BundleObservationRole::HeldOut),
            observation(9, 300, 3, BundleObservationRole::HeldOut),
        ];
        Ok((policy, bindings, seeds, observations))
    }

    #[test]
    fn redundant_problem_is_structurally_admitted_without_geometry_authority(
    ) -> Result<(), Box<dyn StdError>> {
        let refinement = refinement()?;
        let (policy, bindings, seeds, observations) = redundant_inputs(&refinement)?;
        let value = compile_bundle_problem(
            basis(
                &refinement,
                policy,
                &bindings,
                &seeds,
                &observations,
                1,
            )?,
            policy,
            refinement,
            bindings,
            seeds,
            observations,
        )?;
        assert_eq!(value.components.len(), 1);
        assert_eq!(
            value.components[0].status,
            BundleProblemComponentStatus::Redundant
        );
        assert_eq!(
            value.components[0].decision,
            BundleProblemDecision::Admit
        );
        assert_eq!(value.components[0].cycle_rank, 2);
        assert!(value.components[0].bridge_observation_ids.is_empty());
        assert_eq!(
            value.components[0].eligible_held_out_camera_node_ids,
            vec![3]
        );
        assert!(value.validate().is_ok());
        let json = value.to_json()?;
        assert!(json.contains(BUNDLE_PROBLEM_AUTHORITY));
        assert!(json.contains("\"rank_authority\":\"planning_count_only\""));
        assert!(!json.contains("meter"));
        assert!(!json.contains("optimized_landmark"));
        Ok(())
    }

    #[test]
    fn missing_held_out_evidence_is_diagnostic_not_admitted(
    ) -> Result<(), Box<dyn StdError>> {
        let refinement = refinement()?;
        let (policy, bindings, seeds, mut observations) = redundant_inputs(&refinement)?;
        observations.retain(|observation| observation.role == BundleObservationRole::Optimize);
        let value = compile_bundle_problem(
            basis(
                &refinement,
                policy,
                &bindings,
                &seeds,
                &observations,
                2,
            )?,
            policy,
            refinement,
            bindings,
            seeds,
            observations,
        )?;
        assert_eq!(
            value.components[0].status,
            BundleProblemComponentStatus::MissingHeldOutEvidence
        );
        assert_eq!(
            value.components[0].decision,
            BundleProblemDecision::AdmitDiagnostic
        );
        Ok(())
    }

    #[test]
    fn support_pruning_reaches_a_fixed_point_and_preserves_dispositions(
    ) -> Result<(), Box<dyn StdError>> {
        let refinement = refinement()?;
        let scale_root = refinement.components[0].scale_component_root_edge_id;
        let policy = BundleProblemPolicy {
            min_root_active_landmarks: 1,
            min_held_out_observations_per_component: 1,
            ..BundleProblemPolicy::default()
        };
        let bindings = bindings();
        let seeds = vec![seed(100, 500, scale_root), seed(200, 600, scale_root)];
        let observations = vec![
            observation(1, 100, 1, BundleObservationRole::Optimize),
            observation(2, 100, 2, BundleObservationRole::Optimize),
            observation(3, 200, 2, BundleObservationRole::Optimize),
            observation(4, 200, 3, BundleObservationRole::Optimize),
        ];
        let value = compile_bundle_problem(
            basis(
                &refinement,
                policy,
                &bindings,
                &seeds,
                &observations,
                3,
            )?,
            policy,
            refinement,
            bindings,
            seeds,
            observations,
        )?;
        assert!(value.pruning_rounds >= 3);
        assert_eq!(
            value.components[0].status,
            BundleProblemComponentStatus::NoTrainingCore
        );
        assert!(value.components[0].active_landmark_ids.is_empty());
        assert!(value.observations.iter().all(|observation| matches!(
            observation.disposition,
            BundleObservationDisposition::OptimizePrunedLandmarkSupport
                | BundleObservationDisposition::OptimizePrunedCameraSupport
        )));
        Ok(())
    }

    #[test]
    fn unobserved_gauge_root_blocks_a_disconnected_training_island(
    ) -> Result<(), Box<dyn StdError>> {
        let refinement = refinement()?;
        let scale_root = refinement.components[0].scale_component_root_edge_id;
        let policy = BundleProblemPolicy {
            min_active_landmarks_per_camera: 2,
            min_root_active_landmarks: 1,
            min_held_out_observations_per_component: 1,
            ..BundleProblemPolicy::default()
        };
        let bindings = bindings();
        let seeds = vec![seed(100, 500, scale_root), seed(200, 600, scale_root)];
        let observations = vec![
            observation(1, 100, 2, BundleObservationRole::Optimize),
            observation(2, 100, 3, BundleObservationRole::Optimize),
            observation(3, 200, 2, BundleObservationRole::Optimize),
            observation(4, 200, 3, BundleObservationRole::Optimize),
        ];
        let value = compile_bundle_problem(
            basis(
                &refinement,
                policy,
                &bindings,
                &seeds,
                &observations,
                4,
            )?,
            policy,
            refinement,
            bindings,
            seeds,
            observations,
        )?;
        assert_eq!(
            value.components[0].status,
            BundleProblemComponentStatus::GaugeRootUnobserved
        );
        assert!(!value.components[0].root_connected);
        assert_eq!(value.components[0].decision, BundleProblemDecision::Block);
        Ok(())
    }

    #[test]
    fn camera_landmark_pair_cannot_train_and_self_adjudicate(
    ) -> Result<(), Box<dyn StdError>> {
        let refinement = refinement()?;
        let scale_root = refinement.components[0].scale_component_root_edge_id;
        let policy = BundleProblemPolicy::default();
        let bindings = bindings();
        let seeds = vec![seed(100, 500, scale_root)];
        let observations = vec![
            observation(1, 100, 1, BundleObservationRole::Optimize),
            observation(2, 100, 1, BundleObservationRole::HeldOut),
        ];
        assert!(matches!(
            basis(
                &refinement,
                policy,
                &bindings,
                &seeds,
                &observations,
                5
            ),
            Err(BundleProblemError::DuplicateCameraLandmarkPair {
                camera_node_id: 1,
                landmark_id: 100,
            })
        ));
        Ok(())
    }

    #[test]
    fn stale_basis_and_small_operation_budget_fail_closed(
    ) -> Result<(), Box<dyn StdError>> {
        let refinement = refinement()?;
        let (policy, bindings, seeds, observations) = redundant_inputs(&refinement)?;
        let mut stale = basis(
            &refinement,
            policy,
            &bindings,
            &seeds,
            &observations,
            6,
        )?;
        stale.observation_basis_digest = digest(250);
        assert!(matches!(
            compile_bundle_problem(
                stale,
                policy,
                refinement.clone(),
                bindings.clone(),
                seeds.clone(),
                observations.clone(),
            ),
            Err(BundleProblemError::BasisMismatch {
                field: "observation_basis_digest",
                ..
            })
        ));
        let small_policy = BundleProblemPolicy {
            max_operations: 1,
            ..policy
        };
        assert!(matches!(
            compile_bundle_problem(
                basis(
                    &refinement,
                    small_policy,
                    &bindings,
                    &seeds,
                    &observations,
                    7,
                )?,
                small_policy,
                refinement,
                bindings,
                seeds,
                observations,
            ),
            Err(BundleProblemError::OperationBudgetExceeded { .. })
        ));
        Ok(())
    }

    #[test]
    fn larger_successful_execution_ceilings_preserve_semantic_identity(
    ) -> Result<(), Box<dyn StdError>> {
        let refinement = refinement()?;
        let (mut small_policy, bindings, seeds, observations) = redundant_inputs(&refinement)?;
        small_policy.max_operations = 10_000;
        small_policy.max_graph_path_expansions = 100;
        let large_policy = BundleProblemPolicy {
            max_operations: 100_000_000,
            max_graph_path_expansions: 10_000_000,
            ..small_policy
        };
        let small = compile_bundle_problem(
            basis(
                &refinement,
                small_policy,
                &bindings,
                &seeds,
                &observations,
                8,
            )?,
            small_policy,
            refinement.clone(),
            bindings.clone(),
            seeds.clone(),
            observations.clone(),
        )?;
        let large = compile_bundle_problem(
            basis(
                &refinement,
                large_policy,
                &bindings,
                &seeds,
                &observations,
                8,
            )?,
            large_policy,
            refinement,
            bindings,
            seeds,
            observations,
        )?;
        assert_eq!(small.digest()?, large.digest()?);
        assert_ne!(small.policy.max_operations, large.policy.max_operations);
        assert_ne!(
            small.policy.max_graph_path_expansions,
            large.policy.max_graph_path_expansions
        );
        assert_eq!(small.components, large.components);
        Ok(())
    }
}
