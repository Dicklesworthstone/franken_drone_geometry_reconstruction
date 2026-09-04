fn derive_support_core(
    policy: BundleProblemPolicy,
    pose_refinement: &PoseRefinementGeneration,
    landmark_seeds: &[BundleLandmarkSeed],
    observations: &[BundleObservation],
    budget: &mut OperationBudget,
) -> Result<SupportCore, BundleProblemError> {
    let root_ids = pose_refinement
        .components
        .iter()
        .map(|component| component.component_root_node_id)
        .collect::<BTreeSet<_>>();
    let mut active_camera_node_ids = pose_refinement
        .poses
        .iter()
        .map(|pose| pose.node_id)
        .collect::<BTreeSet<_>>();
    let mut active_landmark_ids = landmark_seeds
        .iter()
        .map(|seed| seed.landmark_id)
        .collect::<BTreeSet<_>>();
    let mut pruning_rounds = 0_u32;

    loop {
        pruning_rounds = pruning_rounds.checked_add(1).ok_or(
            BundleProblemError::ArithmeticOverflow {
                field: "pruning_rounds",
            },
        )?;
        let mut cameras_by_landmark = BTreeMap::<u64, BTreeSet<u64>>::new();
        for observation in observations {
            budget.charge("landmark_support_scan")?;
            if observation.role != BundleObservationRole::Optimize
                || !observation_quality_eligible(policy, observation)
                || !active_camera_node_ids.contains(&observation.camera_node_id)
                || !active_landmark_ids.contains(&observation.landmark_id)
            {
                continue;
            }
            cameras_by_landmark
                .entry(observation.landmark_id)
                .or_default()
                .insert(observation.camera_node_id);
        }
        let next_landmark_ids = active_landmark_ids
            .iter()
            .filter_map(|landmark_id| {
                let count = cameras_by_landmark
                    .get(landmark_id)
                    .map_or(0_usize, BTreeSet::len);
                (count >= policy.min_optimize_cameras_per_landmark as usize)
                    .then_some(*landmark_id)
            })
            .collect::<BTreeSet<_>>();

        let mut landmarks_by_camera = BTreeMap::<u64, BTreeSet<u64>>::new();
        for observation in observations {
            budget.charge("camera_support_scan")?;
            if observation.role != BundleObservationRole::Optimize
                || !observation_quality_eligible(policy, observation)
                || !active_camera_node_ids.contains(&observation.camera_node_id)
                || !next_landmark_ids.contains(&observation.landmark_id)
            {
                continue;
            }
            landmarks_by_camera
                .entry(observation.camera_node_id)
                .or_default()
                .insert(observation.landmark_id);
        }
        let next_camera_node_ids = active_camera_node_ids
            .iter()
            .filter_map(|camera_node_id| {
                if root_ids.contains(camera_node_id) {
                    return Some(*camera_node_id);
                }
                let count = landmarks_by_camera
                    .get(camera_node_id)
                    .map_or(0_usize, BTreeSet::len);
                (count >= policy.min_active_landmarks_per_camera as usize)
                    .then_some(*camera_node_id)
            })
            .collect::<BTreeSet<_>>();

        let changed = next_landmark_ids != active_landmark_ids
            || next_camera_node_ids != active_camera_node_ids;
        active_landmark_ids = next_landmark_ids;
        active_camera_node_ids = next_camera_node_ids;
        if !changed {
            break;
        }
    }

    Ok(SupportCore {
        active_camera_node_ids,
        active_landmark_ids,
        pruning_rounds,
    })
}

fn compile_observations(
    policy: BundleProblemPolicy,
    observations: &[BundleObservation],
    support: &SupportCore,
) -> Vec<CompiledBundleObservation> {
    observations
        .iter()
        .cloned()
        .map(|observation| {
            let disposition = match observation.role {
                BundleObservationRole::Optimize if observation.dynamic_masked => {
                    BundleObservationDisposition::OptimizeRejectedDynamicMasked
                }
                BundleObservationRole::Optimize
                    if observation.localization_uncertainty_nano_pixels
                        > policy.max_observation_uncertainty_nano_pixels =>
                {
                    BundleObservationDisposition::OptimizeRejectedUncertaintyExceeded
                }
                BundleObservationRole::Optimize
                    if !support
                        .active_landmark_ids
                        .contains(&observation.landmark_id) =>
                {
                    BundleObservationDisposition::OptimizePrunedLandmarkSupport
                }
                BundleObservationRole::Optimize
                    if !support
                        .active_camera_node_ids
                        .contains(&observation.camera_node_id) =>
                {
                    BundleObservationDisposition::OptimizePrunedCameraSupport
                }
                BundleObservationRole::Optimize => BundleObservationDisposition::ActiveOptimize,
                BundleObservationRole::HeldOut if observation.dynamic_masked => {
                    BundleObservationDisposition::HeldOutRejectedDynamicMasked
                }
                BundleObservationRole::HeldOut
                    if observation.localization_uncertainty_nano_pixels
                        > policy.max_observation_uncertainty_nano_pixels =>
                {
                    BundleObservationDisposition::HeldOutRejectedUncertaintyExceeded
                }
                BundleObservationRole::HeldOut
                    if !support
                        .active_landmark_ids
                        .contains(&observation.landmark_id) =>
                {
                    BundleObservationDisposition::HeldOutInactiveLandmark
                }
                BundleObservationRole::HeldOut => BundleObservationDisposition::EligibleHeldOut,
            };
            CompiledBundleObservation {
                observation,
                disposition,
            }
        })
        .collect()
}

fn compile_cameras(
    pose_refinement: &PoseRefinementGeneration,
    bindings: &[BundleCameraBinding],
    observations: &[CompiledBundleObservation],
    support: &SupportCore,
    budget: &mut OperationBudget,
) -> Result<Vec<CompiledBundleCamera>, BundleProblemError> {
    let binding_by_node = bindings
        .iter()
        .map(|binding| (binding.camera_node_id, binding))
        .collect::<BTreeMap<_, _>>();
    let component_by_root = pose_refinement
        .components
        .iter()
        .map(|component| (component.component_root_node_id, component))
        .collect::<BTreeMap<_, _>>();
    let mut landmarks_by_camera = BTreeMap::<u64, BTreeSet<u64>>::new();
    for observation in observations {
        budget.charge("compiled_camera_support_scan")?;
        if observation.disposition == BundleObservationDisposition::ActiveOptimize {
            landmarks_by_camera
                .entry(observation.observation.camera_node_id)
                .or_default()
                .insert(observation.observation.landmark_id);
        }
    }
    let mut compiled = Vec::with_capacity(pose_refinement.poses.len());
    for pose in &pose_refinement.poses {
        budget.charge("compiled_camera_materialization")?;
        let binding = binding_by_node
            .get(&pose.node_id)
            .copied()
            .ok_or(BundleProblemError::InternalLookup {
                field: "compiled_camera_binding",
            })?;
        let component = component_by_root
            .get(&pose.component_root_node_id)
            .copied()
            .ok_or(BundleProblemError::InternalLookup {
                field: "compiled_camera_component",
            })?;
        let (pose_selection, selected_camera_center_from_root_nano) = match component.decision {
            PoseRefinementDecision::AcceptRefined => (
                BundlePoseSelection::Refined,
                pose.refined_camera_center_from_root_nano,
            ),
            PoseRefinementDecision::RetainInitialization => (
                BundlePoseSelection::Initialization,
                pose.initial_camera_center_from_root_nano,
            ),
            PoseRefinementDecision::Blocked => (
                BundlePoseSelection::BlockedInitialization,
                pose.initial_camera_center_from_root_nano,
            ),
        };
        let disposition = if pose.node_id == pose.component_root_node_id {
            BundleCameraDisposition::ActiveGaugeRoot
        } else if support.active_camera_node_ids.contains(&pose.node_id) {
            BundleCameraDisposition::ActiveTraining
        } else {
            BundleCameraDisposition::PrunedInsufficientLandmarkSupport
        };
        let active_landmark_count = usize_to_u32(
            landmarks_by_camera
                .get(&pose.node_id)
                .map_or(0_usize, BTreeSet::len),
            "active_landmark_count",
        )?;
        compiled.push(CompiledBundleCamera {
            binding: binding.clone(),
            component_root_node_id: pose.component_root_node_id,
            scale_component_root_edge_id: pose.scale_component_root_edge_id,
            rotation_node_from_root_nano: pose.rotation_node_from_root_nano,
            selected_camera_center_from_root_nano,
            pose_selection,
            disposition,
            active_landmark_count,
        });
    }
    compiled.sort_by_key(|camera| camera.binding.camera_node_id);
    Ok(compiled)
}

fn compile_landmarks(
    seeds: &[BundleLandmarkSeed],
    observations: &[CompiledBundleObservation],
    support: &SupportCore,
    budget: &mut OperationBudget,
) -> Result<Vec<CompiledBundleLandmark>, BundleProblemError> {
    let mut active_cameras_by_landmark = BTreeMap::<u64, BTreeSet<u64>>::new();
    let mut held_out_cameras_by_landmark = BTreeMap::<u64, BTreeSet<u64>>::new();
    for observation in observations {
        budget.charge("compiled_landmark_support_scan")?;
        match observation.disposition {
            BundleObservationDisposition::ActiveOptimize => {
                active_cameras_by_landmark
                    .entry(observation.observation.landmark_id)
                    .or_default()
                    .insert(observation.observation.camera_node_id);
            }
            BundleObservationDisposition::EligibleHeldOut => {
                held_out_cameras_by_landmark
                    .entry(observation.observation.landmark_id)
                    .or_default()
                    .insert(observation.observation.camera_node_id);
            }
            _ => {}
        }
    }
    let mut compiled = Vec::with_capacity(seeds.len());
    for seed in seeds {
        budget.charge("compiled_landmark_materialization")?;
        let disposition = if support.active_landmark_ids.contains(&seed.landmark_id) {
            BundleLandmarkDisposition::ActiveTraining
        } else {
            BundleLandmarkDisposition::PrunedInsufficientCameraSupport
        };
        compiled.push(CompiledBundleLandmark {
            seed: seed.clone(),
            disposition,
            active_camera_node_ids: active_cameras_by_landmark
                .get(&seed.landmark_id)
                .map_or_else(Vec::new, |values| values.iter().copied().collect()),
            held_out_camera_node_ids: held_out_cameras_by_landmark
                .get(&seed.landmark_id)
                .map_or_else(Vec::new, |values| values.iter().copied().collect()),
        });
    }
    Ok(compiled)
}
