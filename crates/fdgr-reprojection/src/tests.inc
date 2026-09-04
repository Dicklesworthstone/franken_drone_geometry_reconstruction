#[cfg(test)]
mod tests {
    use super::{
        REPROJECTION_AUTHORITY, REPROJECTION_EVIDENCE_AUTHORITY, ReprojectionBasis,
        ReprojectionCalibration, ReprojectionComponentStatus, ReprojectionDecision,
        ReprojectionError, ReprojectionObservationDisposition, ReprojectionPolicy,
        evaluate_reprojection, reprojection_algorithm_digest,
        reprojection_calibration_table_digest, reprojection_policy_digest,
        root_point_to_camera,
    };
    use fdgr_bundle_admission::{
        BundleAdmissionBasis, BundleAdmissionPolicy, BundleCameraDomain,
        LandmarkSeedProvenance, audit_bundle_admission, bundle_admission_policy_digest,
        camera_domain_table_digest, seed_provenance_table_digest,
    };
    use fdgr_bundle_problem::{
        BundleCameraBinding, BundleLandmarkSeed, BundleObservation, BundleObservationRole,
        BundleProblemBasis, BundleProblemGeneration, BundleProblemPolicy,
        bundle_observation_table_digest, bundle_problem_policy_digest,
        camera_binding_table_digest, compile_bundle_problem, landmark_seed_table_digest,
    };
    use fdgr_calibration::{
        DerivedCalibration, DerivedReadout, DistortionModel, ImageDerivation, PinholeIntrinsics,
        PixelConvention, ReadoutDirection, RigidTransform,
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
        PoseRefinementBasis, PoseRefinementDecision, PoseRefinementGeneration,
        PoseRefinementPolicy, UNIT_NANO, refine_pose_translation,
    };
    use fdgr_projection::{CalibratedProjectionPolicy, project_calibrated_camera_point};
    use fdgr_types::EvidenceDigest;
    use std::error::Error as StdError;

    struct Fixture {
        admission: fdgr_bundle_admission::BundleAdmissionGeneration,
        calibrations: Vec<ReprojectionCalibration>,
    }

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

    fn refinement() -> Result<PoseRefinementGeneration, Box<dyn StdError>> {
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

    const fn sample_index(camera_node_id: u64) -> u64 {
        camera_node_id * 10
    }

    fn frame_digest(camera_node_id: u64) -> EvidenceDigest {
        let byte = u8::try_from(camera_node_id.saturating_add(100)).map_or(240, |value| value);
        digest(byte)
    }

    fn derived_calibration(camera_node_id: u64, rolling: bool) -> DerivedCalibration {
        DerivedCalibration {
            source_calibration_digest: digest(
                u8::try_from(camera_node_id.saturating_add(150)).map_or(239, |value| value),
            ),
            derivation: ImageDerivation {
                source_width: 640,
                source_height: 480,
                crop_x: 0,
                crop_y: 0,
                crop_width: 640,
                crop_height: 480,
                output_width: 640,
                output_height: 480,
            },
            intrinsics: PinholeIntrinsics {
                fx_nano_pixels: 100 * UNIT_NANO,
                fy_nano_pixels: 100 * UNIT_NANO,
                cx_nano_pixels: 320 * UNIT_NANO,
                cy_nano_pixels: 240 * UNIT_NANO,
                pixel_convention: PixelConvention::TopLeftHalfPixel,
            },
            distortion: DistortionModel::None,
            readout: if rolling {
                DerivedReadout {
                    rolling: true,
                    direction: Some(ReadoutDirection::TopToBottom),
                    first_observed_line_offset_ns: 0,
                    observed_readout_time_ns: 10_000_000,
                    reference_phase_nano: 500_000_000,
                }
            } else {
                DerivedReadout {
                    rolling: false,
                    direction: None,
                    first_observed_line_offset_ns: 0,
                    observed_readout_time_ns: 0,
                    reference_phase_nano: 0,
                }
            },
            camera_from_body: RigidTransform::IDENTITY,
            declared_uncertainty_nano_pixels: 100_000_000,
        }
    }

    fn calibrations(
        rolling_camera: Option<u64>,
    ) -> Result<Vec<ReprojectionCalibration>, Box<dyn StdError>> {
        let mut values = Vec::new();
        for camera_node_id in [1_u64, 2, 3] {
            let calibration =
                derived_calibration(camera_node_id, rolling_camera == Some(camera_node_id));
            values.push(ReprojectionCalibration {
                camera_node_id,
                frame_digest: frame_digest(camera_node_id),
                effective_calibration_digest: calibration.digest()?,
                calibration,
            });
        }
        Ok(values)
    }

    fn landmark_seed(
        landmark_id: u64,
        source_track_id: u64,
        scale_root: Option<u64>,
        position: [i64; 3],
    ) -> BundleLandmarkSeed {
        BundleLandmarkSeed {
            landmark_id,
            source_track_id,
            component_root_node_id: 1,
            scale_component_root_edge_id: scale_root,
            seed_evidence_digest: digest(u8::try_from(landmark_id).map_or(200, |value| value)),
            seed_position_from_root_nano: position,
            seed_uncertainty_nano: 100_000_000,
        }
    }

    fn selected_center(
        refinement: &PoseRefinementGeneration,
        camera_node_id: u64,
    ) -> Result<([i64; 9], [i64; 3]), ReprojectionError> {
        let pose = refinement
            .poses
            .iter()
            .find(|pose| pose.node_id == camera_node_id)
            .ok_or(ReprojectionError::InternalLookup {
                field: "test_pose",
            })?;
        let component = refinement
            .components
            .iter()
            .find(|component| {
                component.component_root_node_id == pose.component_root_node_id
            })
            .ok_or(ReprojectionError::InternalLookup {
                field: "test_component",
            })?;
        let center = match component.decision {
            PoseRefinementDecision::AcceptRefined => {
                pose.refined_camera_center_from_root_nano
            }
            PoseRefinementDecision::RetainInitialization | PoseRefinementDecision::Blocked => {
                pose.initial_camera_center_from_root_nano
            }
        };
        Ok((pose.rotation_node_from_root_nano, center))
    }

    fn exact_observation(
        refinement: &PoseRefinementGeneration,
        seed: &BundleLandmarkSeed,
        calibration: &DerivedCalibration,
        observation_id: u64,
        camera_node_id: u64,
        role: BundleObservationRole,
    ) -> Result<BundleObservation, Box<dyn StdError>> {
        let (rotation, center) = selected_center(refinement, camera_node_id)?;
        let point_camera = root_point_to_camera(
            rotation,
            center,
            seed.seed_position_from_root_nano,
        )?;
        let projected = project_calibrated_camera_point(
            calibration,
            point_camera,
            CalibratedProjectionPolicy::default(),
        )?;
        Ok(BundleObservation {
            observation_id,
            landmark_id: seed.landmark_id,
            camera_node_id,
            sample_index: sample_index(camera_node_id),
            frame_digest: frame_digest(camera_node_id),
            source_feature_observation_id: observation_id.saturating_add(1_000),
            evidence_digest: digest(
                u8::try_from(observation_id.saturating_add(20)).map_or(238, |value| value),
            ),
            x_nano_pixels: projected.x_nano_pixels,
            y_nano_pixels: projected.y_nano_pixels,
            localization_uncertainty_nano_pixels: 100_000_000,
            dynamic_masked: false,
            role,
        })
    }

    fn fixture(
        rolling_camera: Option<u64>,
        shifted_observation: Option<(u64, i64)>,
    ) -> Result<Fixture, Box<dyn StdError>> {
        let refinement = refinement()?;
        let scale_root = refinement
            .components
            .first()
            .and_then(|component| component.scale_component_root_edge_id);
        let seeds = vec![
            landmark_seed(100, 500, scale_root, [0, 0, 5 * UNIT_NANO]),
            landmark_seed(200, 600, scale_root, [UNIT_NANO, 0, 5 * UNIT_NANO]),
            landmark_seed(300, 700, scale_root, [0, UNIT_NANO, 5 * UNIT_NANO]),
        ];
        let exact_calibrations = calibrations(rolling_camera)?;
        let observation_calibrations = calibrations(None)?;
        let projection_calibration = |camera_node_id: u64| {
            observation_calibrations
                .iter()
                .find(|calibration| calibration.camera_node_id == camera_node_id)
                .map(|calibration| &calibration.calibration)
                .ok_or(ReprojectionError::InternalLookup {
                    field: "test_projection_calibration",
                })
        };
        let seed_by_id = |landmark_id: u64| {
            seeds
                .iter()
                .find(|seed| seed.landmark_id == landmark_id)
                .ok_or(ReprojectionError::InternalLookup {
                    field: "test_landmark_seed",
                })
        };
        let specifications = [
            (1_u64, 100_u64, 1_u64, BundleObservationRole::Optimize),
            (2, 100, 2, BundleObservationRole::Optimize),
            (3, 100, 3, BundleObservationRole::Optimize),
            (4, 200, 1, BundleObservationRole::Optimize),
            (5, 200, 2, BundleObservationRole::Optimize),
            (6, 200, 3, BundleObservationRole::Optimize),
            (7, 300, 1, BundleObservationRole::Optimize),
            (8, 300, 2, BundleObservationRole::Optimize),
            (9, 300, 3, BundleObservationRole::HeldOut),
        ];
        let mut observations = Vec::new();
        for (observation_id, landmark_id, camera_node_id, role) in specifications {
            observations.push(exact_observation(
                &refinement,
                seed_by_id(landmark_id)?,
                projection_calibration(camera_node_id)?,
                observation_id,
                camera_node_id,
                role,
            )?);
        }
        if let Some((observation_id, shift)) = shifted_observation {
            let observation = observations
                .iter_mut()
                .find(|observation| observation.observation_id == observation_id)
                .ok_or(ReprojectionError::InternalLookup {
                    field: "test_shifted_observation",
                })?;
            observation.x_nano_pixels = observation
                .x_nano_pixels
                .checked_add(shift)
                .ok_or(ReprojectionError::ArithmeticOverflow {
                    field: "test_observation_shift",
                })?;
        }
        let bindings = exact_calibrations
            .iter()
            .map(|calibration| BundleCameraBinding {
                camera_node_id: calibration.camera_node_id,
                sample_index: sample_index(calibration.camera_node_id),
                frame_digest: calibration.frame_digest.clone(),
                effective_calibration_digest: calibration.effective_calibration_digest.clone(),
            })
            .collect::<Vec<_>>();
        let problem_policy = BundleProblemPolicy {
            min_held_out_observations_per_component: 1,
            min_held_out_cameras_per_component: 1,
            ..BundleProblemPolicy::default()
        };
        let problem = compile_bundle_problem(
            BundleProblemBasis {
                pose_refinement_digest: refinement.digest()?,
                camera_binding_basis_digest: camera_binding_table_digest(&bindings)?,
                landmark_seed_basis_digest: landmark_seed_table_digest(&seeds)?,
                observation_basis_digest: bundle_observation_table_digest(&observations)?,
                policy_digest: bundle_problem_policy_digest(problem_policy)?,
                generation: 1,
            },
            problem_policy,
            refinement,
            bindings,
            seeds,
            observations,
        )?;
        admitted_fixture(problem, exact_calibrations)
    }

    fn admitted_fixture(
        problem: BundleProblemGeneration,
        calibrations: Vec<ReprojectionCalibration>,
    ) -> Result<Fixture, Box<dyn StdError>> {
        let domains = calibrations
            .iter()
            .map(|calibration| BundleCameraDomain {
                camera_node_id: calibration.camera_node_id,
                frame_digest: calibration.frame_digest.clone(),
                effective_calibration_digest: calibration.effective_calibration_digest.clone(),
                image_width: calibration.calibration.derivation.output_width,
                image_height: calibration.calibration.derivation.output_height,
            })
            .collect::<Vec<_>>();
        let provenance = vec![
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
        ];
        let policy = BundleAdmissionPolicy::default();
        let admission = audit_bundle_admission(
            BundleAdmissionBasis {
                bundle_problem_digest: problem.digest()?,
                camera_domain_basis_digest: camera_domain_table_digest(&domains)?,
                seed_provenance_basis_digest: seed_provenance_table_digest(&provenance)?,
                policy_digest: bundle_admission_policy_digest(policy)?,
                generation: 1,
            },
            policy,
            problem,
            domains,
            provenance,
        )?;
        Ok(Fixture {
            admission,
            calibrations,
        })
    }

    fn evaluation_basis(
        fixture: &Fixture,
        policy: ReprojectionPolicy,
        generation: u64,
    ) -> Result<ReprojectionBasis, ReprojectionError> {
        Ok(ReprojectionBasis {
            bundle_admission_digest: fixture.admission.digest()?,
            calibration_table_digest: reprojection_calibration_table_digest(
                &fixture.calibrations,
            )?,
            algorithm_digest: reprojection_algorithm_digest()?,
            policy_digest: reprojection_policy_digest(policy)?,
            generation,
        })
    }

    #[test]
    fn exact_baseline_is_admitted_and_replays() -> Result<(), Box<dyn StdError>> {
        let fixture = fixture(None, None)?;
        let policy = ReprojectionPolicy::default();
        let value = evaluate_reprojection(
            evaluation_basis(&fixture, policy, 1)?,
            policy,
            fixture.admission,
            fixture.calibrations,
        )?;
        assert_eq!(value.components.len(), 1);
        assert_eq!(
            value.components[0].status,
            ReprojectionComponentStatus::Consistent
        );
        assert_eq!(value.components[0].decision, ReprojectionDecision::Admit);
        assert_eq!(value.components[0].optimize.eligible_observation_count, 8);
        assert_eq!(value.components[0].optimize.projected_observation_count, 8);
        assert_eq!(value.components[0].optimize.inlier_observation_count, 8);
        assert_eq!(value.components[0].held_out.eligible_observation_count, 1);
        assert_eq!(value.components[0].held_out.projected_observation_count, 1);
        assert_eq!(value.components[0].held_out.inlier_observation_count, 1);
        assert_eq!(value.components[0].optimize.rms_residual_nano_pixels, Some(0));
        assert_eq!(value.components[0].held_out.rms_residual_nano_pixels, Some(0));
        assert!(value.observations.iter().all(|observation| {
            observation.disposition == ReprojectionObservationDisposition::ProjectedInlier
        }));
        assert_eq!(value.authority(), REPROJECTION_AUTHORITY);
        assert!(value.validate().is_ok());
        let json = value.to_json()?;
        assert!(json.contains("fdgr.reprojection_evaluation/1"));
        assert!(json.contains(REPROJECTION_AUTHORITY));
        assert!(!json.contains("meter"));
        assert!(!json.contains("optimized_landmark"));
        Ok(())
    }

    #[test]
    fn residual_conflict_is_diagnostic_and_evidence_only(
    ) -> Result<(), Box<dyn StdError>> {
        let fixture = fixture(None, Some((1, 3 * UNIT_NANO)))?;
        let policy = ReprojectionPolicy::default();
        let value = evaluate_reprojection(
            evaluation_basis(&fixture, policy, 2)?,
            policy,
            fixture.admission,
            fixture.calibrations,
        )?;
        assert_eq!(
            value.components[0].status,
            ReprojectionComponentStatus::ResidualConflict
        );
        assert_eq!(
            value.components[0].decision,
            ReprojectionDecision::AdmitDiagnostic
        );
        assert_eq!(value.components[0].conflicting_observation_ids, vec![1]);
        assert_eq!(value.authority(), REPROJECTION_EVIDENCE_AUTHORITY);
        assert!(value.observations.iter().any(|observation| {
            observation.observation_id == 1
                && observation.disposition
                    == ReprojectionObservationDisposition::ResidualExceeded
        }));
        Ok(())
    }

    #[test]
    fn rolling_shutter_requires_motion_model() -> Result<(), Box<dyn StdError>> {
        let fixture = fixture(Some(3), None)?;
        let policy = ReprojectionPolicy::default();
        let value = evaluate_reprojection(
            evaluation_basis(&fixture, policy, 3)?,
            policy,
            fixture.admission,
            fixture.calibrations,
        )?;
        assert_eq!(
            value.components[0].status,
            ReprojectionComponentStatus::ProjectionConflict
        );
        assert_eq!(value.components[0].decision, ReprojectionDecision::Block);
        assert_eq!(
            value.components[0].unprojectable_observation_ids,
            vec![3, 6, 9]
        );
        assert!(value.observations.iter().filter(|observation| {
            observation.disposition
                == ReprojectionObservationDisposition::RollingShutterUnsupported
        }).count() == 3);
        assert_eq!(value.authority(), REPROJECTION_EVIDENCE_AUTHORITY);
        Ok(())
    }

    #[test]
    fn calibration_substitution_fails_closed() -> Result<(), Box<dyn StdError>> {
        let fixture = fixture(None, None)?;
        let policy = ReprojectionPolicy::default();
        let mut wrong_frame = fixture.calibrations.clone();
        wrong_frame[0].frame_digest = digest(222);
        let wrong_frame_basis = ReprojectionBasis {
            bundle_admission_digest: fixture.admission.digest()?,
            calibration_table_digest: reprojection_calibration_table_digest(&wrong_frame)?,
            algorithm_digest: reprojection_algorithm_digest()?,
            policy_digest: reprojection_policy_digest(policy)?,
            generation: 4,
        };
        assert!(matches!(
            evaluate_reprojection(
                wrong_frame_basis,
                policy,
                fixture.admission.clone(),
                wrong_frame,
            ),
            Err(ReprojectionError::CalibrationFrameMismatch { camera_node_id: 1 })
        ));

        let mut changed_calibration = fixture.calibrations.clone();
        changed_calibration[0]
            .calibration
            .declared_uncertainty_nano_pixels = 100_000_001;
        assert!(matches!(
            evaluate_reprojection(
                evaluation_basis(&fixture, policy, 5)?,
                policy,
                fixture.admission,
                changed_calibration,
            ),
            Err(ReprojectionError::CalibrationDigestMismatch { camera_node_id: 1 })
        ));
        Ok(())
    }

    #[test]
    fn input_order_and_successful_budget_do_not_change_identity(
    ) -> Result<(), Box<dyn StdError>> {
        let fixture = fixture(None, None)?;
        let low_policy = ReprojectionPolicy {
            max_operations: 10_000,
            ..ReprojectionPolicy::default()
        };
        let high_policy = ReprojectionPolicy {
            max_operations: 10_000_000,
            ..ReprojectionPolicy::default()
        };
        let mut reversed = fixture.calibrations.clone();
        reversed.reverse();
        let low = evaluate_reprojection(
            evaluation_basis(&fixture, low_policy, 6)?,
            low_policy,
            fixture.admission.clone(),
            reversed,
        )?;
        let high = evaluate_reprojection(
            evaluation_basis(&fixture, high_policy, 6)?,
            high_policy,
            fixture.admission,
            fixture.calibrations,
        )?;
        assert_eq!(low.digest()?, high.digest()?);
        assert_eq!(low.operation_count, high.operation_count);
        assert_eq!(
            low.calibrations
                .iter()
                .map(|calibration| calibration.camera_node_id)
                .collect::<Vec<_>>(),
            vec![1, 2, 3]
        );
        Ok(())
    }

    #[test]
    fn operation_budget_refuses_partial_generation() -> Result<(), Box<dyn StdError>> {
        let fixture = fixture(None, None)?;
        let policy = ReprojectionPolicy {
            max_operations: 1,
            ..ReprojectionPolicy::default()
        };
        assert!(matches!(
            evaluate_reprojection(
                evaluation_basis(&fixture, policy, 7)?,
                policy,
                fixture.admission,
                fixture.calibrations,
            ),
            Err(ReprojectionError::OperationBudgetExceeded { .. })
        ));
        Ok(())
    }

    #[test]
    fn retained_output_mutation_fails_replay() -> Result<(), Box<dyn StdError>> {
        let fixture = fixture(None, None)?;
        let policy = ReprojectionPolicy::default();
        let mut value = evaluate_reprojection(
            evaluation_basis(&fixture, policy, 8)?,
            policy,
            fixture.admission,
            fixture.calibrations,
        )?;
        value.operation_count = value.operation_count.saturating_add(1);
        assert!(matches!(
            value.validate(),
            Err(ReprojectionError::DerivedMismatch {
                field: "reprojection_evaluation"
            })
        ));
        Ok(())
    }
}
