const CAMERA_BINDING_ROW_SCHEMA: &str = "fdgr.bundle_camera_binding/1";
const LANDMARK_SEED_ROW_SCHEMA: &str = "fdgr.bundle_landmark_seed/1";
const BUNDLE_OBSERVATION_ROW_SCHEMA: &str = "fdgr.bundle_observation/1";
const GRAPH_SELECTION_POLICY_TEXT: &str =
    "maximum_uncertainty_margin_then_lowest_observation_id";

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct OperationBudget {
    used: u64,
    maximum: u64,
}

impl OperationBudget {
    const fn new(maximum: u64) -> Self {
        Self { used: 0, maximum }
    }

    fn charge(&mut self, _field: &'static str) -> Result<(), BundleProblemError> {
        let attempted = self
            .used
            .checked_add(1)
            .ok_or(BundleProblemError::ArithmeticOverflow {
                field: "operation_count",
            })?;
        if attempted > self.maximum {
            return Err(BundleProblemError::OperationBudgetExceeded {
                attempted,
                maximum: self.maximum,
            });
        }
        self.used = attempted;
        Ok(())
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct SupportCore {
    active_camera_node_ids: BTreeSet<u64>,
    active_landmark_ids: BTreeSet<u64>,
    pruning_rounds: u32,
}

/// Computes the exact semantic identity of a structural bundle policy.
///
/// Execution ceilings are intentionally excluded after successful completion.
///
/// # Errors
///
/// Returns a stable policy, domain, or codec error.
pub fn bundle_problem_policy_digest(
    policy: BundleProblemPolicy,
) -> Result<EvidenceDigest, BundleProblemError> {
    validate_policy(policy)?;
    let mut encoder = Encoder::with_capacity(128);
    encoder.put_str(BUNDLE_PROBLEM_POLICY_SCHEMA)?;
    encoder.put_u32(policy.min_optimize_cameras_per_landmark);
    encoder.put_u32(policy.min_active_landmarks_per_camera);
    encoder.put_u32(policy.min_root_active_landmarks);
    encoder.put_u64(policy.max_observation_uncertainty_nano_pixels);
    encoder.put_u32(policy.min_held_out_observations_per_component);
    encoder.put_u32(policy.min_held_out_cameras_per_component);
    hash_encoder(BUNDLE_PROBLEM_POLICY_SCHEMA, encoder)
}

/// Computes the exact canonical camera-binding table identity.
///
/// # Errors
///
/// Returns a stable identity, duplicate, numeric, domain, or codec error.
pub fn camera_binding_table_digest(
    bindings: &[BundleCameraBinding],
) -> Result<EvidenceDigest, BundleProblemError> {
    let bindings = canonicalize_camera_bindings(bindings.to_vec())?;
    let mut encoder = Encoder::with_capacity(64_usize.saturating_add(bindings.len() * 96));
    encoder.put_str(CAMERA_BINDING_TABLE_SCHEMA)?;
    encoder.put_u64(usize_to_u64(bindings.len())?);
    for binding in &bindings {
        encode_camera_binding(&mut encoder, binding);
    }
    hash_encoder(CAMERA_BINDING_TABLE_SCHEMA, encoder)
}

/// Computes the exact canonical landmark-seed table identity.
///
/// # Errors
///
/// Returns a stable identity, duplicate, numeric, domain, or codec error.
pub fn landmark_seed_table_digest(
    seeds: &[BundleLandmarkSeed],
) -> Result<EvidenceDigest, BundleProblemError> {
    let seeds = canonicalize_landmark_seeds(seeds.to_vec())?;
    let mut encoder = Encoder::with_capacity(64_usize.saturating_add(seeds.len() * 144));
    encoder.put_str(LANDMARK_SEED_TABLE_SCHEMA)?;
    encoder.put_u64(usize_to_u64(seeds.len())?);
    for seed in &seeds {
        encode_landmark_seed(&mut encoder, seed);
    }
    hash_encoder(LANDMARK_SEED_TABLE_SCHEMA, encoder)
}

/// Computes the exact canonical bundle-observation table identity.
///
/// # Errors
///
/// Returns a stable identity, duplicate, reuse, numeric, domain, or codec error.
pub fn bundle_observation_table_digest(
    observations: &[BundleObservation],
) -> Result<EvidenceDigest, BundleProblemError> {
    let observations = canonicalize_observations(observations.to_vec())?;
    let mut encoder =
        Encoder::with_capacity(64_usize.saturating_add(observations.len() * 144));
    encoder.put_str(BUNDLE_OBSERVATION_TABLE_SCHEMA)?;
    encoder.put_u64(usize_to_u64(observations.len())?);
    for observation in &observations {
        encode_observation(&mut encoder, observation);
    }
    hash_encoder(BUNDLE_OBSERVATION_TABLE_SCHEMA, encoder)
}

/// Returns the fixed semantic identity of the bipartite maximum-forest ordering policy.
///
/// # Errors
///
/// Returns a domain or codec error.
pub fn bundle_graph_selection_policy_digest(
) -> Result<EvidenceDigest, BundleProblemError> {
    let mut encoder = Encoder::with_capacity(128);
    encoder.put_str(BUNDLE_GRAPH_SELECTION_POLICY_SCHEMA)?;
    encoder.put_str(GRAPH_SELECTION_POLICY_TEXT)?;
    hash_encoder(BUNDLE_GRAPH_SELECTION_POLICY_SCHEMA, encoder)
}

/// Compiles one exact, structural, authority-limited relative bundle problem.
///
/// This operation authenticates all input bases, chooses the upstream accepted-or-retained camera
/// center per component, computes a deterministic support core, preserves every observation
/// disposition, and derives a bipartite topology certificate. It performs no geometric
/// optimization and emits no landmark-coordinate or metric authority.
///
/// # Errors
///
/// Returns a stable basis, upstream, binding, observation, gauge, support, graph, budget,
/// arithmetic, codec, or replay error.
pub fn compile_bundle_problem(
    basis: BundleProblemBasis,
    policy: BundleProblemPolicy,
    pose_refinement: PoseRefinementGeneration,
    camera_bindings: Vec<BundleCameraBinding>,
    landmark_seeds: Vec<BundleLandmarkSeed>,
    observations: Vec<BundleObservation>,
) -> Result<BundleProblemGeneration, BundleProblemError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    pose_refinement.validate()?;
    if pose_refinement.poses.is_empty() {
        return Err(BundleProblemError::EmptyPoseRefinement);
    }

    let observed_pose_digest = pose_refinement.digest()?;
    require_basis_match(
        "pose_refinement_digest",
        &basis.pose_refinement_digest,
        &observed_pose_digest,
    )?;
    let observed_policy_digest = bundle_problem_policy_digest(policy)?;
    require_basis_match(
        "policy_digest",
        &basis.policy_digest,
        &observed_policy_digest,
    )?;

    let camera_bindings = canonicalize_camera_bindings(camera_bindings)?;
    let landmark_seeds = canonicalize_landmark_seeds(landmark_seeds)?;
    let observations = canonicalize_observations(observations)?;
    let total_graph_subjects = camera_bindings
        .len()
        .checked_add(landmark_seeds.len())
        .ok_or(BundleProblemError::ArithmeticOverflow {
            field: "input_graph_subject_count",
        })?;
    if total_graph_subjects > MAX_BUNDLE_GRAPH_NODES {
        return Err(BundleProblemError::BoundExceeded {
            field: "input_graph_subject_count",
            actual: total_graph_subjects as u128,
            maximum: MAX_BUNDLE_GRAPH_NODES as u128,
        });
    }

    let observed_camera_digest = camera_binding_table_digest(&camera_bindings)?;
    require_basis_match(
        "camera_binding_basis_digest",
        &basis.camera_binding_basis_digest,
        &observed_camera_digest,
    )?;
    let observed_landmark_digest = landmark_seed_table_digest(&landmark_seeds)?;
    require_basis_match(
        "landmark_seed_basis_digest",
        &basis.landmark_seed_basis_digest,
        &observed_landmark_digest,
    )?;
    let observed_observation_digest = bundle_observation_table_digest(&observations)?;
    require_basis_match(
        "observation_basis_digest",
        &basis.observation_basis_digest,
        &observed_observation_digest,
    )?;

    let mut budget = OperationBudget::new(policy.max_operations);
    let pose_by_node = pose_refinement
        .poses
        .iter()
        .map(|pose| (pose.node_id, pose))
        .collect::<BTreeMap<_, _>>();
    let pose_component_by_root = pose_refinement
        .components
        .iter()
        .map(|component| (component.component_root_node_id, component))
        .collect::<BTreeMap<_, _>>();
    validate_bindings_against_poses(&camera_bindings, &pose_by_node, &mut budget)?;
    validate_landmarks_against_components(
        &landmark_seeds,
        &pose_component_by_root,
        &mut budget,
    )?;
    let binding_by_node = camera_bindings
        .iter()
        .map(|binding| (binding.camera_node_id, binding))
        .collect::<BTreeMap<_, _>>();
    let landmark_by_id = landmark_seeds
        .iter()
        .map(|seed| (seed.landmark_id, seed))
        .collect::<BTreeMap<_, _>>();
    validate_observation_cross_references(
        &observations,
        &pose_by_node,
        &binding_by_node,
        &landmark_by_id,
        &mut budget,
    )?;

    let support = derive_support_core(
        policy,
        &pose_refinement,
        &landmark_seeds,
        &observations,
        &mut budget,
    )?;
    let compiled_observations = compile_observations(policy, &observations, &support);
    let compiled_cameras = compile_cameras(
        &pose_refinement,
        &camera_bindings,
        &compiled_observations,
        &support,
        &mut budget,
    )?;
    let compiled_landmarks = compile_landmarks(
        &landmark_seeds,
        &compiled_observations,
        &support,
        &mut budget,
    )?;
    let (graph_nodes, graph) = build_bundle_graph(
        &basis,
        policy,
        &compiled_cameras,
        &compiled_landmarks,
        &compiled_observations,
        &mut budget,
    )?;
    let components = compile_components(
        policy,
        &pose_refinement,
        &compiled_cameras,
        &compiled_landmarks,
        &compiled_observations,
        &graph_nodes,
        &graph,
        &mut budget,
    )?;

    Ok(BundleProblemGeneration {
        basis,
        policy,
        pose_refinement,
        cameras: compiled_cameras,
        landmarks: compiled_landmarks,
        observations: compiled_observations,
        graph_nodes,
        graph,
        components,
        pruning_rounds: support.pruning_rounds,
        operation_count: budget.used,
    })
}
