fn compile_components(
    policy: BundleProblemPolicy,
    pose_refinement: &PoseRefinementGeneration,
    cameras: &[CompiledBundleCamera],
    landmarks: &[CompiledBundleLandmark],
    observations: &[CompiledBundleObservation],
    graph_nodes: &[BundleGraphNodeMapping],
    graph: &GraphAnalysis,
    budget: &mut OperationBudget,
) -> Result<Vec<BundleProblemComponent>, BundleProblemError> {
    let mut camera_by_node = BTreeMap::new();
    let mut active_cameras_by_root = BTreeMap::<u64, Vec<u64>>::new();
    let mut pruned_cameras_by_root = BTreeMap::<u64, Vec<u64>>::new();
    for camera in cameras {
        budget.charge("component_camera_index")?;
        camera_by_node.insert(camera.binding.camera_node_id, camera);
        let target = if camera.disposition
            == BundleCameraDisposition::PrunedInsufficientLandmarkSupport
        {
            &mut pruned_cameras_by_root
        } else {
            &mut active_cameras_by_root
        };
        target
            .entry(camera.component_root_node_id)
            .or_default()
            .push(camera.binding.camera_node_id);
    }

    let mut active_landmarks_by_root = BTreeMap::<u64, Vec<u64>>::new();
    let mut pruned_landmarks_by_root = BTreeMap::<u64, Vec<u64>>::new();
    for landmark in landmarks {
        budget.charge("component_landmark_index")?;
        let target = if landmark.disposition == BundleLandmarkDisposition::ActiveTraining {
            &mut active_landmarks_by_root
        } else {
            &mut pruned_landmarks_by_root
        };
        target
            .entry(landmark.seed.component_root_node_id)
            .or_default()
            .push(landmark.seed.landmark_id);
    }

    let mut active_observation_component = BTreeMap::<u64, u64>::new();
    let mut active_observations_by_root = BTreeMap::<u64, Vec<u64>>::new();
    let mut held_out_observations_by_root = BTreeMap::<u64, Vec<u64>>::new();
    let mut held_out_cameras_by_root = BTreeMap::<u64, BTreeSet<u64>>::new();
    let mut root_landmarks_by_root = BTreeMap::<u64, BTreeSet<u64>>::new();
    for compiled in observations {
        budget.charge("component_observation_index")?;
        let observation = &compiled.observation;
        let camera = camera_by_node
            .get(&observation.camera_node_id)
            .copied()
            .ok_or(BundleProblemError::InternalLookup {
                field: "component_observation_camera",
            })?;
        let root = camera.component_root_node_id;
        match compiled.disposition {
            BundleObservationDisposition::ActiveOptimize => {
                active_observation_component.insert(observation.observation_id, root);
                active_observations_by_root
                    .entry(root)
                    .or_default()
                    .push(observation.observation_id);
                if observation.camera_node_id == root {
                    root_landmarks_by_root
                        .entry(root)
                        .or_default()
                        .insert(observation.landmark_id);
                }
            }
            BundleObservationDisposition::EligibleHeldOut => {
                held_out_observations_by_root
                    .entry(root)
                    .or_default()
                    .push(observation.observation_id);
                held_out_cameras_by_root
                    .entry(root)
                    .or_default()
                    .insert(observation.camera_node_id);
            }
            _ => {}
        }
    }

    let mut graph_mapping_by_id = BTreeMap::new();
    let mut camera_graph_id = BTreeMap::new();
    let mut graph_node_ids_by_root = BTreeMap::<u64, Vec<u64>>::new();
    for mapping in graph_nodes {
        budget.charge("component_graph_node_index")?;
        graph_mapping_by_id.insert(mapping.graph_node_id, mapping);
        graph_node_ids_by_root
            .entry(mapping.component_root_node_id)
            .or_default()
            .push(mapping.graph_node_id);
        if mapping.kind == BundleGraphNodeKind::Camera {
            camera_graph_id.insert(mapping.domain_id, mapping.graph_node_id);
        }
    }

    let mut graph_component_by_node = BTreeMap::new();
    let mut graph_component_ids_by_root = BTreeMap::<u64, BTreeSet<u64>>::new();
    for component in &graph.components {
        for graph_node_id in &component.node_ids {
            budget.charge("component_graph_component_index")?;
            graph_component_by_node.insert(*graph_node_id, component.component_id);
            let mapping = graph_mapping_by_id
                .get(graph_node_id)
                .copied()
                .ok_or(BundleProblemError::InternalLookup {
                    field: "component_graph_mapping",
                })?;
            graph_component_ids_by_root
                .entry(mapping.component_root_node_id)
                .or_default()
                .insert(component.component_id);
        }
    }

    let mut bridge_observations_by_root = BTreeMap::<u64, Vec<u64>>::new();
    for observation_id in &graph.bridge_edge_ids {
        budget.charge("component_bridge_index")?;
        let root = *active_observation_component
            .get(observation_id)
            .ok_or(BundleProblemError::InternalLookup {
                field: "bridge_observation_component",
            })?;
        bridge_observations_by_root
            .entry(root)
            .or_default()
            .push(*observation_id);
    }
    let mut forest_observations_by_root = BTreeMap::<u64, Vec<u64>>::new();
    for observation_id in &graph.forest_edge_ids {
        budget.charge("component_forest_index")?;
        let root = *active_observation_component
            .get(observation_id)
            .ok_or(BundleProblemError::InternalLookup {
                field: "forest_observation_component",
            })?;
        forest_observations_by_root
            .entry(root)
            .or_default()
            .push(*observation_id);
    }
    let mut non_forest_observations_by_root = BTreeMap::<u64, Vec<u64>>::new();
    for observation_id in &graph.non_forest_edge_ids {
        budget.charge("component_non_forest_index")?;
        let root = *active_observation_component
            .get(observation_id)
            .ok_or(BundleProblemError::InternalLookup {
                field: "non_forest_observation_component",
            })?;
        non_forest_observations_by_root
            .entry(root)
            .or_default()
            .push(*observation_id);
    }
    let mut cycle_rank_by_root = BTreeMap::<u64, u64>::new();
    for cycle in &graph.fundamental_cycles {
        budget.charge("component_cycle_index")?;
        let root = *active_observation_component
            .get(&cycle.closing_edge_id)
            .ok_or(BundleProblemError::InternalLookup {
                field: "cycle_observation_component",
            })?;
        let entry = cycle_rank_by_root.entry(root).or_default();
        *entry = entry
            .checked_add(1)
            .ok_or(BundleProblemError::ArithmeticOverflow {
                field: "component_cycle_rank",
            })?;
    }

    let mut output = Vec::with_capacity(pose_refinement.components.len());
    for pose_component in &pose_refinement.components {
        budget.charge("bundle_component_materialization")?;
        let root = pose_component.component_root_node_id;
        let mut active_camera_node_ids = active_cameras_by_root
            .remove(&root)
            .unwrap_or_default();
        let mut pruned_camera_node_ids = pruned_cameras_by_root
            .remove(&root)
            .unwrap_or_default();
        let mut active_landmark_ids = active_landmarks_by_root
            .remove(&root)
            .unwrap_or_default();
        let mut pruned_landmark_ids = pruned_landmarks_by_root
            .remove(&root)
            .unwrap_or_default();
        let mut active_optimize_observation_ids = active_observations_by_root
            .remove(&root)
            .unwrap_or_default();
        let mut eligible_held_out_observation_ids = held_out_observations_by_root
            .remove(&root)
            .unwrap_or_default();
        let mut eligible_held_out_camera_node_ids = held_out_cameras_by_root
            .remove(&root)
            .unwrap_or_default()
            .into_iter()
            .collect::<Vec<_>>();
        let mut bridge_observation_ids = bridge_observations_by_root
            .remove(&root)
            .unwrap_or_default();
        let mut forest_observation_ids = forest_observations_by_root
            .remove(&root)
            .unwrap_or_default();
        let mut non_forest_observation_ids = non_forest_observations_by_root
            .remove(&root)
            .unwrap_or_default();
        let cycle_rank = cycle_rank_by_root.remove(&root).unwrap_or_default();
        let mut graph_component_ids = graph_component_ids_by_root
            .remove(&root)
            .unwrap_or_default()
            .into_iter()
            .collect::<Vec<_>>();

        let root_graph_node_id = *camera_graph_id
            .get(&root)
            .ok_or(BundleProblemError::InternalLookup {
                field: "root_graph_node_id",
            })?;
        let root_graph_component_id = *graph_component_by_node
            .get(&root_graph_node_id)
            .ok_or(BundleProblemError::InternalLookup {
                field: "root_graph_component_id",
            })?;
        let root_connected = graph_node_ids_by_root
            .get(&root)
            .is_some_and(|graph_node_ids| {
                graph_node_ids.iter().all(|graph_node_id| {
                    graph_component_by_node
                        .get(graph_node_id)
                        .is_some_and(|component_id| *component_id == root_graph_component_id)
                })
            });
        let root_active_landmark_count = usize_to_u32(
            root_landmarks_by_root
                .remove(&root)
                .map_or(0_usize, |values| values.len()),
            "root_active_landmark_count",
        )?;

        active_camera_node_ids.sort_unstable();
        pruned_camera_node_ids.sort_unstable();
        active_landmark_ids.sort_unstable();
        pruned_landmark_ids.sort_unstable();
        active_optimize_observation_ids.sort_unstable();
        eligible_held_out_observation_ids.sort_unstable();
        eligible_held_out_camera_node_ids.sort_unstable();
        bridge_observation_ids.sort_unstable();
        forest_observation_ids.sort_unstable();
        non_forest_observation_ids.sort_unstable();
        graph_component_ids.sort_unstable();

        let held_out_sufficient = eligible_held_out_observation_ids.len()
            >= policy.min_held_out_observations_per_component as usize
            && eligible_held_out_camera_node_ids.len()
                >= policy.min_held_out_cameras_per_component as usize;
        let status = if pose_component.decision == PoseRefinementDecision::Blocked {
            BundleProblemComponentStatus::BlockedPoseRefinement
        } else if active_landmark_ids.is_empty() || active_optimize_observation_ids.is_empty() {
            BundleProblemComponentStatus::NoTrainingCore
        } else if root_active_landmark_count < policy.min_root_active_landmarks {
            BundleProblemComponentStatus::GaugeRootUnobserved
        } else if !root_connected {
            BundleProblemComponentStatus::DisconnectedFromGaugeRoot
        } else if cycle_rank == 0 {
            BundleProblemComponentStatus::TreeLike
        } else if !bridge_observation_ids.is_empty() {
            BundleProblemComponentStatus::Fragile
        } else if !held_out_sufficient {
            BundleProblemComponentStatus::MissingHeldOutEvidence
        } else {
            BundleProblemComponentStatus::Redundant
        };
        let (decision, recommendation) = component_decision(status);
        let nominal_equation_count = usize_to_u64(active_optimize_observation_ids.len())?
            .checked_mul(2)
            .ok_or(BundleProblemError::ArithmeticOverflow {
                field: "nominal_equation_count",
            })?;
        let non_root_camera_count = active_camera_node_ids
            .iter()
            .filter(|camera_node_id| **camera_node_id != root)
            .count();
        let nominal_unknown_count = usize_to_u64(
            non_root_camera_count
                .checked_add(active_landmark_ids.len())
                .ok_or(BundleProblemError::ArithmeticOverflow {
                    field: "nominal_unknown_subject_count",
                })?,
        )?
        .checked_mul(3)
        .ok_or(BundleProblemError::ArithmeticOverflow {
            field: "nominal_unknown_count",
        })?;
        let nominal_equation_surplus_i128 = i128::from(nominal_equation_count)
            .checked_sub(i128::from(nominal_unknown_count))
            .ok_or(BundleProblemError::ArithmeticOverflow {
                field: "nominal_equation_surplus",
            })?;
        let nominal_equation_surplus = i64::try_from(nominal_equation_surplus_i128).map_err(
            |_| BundleProblemError::ArithmeticOverflow {
                field: "nominal_equation_surplus",
            },
        )?;

        output.push(BundleProblemComponent {
            component_root_node_id: root,
            scale_component_root_edge_id: pose_component.scale_component_root_edge_id,
            status,
            decision,
            recommendation,
            active_camera_node_ids,
            pruned_camera_node_ids,
            active_landmark_ids,
            pruned_landmark_ids,
            active_optimize_observation_ids,
            eligible_held_out_observation_ids,
            eligible_held_out_camera_node_ids,
            bridge_observation_ids,
            forest_observation_ids,
            non_forest_observation_ids,
            graph_component_ids,
            root_connected,
            root_active_landmark_count,
            cycle_rank,
            nominal_equation_count,
            nominal_unknown_count,
            nominal_equation_surplus,
        });
    }
    output.sort_by_key(|component| component.component_root_node_id);
    Ok(output)
}
