/// Initializes component-relative camera centers from exact pose-graph orientations and relative
/// edge-scale gauges.
///
/// # Errors
///
/// Returns a stable basis, upstream-validation, scale-gauge, position, budget, arithmetic, or
/// encoding error.
pub fn initialize_global_pose(
    basis: GlobalPoseBasis,
    policy: GlobalPosePolicy,
    pose_graph: PoseGraphGeneration,
    edge_scale: EdgeScaleGeneration,
) -> Result<GlobalPoseInitialization, GlobalPoseError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    pose_graph.validate()?;
    edge_scale.validate()?;
    let pose_digest = pose_graph.digest()?;
    let scale_digest = edge_scale.digest()?;
    require_basis_digest(
        "pose_graph_generation_digest",
        &basis.pose_graph_generation_digest,
        &pose_digest,
    )?;
    require_basis_digest(
        "edge_scale_generation_digest",
        &basis.edge_scale_generation_digest,
        &scale_digest,
    )?;
    if edge_scale.basis.pose_graph_generation_digest != pose_digest {
        return Err(GlobalPoseError::EdgeScalePoseGraphMismatch {
            expected: pose_digest,
            observed: edge_scale.basis.pose_graph_generation_digest.clone(),
        });
    }

    let edge_by_id = pose_graph
        .edges
        .iter()
        .map(|edge| (edge.edge_id, edge))
        .collect::<BTreeMap<_, _>>();
    let orientation_by_node = pose_graph
        .orientations
        .iter()
        .map(|orientation| (orientation.node_id, orientation))
        .collect::<BTreeMap<_, _>>();
    let scale_by_edge = edge_scale
        .scales
        .iter()
        .map(|scale| (scale.edge_id, scale))
        .collect::<BTreeMap<_, _>>();
    let scale_component_by_root = edge_scale
        .components
        .iter()
        .map(|component| (component.scale_component_root_edge_id, component))
        .collect::<BTreeMap<_, _>>();
    let cycle_by_edge = pose_graph
        .topology
        .fundamental_cycles
        .iter()
        .map(|cycle| (cycle.closing_edge_id, cycle))
        .collect::<BTreeMap<_, _>>();
    let mut budget = OperationBudget::new(policy.max_operations);
    let mut poses = Vec::with_capacity(pose_graph.nodes.len());
    let mut translation_cycles = Vec::with_capacity(pose_graph.topology.non_forest_edge_ids.len());
    let mut components = Vec::with_capacity(pose_graph.components.len());

    for pose_component in &pose_graph.components {
        budget.charge("component_initialization")?;
        let root = pose_component.component_root_node_id;
        let forest_edge_set = pose_component
            .forest_edge_ids
            .iter()
            .copied()
            .collect::<BTreeSet<_>>();
        let non_forest_edge_ids = pose_component
            .edge_ids
            .iter()
            .copied()
            .filter(|edge_id| !forest_edge_set.contains(edge_id))
            .collect::<Vec<_>>();
        let scale_root = coherent_forest_scale_root(
            root,
            &pose_component.forest_edge_ids,
            &scale_by_edge,
        )?;
        let scale_component = scale_root
            .map(|scale_root| {
                scale_component_by_root
                    .get(&scale_root)
                    .copied()
                    .ok_or(GlobalPoseError::InternalLookup {
                        field: "scale_component_by_root",
                    })
            })
            .transpose()?;
        let (positions, parents) = propagate_component_positions(
            root,
            &pose_component.node_ids,
            &pose_component.forest_edge_ids,
            &edge_by_id,
            &orientation_by_node,
            &scale_by_edge,
            policy,
            &mut budget,
        )?;

        for node_id in &pose_component.node_ids {
            let orientation = orientation_by_node
                .get(node_id)
                .copied()
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "node_orientation",
                })?;
            let center = *positions
                .get(node_id)
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "camera_center",
                })?;
            let (parent_node_id, parent_edge_id) = parents
                .get(node_id)
                .copied()
                .unwrap_or((None, None));
            poses.push(InitializedCameraPose {
                node_id: *node_id,
                sample_index: orientation.sample_index,
                component_root_node_id: root,
                scale_component_root_edge_id: scale_root,
                rotation_node_from_root_nano: orientation.rotation_node_from_root_nano,
                camera_center_from_root_nano: center,
                parent_node_id,
                parent_edge_id,
            });
        }

        let cycle_start = translation_cycles.len();
        for closing_edge_id in &non_forest_edge_ids {
            let edge = edge_by_id
                .get(closing_edge_id)
                .copied()
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "closing_pose_edge",
                })?;
            let cycle = cycle_by_edge
                .get(closing_edge_id)
                .copied()
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "fundamental_translation_cycle",
                })?;
            let left_center = positions
                .get(&edge.left_node_id)
                .copied()
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "cycle_left_camera_center",
                })?;
            let right_center = positions
                .get(&edge.right_node_id)
                .copied()
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "cycle_right_camera_center",
                })?;
            let implied = subtract_vectors(right_center, left_center)?;
            budget.charge("translation_cycle")?;
            let edge_scale = scale_by_edge
                .get(closing_edge_id)
                .copied()
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "closing_edge_scale",
                })?;
            let (measured, residual, status) = if Some(edge_scale.scale_component_root_edge_id)
                != scale_root
            {
                (None, None, TranslationCycleStatus::IncomparableScaleGauge)
            } else {
                let measured = edge_displacement_root(
                    edge,
                    edge_scale,
                    &orientation_by_node,
                    &mut budget,
                )?;
                let residual = vector_residual_ppm(implied, measured)?;
                let status = if residual <= policy.max_translation_cycle_residual_ppm {
                    TranslationCycleStatus::Consistent
                } else {
                    TranslationCycleStatus::Conflicting
                };
                (Some(measured), Some(residual), status)
            };
            translation_cycles.push(TranslationCycleAssessment {
                closing_edge_id: *closing_edge_id,
                forest_path_edge_ids: cycle.forest_path_edge_ids.clone(),
                implied_displacement_nano: implied,
                measured_displacement_nano: measured,
                residual_ppm: residual,
                status,
            });
        }
        let component_cycles = &translation_cycles[cycle_start..];
        let conflicting_edge_ids = component_cycles
            .iter()
            .filter(|cycle| cycle.status == TranslationCycleStatus::Conflicting)
            .map(|cycle| cycle.closing_edge_id)
            .collect::<Vec<_>>();
        let incomparable_cycle_edge_ids = component_cycles
            .iter()
            .filter(|cycle| cycle.status == TranslationCycleStatus::IncomparableScaleGauge)
            .map(|cycle| cycle.closing_edge_id)
            .collect::<Vec<_>>();
        let status = component_status(
            pose_component.orientation_status,
            scale_component.map(|component| component.status),
            pose_component.node_ids.len(),
            component_cycles,
        );
        components.push(GlobalPoseComponent {
            component_root_node_id: root,
            scale_component_root_edge_id: scale_root,
            node_ids: pose_component.node_ids.clone(),
            forest_edge_ids: pose_component.forest_edge_ids.clone(),
            non_forest_edge_ids,
            status,
            scale_cross_validated: scale_component.is_some_and(|component| component.cross_validated),
            conflicting_edge_ids,
            incomparable_cycle_edge_ids,
        });
    }

    poses.sort_by_key(|pose| pose.node_id);
    translation_cycles.sort_by_key(|cycle| cycle.closing_edge_id);
    components.sort_by_key(|component| component.component_root_node_id);
    Ok(GlobalPoseInitialization {
        basis,
        policy,
        pose_graph,
        edge_scale,
        poses,
        translation_cycles,
        components,
        operation_count: budget.used,
    })
}

fn validate_basis(basis: &GlobalPoseBasis) -> Result<(), GlobalPoseError> {
    for (field, digest) in [
        (
            "pose_graph_generation_digest",
            &basis.pose_graph_generation_digest,
        ),
        (
            "edge_scale_generation_digest",
            &basis.edge_scale_generation_digest,
        ),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_nonzero("generation", basis.generation)
}

fn validate_policy(policy: GlobalPosePolicy) -> Result<(), GlobalPoseError> {
    if policy.max_translation_cycle_residual_ppm > MAX_TRANSLATION_RESIDUAL_PPM {
        return Err(GlobalPoseError::BoundExceeded {
            field: "max_translation_cycle_residual_ppm",
            actual: u128::from(policy.max_translation_cycle_residual_ppm),
            maximum: u128::from(MAX_TRANSLATION_RESIDUAL_PPM),
        });
    }
    if policy.max_camera_center_abs_nano == 0 {
        return Err(GlobalPoseError::ZeroValue {
            field: "max_camera_center_abs_nano",
        });
    }
    if policy.max_camera_center_abs_nano > MAX_CAMERA_CENTER_ABS_NANO {
        return Err(GlobalPoseError::BoundExceeded {
            field: "max_camera_center_abs_nano",
            actual: u128::from(policy.max_camera_center_abs_nano),
            maximum: u128::from(MAX_CAMERA_CENTER_ABS_NANO),
        });
    }
    if policy.max_operations == 0 {
        return Err(GlobalPoseError::ZeroValue {
            field: "max_operations",
        });
    }
    if policy.max_operations > MAX_GLOBAL_POSE_OPERATIONS {
        return Err(GlobalPoseError::BoundExceeded {
            field: "max_operations",
            actual: u128::from(policy.max_operations),
            maximum: u128::from(MAX_GLOBAL_POSE_OPERATIONS),
        });
    }
    Ok(())
}

fn require_basis_digest(
    field: &'static str,
    expected: &EvidenceDigest,
    observed: &EvidenceDigest,
) -> Result<(), GlobalPoseError> {
    if expected == observed {
        Ok(())
    } else {
        Err(GlobalPoseError::BasisMismatch {
            field,
            expected: expected.clone(),
            observed: observed.clone(),
        })
    }
}

fn coherent_forest_scale_root(
    component_root_node_id: u64,
    forest_edge_ids: &[u64],
    scale_by_edge: &BTreeMap<u64, &ReconciledEdgeScale>,
) -> Result<Option<u64>, GlobalPoseError> {
    let mut roots = BTreeSet::new();
    for edge_id in forest_edge_ids {
        let scale = scale_by_edge
            .get(edge_id)
            .copied()
            .ok_or(GlobalPoseError::InternalLookup {
                field: "forest_edge_scale",
            })?;
        roots.insert(scale.scale_component_root_edge_id);
    }
    if roots.len() > 1 {
        return Err(GlobalPoseError::IncomparableScaleComponents {
            component_root_node_id,
            scale_component_root_edge_ids: roots.into_iter().collect(),
        });
    }
    Ok(roots.into_iter().next())
}

#[allow(clippy::too_many_arguments)]
fn propagate_component_positions(
    root: u64,
    node_ids: &[u64],
    forest_edge_ids: &[u64],
    edge_by_id: &BTreeMap<u64, &RelativePoseEdge>,
    orientation_by_node: &BTreeMap<u64, &fdgr_pose_graph::NodeOrientation>,
    scale_by_edge: &BTreeMap<u64, &ReconciledEdgeScale>,
    policy: GlobalPosePolicy,
    budget: &mut OperationBudget,
) -> Result<
    (
        BTreeMap<u64, [i64; 3]>,
        BTreeMap<u64, (Option<u64>, Option<u64>)>,
    ),
    GlobalPoseError,
> {
    let mut adjacency = node_ids
        .iter()
        .copied()
        .map(|node_id| (node_id, Vec::<(u64, u64)>::new()))
        .collect::<BTreeMap<_, _>>();
    for edge_id in forest_edge_ids {
        let edge = edge_by_id
            .get(edge_id)
            .copied()
            .ok_or(GlobalPoseError::InternalLookup {
                field: "forest_pose_edge",
            })?;
        adjacency
            .get_mut(&edge.left_node_id)
            .ok_or(GlobalPoseError::InternalLookup {
                field: "left_forest_adjacency",
            })?
            .push((edge.right_node_id, edge.edge_id));
        adjacency
            .get_mut(&edge.right_node_id)
            .ok_or(GlobalPoseError::InternalLookup {
                field: "right_forest_adjacency",
            })?
            .push((edge.left_node_id, edge.edge_id));
    }
    for neighbors in adjacency.values_mut() {
        neighbors.sort_unstable();
    }
    let mut positions = BTreeMap::new();
    let mut parents = BTreeMap::new();
    positions.insert(root, [0, 0, 0]);
    parents.insert(root, (None, None));
    let mut queue = VecDeque::new();
    queue.push_back(root);
    while let Some(parent_id) = queue.pop_front() {
        budget.charge("forest_node")?;
        let parent_center = *positions
            .get(&parent_id)
            .ok_or(GlobalPoseError::InternalLookup {
                field: "parent_camera_center",
            })?;
        let neighbors = adjacency
            .get(&parent_id)
            .ok_or(GlobalPoseError::InternalLookup {
                field: "forest_adjacency",
            })?;
        for (child_id, edge_id) in neighbors {
            if positions.contains_key(child_id) {
                continue;
            }
            let edge = edge_by_id
                .get(edge_id)
                .copied()
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "position_pose_edge",
                })?;
            let scale = scale_by_edge
                .get(edge_id)
                .copied()
                .ok_or(GlobalPoseError::InternalLookup {
                    field: "position_edge_scale",
                })?;
            let forward = edge.left_node_id == parent_id && edge.right_node_id == *child_id;
            let reverse = edge.right_node_id == parent_id && edge.left_node_id == *child_id;
            if !forward && !reverse {
                return Err(GlobalPoseError::InternalLookup {
                    field: "forest_edge_direction",
                });
            }
            let displacement = edge_displacement_root(
                edge,
                scale,
                orientation_by_node,
                budget,
            )?;
            let child_center = if forward {
                add_vectors(parent_center, displacement)?
            } else {
                subtract_vectors(parent_center, displacement)?
            };
            validate_position(*child_id, child_center, policy.max_camera_center_abs_nano)?;
            positions.insert(*child_id, child_center);
            parents.insert(*child_id, (Some(parent_id), Some(*edge_id)));
            queue.push_back(*child_id);
        }
    }
    if positions.len() != node_ids.len() {
        return Err(GlobalPoseError::InternalLookup {
            field: "complete_camera_center_coverage",
        });
    }
    Ok((positions, parents))
}

fn edge_displacement_root(
    edge: &RelativePoseEdge,
    scale: &ReconciledEdgeScale,
    orientation_by_node: &BTreeMap<u64, &fdgr_pose_graph::NodeOrientation>,
    budget: &mut OperationBudget,
) -> Result<[i64; 3], GlobalPoseError> {
    budget.charge("edge_displacement")?;
    let right_orientation = orientation_by_node
        .get(&edge.right_node_id)
        .copied()
        .ok_or(GlobalPoseError::InternalLookup {
            field: "right_edge_orientation",
        })?;
    let scaled_translation = scale_direction(
        edge.translation_right_from_left_direction_nano,
        scale.relative_scale_nano,
    )?;
    let in_root = transpose_matrix_vector(
        right_orientation.rotation_node_from_root_nano,
        scaled_translation,
    )?;
    negate_vector(in_root)
}

fn component_status(
    orientation_status: OrientationStatus,
    scale_status: Option<EdgeScaleComponentStatus>,
    node_count: usize,
    cycles: &[TranslationCycleAssessment],
) -> GlobalPoseComponentStatus {
    if orientation_status == OrientationStatus::Conflicted {
        return GlobalPoseComponentStatus::OrientationConflicted;
    }
    if scale_status == Some(EdgeScaleComponentStatus::Conflicted) {
        return GlobalPoseComponentStatus::ScaleConflicted;
    }
    if cycles
        .iter()
        .any(|cycle| cycle.status == TranslationCycleStatus::Conflicting)
    {
        return GlobalPoseComponentStatus::TranslationConflicted;
    }
    if node_count == 1 {
        return GlobalPoseComponentStatus::Singleton;
    }
    if !cycles.is_empty()
        && cycles
            .iter()
            .all(|cycle| cycle.status == TranslationCycleStatus::Consistent)
    {
        GlobalPoseComponentStatus::CycleConsistent
    } else {
        GlobalPoseComponentStatus::TreeInitialized
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct OperationBudget {
    used: u64,
    maximum: u64,
}

impl OperationBudget {
    const fn new(maximum: u64) -> Self {
        Self { used: 0, maximum }
    }

    fn charge(&mut self, _field: &'static str) -> Result<(), GlobalPoseError> {
        let attempted = self
            .used
            .checked_add(1)
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "operation_count",
            })?;
        if attempted > self.maximum {
            return Err(GlobalPoseError::OperationBudgetExceeded {
                attempted,
                maximum: self.maximum,
            });
        }
        self.used = attempted;
        Ok(())
    }
}

fn scale_direction(direction: [i64; 3], scale_nano: u64) -> Result<[i64; 3], GlobalPoseError> {
    let scale = i128::from(scale_nano);
    let denominator = i128::from(UNIT_NANO);
    let mut output = [0_i64; 3];
    for index in 0..3 {
        let product = i128::from(direction[index])
            .checked_mul(scale)
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "scaled_translation",
            })?;
        output[index] = i128_to_i64(
            divide_round_i128(product, denominator, "scaled_translation")?,
            "scaled_translation",
        )?;
    }
    Ok(output)
}

fn transpose_matrix_vector(
    matrix: [i64; 9],
    vector: [i64; 3],
) -> Result<[i64; 3], GlobalPoseError> {
    let denominator = i128::from(UNIT_NANO);
    let mut output = [0_i64; 3];
    for row in 0..3 {
        let mut total = 0_i128;
        for column in 0..3 {
            let coefficient = i128::from(matrix[column * 3 + row]);
            total = total
                .checked_add(
                    coefficient
                        .checked_mul(i128::from(vector[column]))
                        .ok_or(GlobalPoseError::ArithmeticOverflow {
                            field: "transpose_matrix_vector_product",
                        })?,
                )
                .ok_or(GlobalPoseError::ArithmeticOverflow {
                    field: "transpose_matrix_vector_sum",
                })?;
        }
        output[row] = i128_to_i64(
            divide_round_i128(total, denominator, "transpose_matrix_vector")?,
            "transpose_matrix_vector",
        )?;
    }
    Ok(output)
}

fn divide_round_i128(
    value: i128,
    denominator: i128,
    field: &'static str,
) -> Result<i128, GlobalPoseError> {
    if denominator <= 0 {
        return Err(GlobalPoseError::ZeroValue { field });
    }
    let half = denominator / 2;
    if value >= 0 {
        value
            .checked_add(half)
            .map(|rounded| rounded / denominator)
            .ok_or(GlobalPoseError::ArithmeticOverflow { field })
    } else {
        let magnitude = value
            .checked_neg()
            .ok_or(GlobalPoseError::ArithmeticOverflow { field })?;
        magnitude
            .checked_add(half)
            .map(|rounded| -(rounded / denominator))
            .ok_or(GlobalPoseError::ArithmeticOverflow { field })
    }
}

fn i128_to_i64(value: i128, field: &'static str) -> Result<i64, GlobalPoseError> {
    i64::try_from(value).map_err(|_| GlobalPoseError::ArithmeticOverflow { field })
}

fn add_vectors(left: [i64; 3], right: [i64; 3]) -> Result<[i64; 3], GlobalPoseError> {
    Ok([
        left[0]
            .checked_add(right[0])
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_center_add",
            })?,
        left[1]
            .checked_add(right[1])
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_center_add",
            })?,
        left[2]
            .checked_add(right[2])
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_center_add",
            })?,
    ])
}

fn subtract_vectors(left: [i64; 3], right: [i64; 3]) -> Result<[i64; 3], GlobalPoseError> {
    Ok([
        left[0]
            .checked_sub(right[0])
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_center_subtract",
            })?,
        left[1]
            .checked_sub(right[1])
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_center_subtract",
            })?,
        left[2]
            .checked_sub(right[2])
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_center_subtract",
            })?,
    ])
}

fn negate_vector(value: [i64; 3]) -> Result<[i64; 3], GlobalPoseError> {
    Ok([
        value[0]
            .checked_neg()
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_displacement_negation",
            })?,
        value[1]
            .checked_neg()
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_displacement_negation",
            })?,
        value[2]
            .checked_neg()
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "camera_displacement_negation",
            })?,
    ])
}

fn vector_residual_ppm(
    implied: [i64; 3],
    measured: [i64; 3],
) -> Result<u64, GlobalPoseError> {
    let mut difference = 0_u128;
    let mut implied_norm = 0_u128;
    let mut measured_norm = 0_u128;
    for index in 0..3 {
        difference = difference
            .checked_add(u128::from(implied[index].abs_diff(measured[index])))
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "translation_cycle_difference",
            })?;
        implied_norm = implied_norm
            .checked_add(u128::from(implied[index].unsigned_abs()))
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "translation_cycle_norm",
            })?;
        measured_norm = measured_norm
            .checked_add(u128::from(measured[index].unsigned_abs()))
            .ok_or(GlobalPoseError::ArithmeticOverflow {
                field: "translation_cycle_norm",
            })?;
    }
    let denominator = implied_norm.max(measured_norm);
    if denominator == 0 {
        return Ok(0);
    }
    let numerator = difference
        .checked_mul(u128::from(PPM_SCALE))
        .ok_or(GlobalPoseError::ArithmeticOverflow {
            field: "translation_cycle_residual_ppm",
        })?;
    let rounded = numerator
        .checked_add(denominator.saturating_sub(1))
        .ok_or(GlobalPoseError::ArithmeticOverflow {
            field: "translation_cycle_residual_ppm",
        })?
        / denominator;
    u64::try_from(rounded).map_err(|_| GlobalPoseError::ArithmeticOverflow {
        field: "translation_cycle_residual_ppm",
    })
}

fn validate_position(
    node_id: u64,
    position: [i64; 3],
    maximum: u64,
) -> Result<(), GlobalPoseError> {
    for coordinate in position {
        let actual = coordinate.unsigned_abs();
        if actual > maximum {
            return Err(GlobalPoseError::PositionBoundExceeded {
                node_id,
                actual: u128::from(actual),
                maximum: u128::from(maximum),
            });
        }
    }
    Ok(())
}

impl GlobalPoseInitialization {
    /// Replays every derived field from the exact retained upstream generations.
    ///
    /// # Errors
    ///
    /// Returns a stable upstream, construction, budget, or deterministic mismatch.
    pub fn validate(&self) -> Result<(), GlobalPoseError> {
        let rebuilt = initialize_global_pose(
            self.basis.clone(),
            self.policy,
            self.pose_graph.clone(),
            self.edge_scale.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(GlobalPoseError::DerivedMismatch {
                field: "global_pose_initialization",
            })
        }
    }

    /// Returns deterministic canonical semantic bytes.
    ///
    /// `max_operations` and `operation_count` remain visible operation evidence but are excluded
    /// from successful semantic identity.
    ///
    /// # Errors
    ///
    /// Returns a validation or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, GlobalPoseError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(
            2_048_usize
                .saturating_add(self.poses.len().saturating_mul(192))
                .saturating_add(self.translation_cycles.len().saturating_mul(128)),
        );
        encoder.put_str(GLOBAL_POSE_INITIALIZATION_SCHEMA)?;
        encoder.put_digest(&self.basis.pose_graph_generation_digest);
        encoder.put_digest(&self.basis.edge_scale_generation_digest);
        encoder.put_digest(&self.basis.policy_digest);
        encoder.put_u64(self.basis.generation);
        encoder.put_u64(self.policy.max_translation_cycle_residual_ppm);
        encoder.put_u64(self.policy.max_camera_center_abs_nano);
        encoder.put_u64(usize_to_u64(self.poses.len())?);
        for pose in &self.poses {
            encoder.put_u64(pose.node_id);
            encoder.put_u64(pose.sample_index);
            encoder.put_u64(pose.component_root_node_id);
            encode_optional_u64(&mut encoder, pose.scale_component_root_edge_id);
            for value in pose.rotation_node_from_root_nano {
                encoder.put_i64(value);
            }
            for value in pose.camera_center_from_root_nano {
                encoder.put_i64(value);
            }
            encode_optional_u64(&mut encoder, pose.parent_node_id);
            encode_optional_u64(&mut encoder, pose.parent_edge_id);
        }
        encoder.put_u64(usize_to_u64(self.translation_cycles.len())?);
        for cycle in &self.translation_cycles {
            encoder.put_u64(cycle.closing_edge_id);
            encode_u64s(&mut encoder, &cycle.forest_path_edge_ids)?;
            for value in cycle.implied_displacement_nano {
                encoder.put_i64(value);
            }
            encode_optional_vector(&mut encoder, cycle.measured_displacement_nano);
            encode_optional_u64(&mut encoder, cycle.residual_ppm);
            encoder.put_u8(cycle.status.code());
        }
        encoder.put_u64(usize_to_u64(self.components.len())?);
        for component in &self.components {
            encoder.put_u64(component.component_root_node_id);
            encode_optional_u64(&mut encoder, component.scale_component_root_edge_id);
            encode_u64s(&mut encoder, &component.node_ids)?;
            encode_u64s(&mut encoder, &component.forest_edge_ids)?;
            encode_u64s(&mut encoder, &component.non_forest_edge_ids)?;
            encoder.put_u8(component.status.code());
            encoder.put_bool(component.scale_cross_validated);
            encode_u64s(&mut encoder, &component.conflicting_edge_ids)?;
            encode_u64s(&mut encoder, &component.incomparable_cycle_edge_ids)?;
        }
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated semantic initialization identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, GlobalPoseError> {
        let domain = DigestDomain::parse(GLOBAL_POSE_INITIALIZATION_SCHEMA)?;
        Ok(hash_domain(&domain, &self.to_canonical_bytes()?)?)
    }

    /// Renders deterministic field-ordered JSON.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, GlobalPoseError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{GLOBAL_POSE_INITIALIZATION_SCHEMA}\",\"initialization_digest\":\"{digest}\",\"authority\":\"{GLOBAL_POSE_AUTHORITY}\",\"unit\":\"component_edge_scale_unit_nano\",\"pose_graph_generation_digest\":\"{}\",\"edge_scale_generation_digest\":\"{}\",\"policy_digest\":\"{}\",\"generation\":{},\"pose_count\":{},\"translation_cycle_count\":{},\"component_count\":{},\"operation_count\":{},\"policy\":{{\"max_translation_cycle_residual_ppm\":{},\"max_camera_center_abs_nano\":{},\"max_operations\":{}}},\"poses\":[",
            self.basis.pose_graph_generation_digest,
            self.basis.edge_scale_generation_digest,
            self.basis.policy_digest,
            self.basis.generation,
            self.poses.len(),
            self.translation_cycles.len(),
            self.components.len(),
            self.operation_count,
            self.policy.max_translation_cycle_residual_ppm,
            self.policy.max_camera_center_abs_nano,
            self.policy.max_operations,
        );
        for (index, pose) in self.poses.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"node_id\":{},\"sample_index\":{},\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"rotation_node_from_root_nano\":[",
                pose.node_id,
                pose.sample_index,
                pose.component_root_node_id,
                optional_u64_json(pose.scale_component_root_edge_id),
            )
            .map_err(json_error)?;
            push_i64s(&mut output, &pose.rotation_node_from_root_nano)?;
            output.push_str("],\"camera_center_from_root_nano\":[");
            push_i64s(&mut output, &pose.camera_center_from_root_nano)?;
            write!(
                output,
                "],\"parent_node_id\":{},\"parent_edge_id\":{}}}",
                optional_u64_json(pose.parent_node_id),
                optional_u64_json(pose.parent_edge_id),
            )
            .map_err(json_error)?;
        }
        output.push_str("],\"translation_cycles\":[");
        for (index, cycle) in self.translation_cycles.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"closing_edge_id\":{},\"forest_path_edge_ids\":[",
                cycle.closing_edge_id,
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &cycle.forest_path_edge_ids)?;
            output.push_str("],\"implied_displacement_nano\":[");
            push_i64s(&mut output, &cycle.implied_displacement_nano)?;
            output.push_str("],\"measured_displacement_nano\":");
            push_optional_vector_json(&mut output, cycle.measured_displacement_nano)?;
            write!(
                output,
                ",\"residual_ppm\":{},\"status\":\"{}\"}}",
                optional_u64_json(cycle.residual_ppm),
                cycle.status.as_str(),
            )
            .map_err(json_error)?;
        }
        output.push_str("],\"components\":[");
        for (index, component) in self.components.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"status\":\"{}\",\"scale_cross_validated\":{},\"node_ids\":[",
                component.component_root_node_id,
                optional_u64_json(component.scale_component_root_edge_id),
                component.status.as_str(),
                component.scale_cross_validated,
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &component.node_ids)?;
            output.push_str("],\"forest_edge_ids\":[");
            push_u64s(&mut output, &component.forest_edge_ids)?;
            output.push_str("],\"non_forest_edge_ids\":[");
            push_u64s(&mut output, &component.non_forest_edge_ids)?;
            output.push_str("],\"conflicting_edge_ids\":[");
            push_u64s(&mut output, &component.conflicting_edge_ids)?;
            output.push_str("],\"incomparable_cycle_edge_ids\":[");
            push_u64s(&mut output, &component.incomparable_cycle_edge_ids)?;
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_optional_u64(encoder: &mut Encoder, value: Option<u64>) {
    match value {
        Some(value) => {
            encoder.put_bool(true);
            encoder.put_u64(value);
        }
        None => encoder.put_bool(false),
    }
}

fn encode_optional_vector(encoder: &mut Encoder, value: Option<[i64; 3]>) {
    match value {
        Some(vector) => {
            encoder.put_bool(true);
            for coordinate in vector {
                encoder.put_i64(coordinate);
            }
        }
        None => encoder.put_bool(false),
    }
}

fn encode_u64s(encoder: &mut Encoder, values: &[u64]) -> Result<(), GlobalPoseError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn push_i64s<const N: usize>(
    output: &mut String,
    values: &[i64; N],
) -> Result<(), GlobalPoseError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn push_u64s(output: &mut String, values: &[u64]) -> Result<(), GlobalPoseError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn push_optional_vector_json(
    output: &mut String,
    value: Option<[i64; 3]>,
) -> Result<(), GlobalPoseError> {
    if let Some(vector) = value {
        output.push('[');
        push_i64s(output, &vector)?;
        output.push(']');
    } else {
        output.push_str("null");
    }
    Ok(())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn json_error(error: fmt::Error) -> GlobalPoseError {
    GlobalPoseError::JsonRendering(error.to_string())
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), GlobalPoseError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(GlobalPoseError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn validate_safe_nonzero(field: &'static str, value: u64) -> Result<(), GlobalPoseError> {
    if value == 0 {
        return Err(GlobalPoseError::ZeroValue { field });
    }
    if value > MAX_SAFE_JSON_INTEGER {
        return Err(GlobalPoseError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        });
    }
    Ok(())
}

fn usize_to_u64(value: usize) -> Result<u64, GlobalPoseError> {
    u64::try_from(value).map_err(|_| GlobalPoseError::ArithmeticOverflow {
        field: "collection_length",
    })
}
