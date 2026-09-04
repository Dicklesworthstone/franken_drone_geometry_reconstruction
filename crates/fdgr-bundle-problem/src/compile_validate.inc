fn validate_basis(basis: &BundleProblemBasis) -> Result<(), BundleProblemError> {
    for (field, digest) in [
        ("pose_refinement_digest", &basis.pose_refinement_digest),
        (
            "camera_binding_basis_digest",
            &basis.camera_binding_basis_digest,
        ),
        (
            "landmark_seed_basis_digest",
            &basis.landmark_seed_basis_digest,
        ),
        ("observation_basis_digest", &basis.observation_basis_digest),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_nonzero("generation", basis.generation)
}

fn validate_policy(policy: BundleProblemPolicy) -> Result<(), BundleProblemError> {
    if policy.min_optimize_cameras_per_landmark < 2 {
        return Err(BundleProblemError::BelowMinimum {
            field: "min_optimize_cameras_per_landmark",
            actual: u128::from(policy.min_optimize_cameras_per_landmark),
            minimum: 2,
        });
    }
    for (field, value) in [
        (
            "min_active_landmarks_per_camera",
            policy.min_active_landmarks_per_camera,
        ),
        ("min_root_active_landmarks", policy.min_root_active_landmarks),
        (
            "min_held_out_observations_per_component",
            policy.min_held_out_observations_per_component,
        ),
        (
            "min_held_out_cameras_per_component",
            policy.min_held_out_cameras_per_component,
        ),
    ] {
        if value == 0 {
            return Err(BundleProblemError::ZeroValue { field });
        }
        if u64::from(value) > MAX_SAFE_JSON_INTEGER {
            return Err(BundleProblemError::BoundExceeded {
                field,
                actual: u128::from(value),
                maximum: u128::from(MAX_SAFE_JSON_INTEGER),
            });
        }
    }
    if policy.min_held_out_cameras_per_component
        > policy.min_held_out_observations_per_component
    {
        return Err(BundleProblemError::BoundExceeded {
            field: "min_held_out_cameras_per_component",
            actual: u128::from(policy.min_held_out_cameras_per_component),
            maximum: u128::from(policy.min_held_out_observations_per_component),
        });
    }
    validate_bounded_nonzero(
        "max_observation_uncertainty_nano_pixels",
        policy.max_observation_uncertainty_nano_pixels,
        MAX_OBSERVATION_UNCERTAINTY_NANO_PIXELS,
    )?;
    validate_bounded_nonzero(
        "max_graph_path_expansions",
        policy.max_graph_path_expansions,
        MAX_BUNDLE_GRAPH_PATH_EXPANSIONS,
    )?;
    validate_bounded_nonzero(
        "max_operations",
        policy.max_operations,
        MAX_BUNDLE_OPERATIONS,
    )?;
    Ok(())
}

fn canonicalize_camera_bindings(
    mut bindings: Vec<BundleCameraBinding>,
) -> Result<Vec<BundleCameraBinding>, BundleProblemError> {
    if bindings.len() > MAX_BUNDLE_CAMERAS {
        return Err(BundleProblemError::BoundExceeded {
            field: "camera_binding_count",
            actual: bindings.len() as u128,
            maximum: MAX_BUNDLE_CAMERAS as u128,
        });
    }
    bindings.sort_by_key(|binding| binding.camera_node_id);
    let mut ids = BTreeSet::new();
    for binding in &bindings {
        validate_safe_nonzero("camera_node_id", binding.camera_node_id)?;
        validate_safe_nonzero("camera_sample_index", binding.sample_index)?;
        reject_zero_digest("camera_frame_digest", &binding.frame_digest)?;
        reject_zero_digest(
            "effective_calibration_digest",
            &binding.effective_calibration_digest,
        )?;
        if !ids.insert(binding.camera_node_id) {
            return Err(BundleProblemError::DuplicateCameraBinding {
                camera_node_id: binding.camera_node_id,
            });
        }
    }
    Ok(bindings)
}

fn canonicalize_landmark_seeds(
    mut seeds: Vec<BundleLandmarkSeed>,
) -> Result<Vec<BundleLandmarkSeed>, BundleProblemError> {
    if seeds.len() > MAX_BUNDLE_LANDMARKS {
        return Err(BundleProblemError::BoundExceeded {
            field: "landmark_seed_count",
            actual: seeds.len() as u128,
            maximum: MAX_BUNDLE_LANDMARKS as u128,
        });
    }
    seeds.sort_by_key(|seed| seed.landmark_id);
    let mut landmark_ids = BTreeSet::new();
    let mut track_ids = BTreeSet::new();
    for seed in &seeds {
        validate_safe_nonzero("landmark_id", seed.landmark_id)?;
        validate_safe_nonzero("source_track_id", seed.source_track_id)?;
        validate_safe_nonzero(
            "landmark_component_root_node_id",
            seed.component_root_node_id,
        )?;
        if let Some(scale_root) = seed.scale_component_root_edge_id {
            validate_safe_nonzero("landmark_scale_component_root_edge_id", scale_root)?;
        }
        reject_zero_digest("seed_evidence_digest", &seed.seed_evidence_digest)?;
        for coordinate in seed.seed_position_from_root_nano {
            let magnitude = coordinate.unsigned_abs();
            if magnitude > MAX_LANDMARK_POSITION_ABS_NANO {
                return Err(BundleProblemError::BoundExceeded {
                    field: "seed_position_from_root_nano",
                    actual: u128::from(magnitude),
                    maximum: u128::from(MAX_LANDMARK_POSITION_ABS_NANO),
                });
            }
        }
        if seed.seed_uncertainty_nano > MAX_LANDMARK_UNCERTAINTY_NANO {
            return Err(BundleProblemError::BoundExceeded {
                field: "seed_uncertainty_nano",
                actual: u128::from(seed.seed_uncertainty_nano),
                maximum: u128::from(MAX_LANDMARK_UNCERTAINTY_NANO),
            });
        }
        if !landmark_ids.insert(seed.landmark_id) {
            return Err(BundleProblemError::DuplicateLandmark {
                landmark_id: seed.landmark_id,
            });
        }
        if !track_ids.insert(seed.source_track_id) {
            return Err(BundleProblemError::DuplicateSourceTrack {
                source_track_id: seed.source_track_id,
            });
        }
    }
    Ok(seeds)
}

fn canonicalize_observations(
    mut observations: Vec<BundleObservation>,
) -> Result<Vec<BundleObservation>, BundleProblemError> {
    if observations.len() > MAX_BUNDLE_OBSERVATIONS {
        return Err(BundleProblemError::BoundExceeded {
            field: "observation_count",
            actual: observations.len() as u128,
            maximum: MAX_BUNDLE_OBSERVATIONS as u128,
        });
    }
    observations.sort_by_key(|observation| observation.observation_id);
    let mut observation_ids = BTreeSet::new();
    let mut camera_landmark_pairs = BTreeSet::new();
    let mut camera_features = BTreeSet::new();
    for observation in &observations {
        validate_safe_nonzero("observation_id", observation.observation_id)?;
        validate_safe_nonzero("observation_landmark_id", observation.landmark_id)?;
        validate_safe_nonzero("observation_camera_node_id", observation.camera_node_id)?;
        validate_safe_nonzero("observation_sample_index", observation.sample_index)?;
        validate_safe_nonzero(
            "source_feature_observation_id",
            observation.source_feature_observation_id,
        )?;
        reject_zero_digest("observation_frame_digest", &observation.frame_digest)?;
        reject_zero_digest("observation_evidence_digest", &observation.evidence_digest)?;
        for coordinate in [observation.x_nano_pixels, observation.y_nano_pixels] {
            let magnitude = coordinate.unsigned_abs();
            if magnitude > MAX_ABS_NANO_PIXEL.unsigned_abs() {
                return Err(BundleProblemError::BoundExceeded {
                    field: "observation_coordinate_nano_pixels",
                    actual: u128::from(magnitude),
                    maximum: u128::from(MAX_ABS_NANO_PIXEL.unsigned_abs()),
                });
            }
        }
        if observation.localization_uncertainty_nano_pixels
            > MAX_OBSERVATION_UNCERTAINTY_NANO_PIXELS
        {
            return Err(BundleProblemError::BoundExceeded {
                field: "localization_uncertainty_nano_pixels",
                actual: u128::from(observation.localization_uncertainty_nano_pixels),
                maximum: u128::from(MAX_OBSERVATION_UNCERTAINTY_NANO_PIXELS),
            });
        }
        if !observation_ids.insert(observation.observation_id) {
            return Err(BundleProblemError::DuplicateObservation {
                observation_id: observation.observation_id,
            });
        }
        if !camera_landmark_pairs.insert((observation.camera_node_id, observation.landmark_id)) {
            return Err(BundleProblemError::DuplicateCameraLandmarkPair {
                camera_node_id: observation.camera_node_id,
                landmark_id: observation.landmark_id,
            });
        }
        if !camera_features.insert((
            observation.camera_node_id,
            observation.source_feature_observation_id,
        )) {
            return Err(BundleProblemError::FeatureObservationReused {
                camera_node_id: observation.camera_node_id,
                source_feature_observation_id: observation.source_feature_observation_id,
            });
        }
    }
    Ok(observations)
}

fn validate_bindings_against_poses(
    bindings: &[BundleCameraBinding],
    pose_by_node: &BTreeMap<u64, &fdgr_pose_refinement::RefinedCameraPose>,
    budget: &mut OperationBudget,
) -> Result<(), BundleProblemError> {
    let binding_by_node = bindings
        .iter()
        .map(|binding| (binding.camera_node_id, binding))
        .collect::<BTreeMap<_, _>>();
    for (camera_node_id, pose) in pose_by_node {
        budget.charge("camera_binding_validation")?;
        let binding = binding_by_node.get(camera_node_id).copied().ok_or(
            BundleProblemError::MissingCameraBinding {
                camera_node_id: *camera_node_id,
            },
        )?;
        if binding.sample_index != pose.sample_index {
            return Err(BundleProblemError::CameraSampleMismatch {
                camera_node_id: *camera_node_id,
                expected: pose.sample_index,
                observed: binding.sample_index,
            });
        }
    }
    for binding in bindings {
        budget.charge("extra_camera_binding_validation")?;
        if !pose_by_node.contains_key(&binding.camera_node_id) {
            return Err(BundleProblemError::ExtraCameraBinding {
                camera_node_id: binding.camera_node_id,
            });
        }
    }
    Ok(())
}

fn validate_landmarks_against_components(
    seeds: &[BundleLandmarkSeed],
    component_by_root: &BTreeMap<u64, &fdgr_pose_refinement::PoseRefinementComponent>,
    budget: &mut OperationBudget,
) -> Result<(), BundleProblemError> {
    for seed in seeds {
        budget.charge("landmark_component_validation")?;
        let component = component_by_root
            .get(&seed.component_root_node_id)
            .copied()
            .ok_or(BundleProblemError::UnknownComponentRoot {
                landmark_id: seed.landmark_id,
                component_root_node_id: seed.component_root_node_id,
            })?;
        if component.scale_component_root_edge_id != seed.scale_component_root_edge_id {
            return Err(BundleProblemError::LandmarkGaugeMismatch {
                landmark_id: seed.landmark_id,
                expected: component.scale_component_root_edge_id,
                observed: seed.scale_component_root_edge_id,
            });
        }
    }
    Ok(())
}

fn validate_observation_cross_references(
    observations: &[BundleObservation],
    pose_by_node: &BTreeMap<u64, &fdgr_pose_refinement::RefinedCameraPose>,
    binding_by_node: &BTreeMap<u64, &BundleCameraBinding>,
    landmark_by_id: &BTreeMap<u64, &BundleLandmarkSeed>,
    budget: &mut OperationBudget,
) -> Result<(), BundleProblemError> {
    for observation in observations {
        budget.charge("observation_cross_reference_validation")?;
        let pose = pose_by_node
            .get(&observation.camera_node_id)
            .copied()
            .ok_or(BundleProblemError::UnknownCamera {
                observation_id: observation.observation_id,
                camera_node_id: observation.camera_node_id,
            })?;
        let binding = binding_by_node
            .get(&observation.camera_node_id)
            .copied()
            .ok_or(BundleProblemError::InternalLookup {
                field: "observation_camera_binding",
            })?;
        let landmark = landmark_by_id
            .get(&observation.landmark_id)
            .copied()
            .ok_or(BundleProblemError::UnknownLandmark {
                observation_id: observation.observation_id,
                landmark_id: observation.landmark_id,
            })?;
        if observation.sample_index != binding.sample_index {
            return Err(BundleProblemError::ObservationSampleMismatch {
                observation_id: observation.observation_id,
                expected: binding.sample_index,
                observed: observation.sample_index,
            });
        }
        if observation.frame_digest != binding.frame_digest {
            return Err(BundleProblemError::ObservationFrameMismatch {
                observation_id: observation.observation_id,
                expected: binding.frame_digest.clone(),
                observed: observation.frame_digest.clone(),
            });
        }
        if pose.component_root_node_id != landmark.component_root_node_id {
            return Err(BundleProblemError::ObservationComponentMismatch {
                observation_id: observation.observation_id,
                camera_component_root_node_id: pose.component_root_node_id,
                landmark_component_root_node_id: landmark.component_root_node_id,
            });
        }
        if pose.scale_component_root_edge_id != landmark.scale_component_root_edge_id {
            return Err(BundleProblemError::ObservationGaugeMismatch {
                observation_id: observation.observation_id,
                camera_scale_component_root_edge_id: pose.scale_component_root_edge_id,
                landmark_scale_component_root_edge_id: landmark.scale_component_root_edge_id,
            });
        }
    }
    Ok(())
}
