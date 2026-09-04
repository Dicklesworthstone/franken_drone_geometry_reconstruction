fn build_bundle_graph(
    basis: &BundleProblemBasis,
    policy: BundleProblemPolicy,
    cameras: &[CompiledBundleCamera],
    landmarks: &[CompiledBundleLandmark],
    observations: &[CompiledBundleObservation],
    budget: &mut OperationBudget,
) -> Result<(Vec<BundleGraphNodeMapping>, GraphAnalysis), BundleProblemError> {
    let active_cameras = cameras
        .iter()
        .filter(|camera| {
            camera.disposition == BundleCameraDisposition::ActiveGaugeRoot
                || camera.disposition == BundleCameraDisposition::ActiveTraining
        })
        .collect::<Vec<_>>();
    let active_landmarks = landmarks
        .iter()
        .filter(|landmark| landmark.disposition == BundleLandmarkDisposition::ActiveTraining)
        .collect::<Vec<_>>();
    let graph_node_count = active_cameras
        .len()
        .checked_add(active_landmarks.len())
        .ok_or(BundleProblemError::ArithmeticOverflow {
            field: "active_graph_node_count",
        })?;
    if graph_node_count > MAX_BUNDLE_GRAPH_NODES {
        return Err(BundleProblemError::BoundExceeded {
            field: "active_graph_node_count",
            actual: graph_node_count as u128,
            maximum: MAX_BUNDLE_GRAPH_NODES as u128,
        });
    }

    let mut graph_nodes = Vec::with_capacity(graph_node_count);
    let mut graph_node_id = 0_u64;
    let mut camera_graph_id = BTreeMap::new();
    let mut landmark_graph_id = BTreeMap::new();
    for camera in active_cameras {
        budget.charge("camera_graph_node")?;
        graph_node_id = graph_node_id
            .checked_add(1)
            .ok_or(BundleProblemError::ArithmeticOverflow {
                field: "graph_node_id",
            })?;
        let evidence_digest = camera_binding_row_digest(&camera.binding)?;
        camera_graph_id.insert(camera.binding.camera_node_id, graph_node_id);
        graph_nodes.push(BundleGraphNodeMapping {
            graph_node_id,
            kind: BundleGraphNodeKind::Camera,
            domain_id: camera.binding.camera_node_id,
            component_root_node_id: camera.component_root_node_id,
            evidence_digest,
        });
    }
    for landmark in active_landmarks {
        budget.charge("landmark_graph_node")?;
        graph_node_id = graph_node_id
            .checked_add(1)
            .ok_or(BundleProblemError::ArithmeticOverflow {
                field: "graph_node_id",
            })?;
        let evidence_digest = landmark_seed_row_digest(&landmark.seed)?;
        landmark_graph_id.insert(landmark.seed.landmark_id, graph_node_id);
        graph_nodes.push(BundleGraphNodeMapping {
            graph_node_id,
            kind: BundleGraphNodeKind::Landmark,
            domain_id: landmark.seed.landmark_id,
            component_root_node_id: landmark.seed.component_root_node_id,
            evidence_digest,
        });
    }

    let graph_nodes_input = graph_nodes
        .iter()
        .map(|mapping| GraphNode {
            node_id: mapping.graph_node_id,
            evidence_digest: mapping.evidence_digest.clone(),
        })
        .collect::<Vec<_>>();
    let mut graph_edges = Vec::new();
    for observation in observations {
        if observation.disposition != BundleObservationDisposition::ActiveOptimize {
            continue;
        }
        budget.charge("bundle_graph_edge")?;
        let camera_id = *camera_graph_id
            .get(&observation.observation.camera_node_id)
            .ok_or(BundleProblemError::InternalLookup {
                field: "camera_graph_node_id",
            })?;
        let landmark_id = *landmark_graph_id
            .get(&observation.observation.landmark_id)
            .ok_or(BundleProblemError::InternalLookup {
                field: "landmark_graph_node_id",
            })?;
        let uncertainty_margin = MAX_OBSERVATION_UNCERTAINTY_NANO_PIXELS
            .checked_sub(
                observation
                    .observation
                    .localization_uncertainty_nano_pixels,
            )
            .ok_or(BundleProblemError::ArithmeticOverflow {
                field: "observation_uncertainty_margin",
            })?;
        graph_edges.push(GraphEdge {
            edge_id: observation.observation.observation_id,
            left_node_id: camera_id.min(landmark_id),
            right_node_id: camera_id.max(landmark_id),
            priority: EdgePriority {
                primary: uncertainty_margin,
                secondary: observation.observation.observation_id,
            },
            evidence_digest: observation_row_digest(&observation.observation)?,
        });
    }
    graph_edges.sort_by_key(|edge| edge.edge_id);

    let node_basis_digest = bundle_graph_node_table_digest(&graph_nodes)?;
    let edge_basis_digest = bundle_graph_edge_table_digest(&graph_edges)?;
    let graph = analyze_graph(
        GraphBasis {
            node_basis_digest,
            edge_basis_digest,
            selection_policy_digest: bundle_graph_selection_policy_digest()?,
            generation: basis.generation,
        },
        GraphBudget {
            max_path_expansions: policy.max_graph_path_expansions,
        },
        graph_nodes_input,
        graph_edges,
    )?;
    Ok((graph_nodes, graph))
}
