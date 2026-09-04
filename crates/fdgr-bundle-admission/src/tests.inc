#[cfg(test)]
mod tests {
    use super::{
        BUNDLE_ADMISSION_AUTHORITY, BundleAdmissionBasis, BundleAdmissionComponentStatus,
        BundleAdmissionDecision, BundleAdmissionError, BundleAdmissionPolicy, BundleCameraDomain,
        LandmarkSeedProvenance, ObservationAuditDisposition, audit_bundle_admission,
        bundle_admission_policy_digest, camera_domain_table_digest,
        seed_provenance_table_digest,
    };
    use fdgr_bundle_problem::{
        BundleCameraBinding, BundleLandmarkSeed, BundleObservation, BundleObservationRole,
        BundleProblemBasis, BundleProblemDecision, BundleProblemGeneration, BundleProblemPolicy,
        bundle_observation_table_digest, bundle_problem_policy_digest, camera_binding_table_digest,
        compile_bundle_problem, landmark_seed_table_digest,
    };
    use fdgr_edge_scale::{
        EdgeRatioSource, EdgeRatioWitness, EdgeScaleBasis, EdgeScalePolicy,
        derive_edge_scale_subjects, edge_witness_table_digest,
        reconcile_pose_graph_edge_scales,
    };
    use fdgr_global_pose::{GlobalPoseBasis, GlobalPosePolicy, initialize_global_pose};
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

    const fn identity() -> [i64; 9] {
        [UNIT_NANO, 0, 0, 0, UNIT_NANO, 0, 0, 0, UNIT_NANO]
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

    fn compile_problem(
        observations: Vec<BundleObservation>,
        min_held_out_observations: u32,
    ) -> Result<BundleProblemGeneration, Box<dyn StdError>> {
        let refinement = refinement()?;
        let scale_root = refinement
            .components
            .first()
            .and_then(|component| component.scale_component_root_edge_id);
        let policy = BundleProblemPolicy {
            min_held_out_observations_per_component: min_held_out_observations,
            min_held_out_cameras_per_component: 1,
            ..BundleProblemPolicy::default()
        };
        let bindings = bindings();
        let seeds = vec![
            seed(100, 500, scale_root),
            seed(200, 600, scale_root),
            seed(300, 700, scale_root),
        ];
        let basis = BundleProblemBasis {
            pose_refinement_digest: refinement.digest()?,
            camera_binding_basis_digest: camera_binding_table_digest(&bindings)?,
            landmark_seed_basis_digest: landmark_seed_table_digest(&seeds)?,
            observation_basis_digest: bundle_observation_table_digest(&observations)?,
            policy_digest: bundle_problem_policy_digest(policy)?,
            generation: 1,
        };
        Ok(compile_bundle_problem(
            basis,
            policy,
            refinement,
            bindings,
            seeds,
            observations,
        )?)
    }

    fn clean_problem() -> Result<BundleProblemGeneration, Box<dyn StdError>> {
        compile_problem(
            vec![
                observation(1, 100, 1, BundleObservationRole::Optimize),
                observation(2, 100, 2, BundleObservationRole::Optimize),
                observation(3, 100, 3, BundleObservationRole::Optimize),
                observation(4, 200, 1, BundleObservationRole::Optimize),
                observation(5, 200, 2, BundleObservationRole::Optimize),
                observation(6, 200, 3, BundleObservationRole::Optimize),
                observation(7, 300, 1, BundleObservationRole::Optimize),
                observation(8, 300, 2, BundleObservationRole::Optimize),
                observation(9, 300, 3, BundleObservationRole::HeldOut),
            ],
            1,
        )
    }

    fn inactive_held_out_problem() -> Result<BundleProblemGeneration, Box<dyn StdError>> {
        compile_problem(
            vec![
                observation(1, 100, 1, BundleObservationRole::Optimize),
                observation(2, 100, 2, BundleObservationRole::Optimize),
                observation(3, 200, 1, BundleObservationRole::Optimize),
                observation(4, 200, 2, BundleObservationRole::Optimize),
                observation(5, 300, 1, BundleObservationRole::Optimize),
                observation(6, 300, 2, BundleObservationRole::Optimize),
                observation(7, 100, 3, BundleObservationRole::HeldOut),
                observation(8, 200, 3, BundleObservationRole::HeldOut),
            ],
            2,
        )
    }

    fn domains() -> Vec<BundleCameraDomain> {
        bindings()
            .into_iter()
            .map(|binding| BundleCameraDomain {
                camera_node_id: binding.camera_node_id,
                frame_digest: binding.frame_digest,
                effective_calibration_digest: binding.effective_calibration_digest,
                image_width: 640,
                image_height: 480,
            })
            .collect()
    }

    fn clean_provenance() -> Vec<LandmarkSeedProvenance> {
        vec![
            LandmarkSeedProvenance {
                landmark_id: 100,
                support_observation_ids: vec![1, 2],
            },
            LandmarkSeedProvenance {
                landmark_id: 200,
                support_observation_ids: vec![4, 5],
            },
            LandmarkSeedProvenance {
                landmark_id: 300,
                support_observation_ids: vec![7, 8],
            },
        ]
    }

    fn inactive_problem_provenance() -> Vec<LandmarkSeedProvenance> {
        vec![
            LandmarkSeedProvenance {
                landmark_id: 100,
                support_observation_ids: vec![1, 2],
            },
            LandmarkSeedProvenance {
                landmark_id: 200,
                support_observation_ids: vec![3, 4],
            },
            LandmarkSeedProvenance {
                landmark_id: 300,
                support_observation_ids: vec![5, 6],
            },
        ]
    }

    fn admission_basis(
        problem: &BundleProblemGeneration,
        domains: &[BundleCameraDomain],
        provenance: &[LandmarkSeedProvenance],
        policy: BundleAdmissionPolicy,
        generation: u64,
    ) -> Result<BundleAdmissionBasis, BundleAdmissionError> {
        Ok(BundleAdmissionBasis {
            bundle_problem_digest: problem.digest()?,
            camera_domain_basis_digest: camera_domain_table_digest(domains)?,
            seed_provenance_basis_digest: seed_provenance_table_digest(provenance)?,
            policy_digest: bundle_admission_policy_digest(policy)?,
            generation,
        })
    }

    #[test]
    fn complete_image_seed_and_held_out_evidence_is_admitted(
    ) -> Result<(), Box<dyn StdError>> {
        let problem = clean_problem()?;
        assert_eq!(problem.components[0].decision, BundleProblemDecision::Admit);
        let domains = domains();
        let provenance = clean_provenance();
        let policy = BundleAdmissionPolicy::default();
        let value = audit_bundle_admission(
            admission_basis(&problem, &domains, &provenance, policy, 1)?,
            policy,
            problem,
            domains,
            provenance,
        )?;
        assert_eq!(value.components.len(), 1);
        assert_eq!(
            value.components[0].status,
            BundleAdmissionComponentStatus::Admitted
        );
        assert_eq!(
            value.components[0].decision,
            BundleAdmissionDecision::Admit
        );
        assert_eq!(
            value.components[0].independent_held_out_observation_ids,
            vec![9]
        );
        assert_eq!(
            value.components[0].independent_held_out_camera_node_ids,
            vec![3]
        );
        assert!(value.components[0].invalid_image_observation_ids.is_empty());
        assert!(value.components[0].unproven_seed_landmark_ids.is_empty());
        assert!(value.validate().is_ok());
        let json = value.to_json()?;
        assert!(json.contains(BUNDLE_ADMISSION_AUTHORITY));
        assert!(!json.contains("meter"));
        assert!(!json.contains("optimized_landmark"));
        Ok(())
    }

    #[test]
    fn held_out_camera_pruned_from_training_is_not_independent(
    ) -> Result<(), Box<dyn StdError>> {
        let problem = inactive_held_out_problem()?;
        assert_eq!(problem.components[0].decision, BundleProblemDecision::Admit);
        let domains = domains();
        let provenance = inactive_problem_provenance();
        let policy = BundleAdmissionPolicy::default();
        let value = audit_bundle_admission(
            admission_basis(&problem, &domains, &provenance, policy, 2)?,
            policy,
            problem,
            domains,
            provenance,
        )?;
        assert_eq!(
            value.components[0].status,
            BundleAdmissionComponentStatus::InsufficientIndependentHeldOut
        );
        assert_eq!(
            value.components[0].inactive_held_out_observation_ids,
            vec![7, 8]
        );
        assert!(value.components[0].independent_held_out_observation_ids.is_empty());
        assert!(value.observations.iter().any(|audit| {
            audit.observation_id == 7
                && audit.disposition == ObservationAuditDisposition::HeldOutInactiveCamera
        }));
        Ok(())
    }

    #[test]
    fn active_observation_outside_exact_image_domain_blocks_component(
    ) -> Result<(), Box<dyn StdError>> {
        let problem = clean_problem()?;
        let mut domains = domains();
        domains[1].image_width = 50;
        let provenance = clean_provenance();
        let policy = BundleAdmissionPolicy::default();
        let value = audit_bundle_admission(
            admission_basis(&problem, &domains, &provenance, policy, 3)?,
            policy,
            problem,
            domains,
            provenance,
        )?;
        assert_eq!(
            value.components[0].status,
            BundleAdmissionComponentStatus::InvalidImageDomain
        );
        assert_eq!(value.components[0].decision, BundleAdmissionDecision::Block);
        assert!(
            value.components[0]
                .invalid_image_observation_ids
                .contains(&2)
        );
        Ok(())
    }

    #[test]
    fn held_out_measurement_cannot_initialize_a_seed() -> Result<(), Box<dyn StdError>> {
        let problem = clean_problem()?;
        let domains = domains();
        let mut provenance = clean_provenance();
        provenance[2].support_observation_ids = vec![7, 9];
        let policy = BundleAdmissionPolicy::default();
        let result = audit_bundle_admission(
            admission_basis(&problem, &domains, &provenance, policy, 4)?,
            policy,
            problem,
            domains,
            provenance,
        );
        assert!(matches!(
            result,
            Err(BundleAdmissionError::SeedSupportUsesHeldOut {
                landmark_id: 300,
                observation_id: 9
            })
        ));
        Ok(())
    }

    #[test]
    fn insufficient_surviving_seed_support_is_diagnostic(
    ) -> Result<(), Box<dyn StdError>> {
        let problem = clean_problem()?;
        let domains = domains();
        let mut provenance = clean_provenance();
        provenance[2].support_observation_ids = vec![7];
        let policy = BundleAdmissionPolicy::default();
        let value = audit_bundle_admission(
            admission_basis(&problem, &domains, &provenance, policy, 5)?,
            policy,
            problem,
            domains,
            provenance,
        )?;
        assert_eq!(
            value.components[0].status,
            BundleAdmissionComponentStatus::UnprovenSeedPartition
        );
        assert_eq!(
            value.components[0].unproven_seed_landmark_ids,
            vec![300]
        );
        assert!(!value.landmarks[2].provenance_proven);
        Ok(())
    }

    #[test]
    fn successful_operation_ceiling_does_not_change_semantic_identity(
    ) -> Result<(), Box<dyn StdError>> {
        let problem = clean_problem()?;
        let domains = domains();
        let provenance = clean_provenance();
        let low_policy = BundleAdmissionPolicy {
            max_operations: 10_000,
            ..BundleAdmissionPolicy::default()
        };
        let high_policy = BundleAdmissionPolicy {
            max_operations: 10_000_000,
            ..BundleAdmissionPolicy::default()
        };
        let low = audit_bundle_admission(
            admission_basis(&problem, &domains, &provenance, low_policy, 6)?,
            low_policy,
            problem.clone(),
            domains.clone(),
            provenance.clone(),
        )?;
        let high = audit_bundle_admission(
            admission_basis(&problem, &domains, &provenance, high_policy, 6)?,
            high_policy,
            problem,
            domains,
            provenance,
        )?;
        assert_eq!(low.digest()?, high.digest()?);
        Ok(())
    }

    #[test]
    fn operation_budget_fails_closed() -> Result<(), Box<dyn StdError>> {
        let problem = clean_problem()?;
        let domains = domains();
        let provenance = clean_provenance();
        let policy = BundleAdmissionPolicy {
            max_operations: 1,
            ..BundleAdmissionPolicy::default()
        };
        let result = audit_bundle_admission(
            admission_basis(&problem, &domains, &provenance, policy, 7)?,
            policy,
            problem,
            domains,
            provenance,
        );
        assert!(matches!(
            result,
            Err(BundleAdmissionError::OperationBudgetExceeded { .. })
        ));
        Ok(())
    }
}
