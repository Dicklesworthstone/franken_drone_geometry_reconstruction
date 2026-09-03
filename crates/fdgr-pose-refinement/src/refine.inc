#[derive(Clone, Debug, Eq, PartialEq)]
struct RefinementFactor {
    edge_id: u64,
    component_root_node_id: u64,
    left_node_id: u64,
    right_node_id: u64,
    base_weight: u32,
    robust_weight: u32,
    measured_displacement_nano: Option<[i64; 3]>,
    blocked_upstream_conflict: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct Objective {
    weighted_squared_error: u128,
    coordinate_weight: u128,
}

impl Objective {
    fn rms_nano(self) -> Result<u64, PoseRefinementError> {
        if self.coordinate_weight == 0 {
            return Ok(0);
        }
        let rounded_mean_square = self
            .weighted_squared_error
            .checked_add(self.coordinate_weight.saturating_sub(1))
            .ok_or(PoseRefinementError::ArithmeticOverflow {
                field: "objective_mean_square",
            })?
            / self.coordinate_weight;
        integer_sqrt(rounded_mean_square)
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

    fn charge(&mut self, _field: &'static str) -> Result<(), PoseRefinementError> {
        let attempted =
            self.used
                .checked_add(1)
                .ok_or(PoseRefinementError::ArithmeticOverflow {
                    field: "operation_count",
                })?;
        if attempted > self.maximum {
            return Err(PoseRefinementError::OperationBudgetExceeded {
                attempted,
                maximum: self.maximum,
            });
        }
        self.used = attempted;
        Ok(())
    }
}

/// Refines component-relative camera centers against fixed admitted pose-edge displacements.
///
/// Rotations, edge scales, component membership, and component gauges are immutable inputs. Every
/// component root remains pinned to its initialized zero origin. Orientation- or scale-conflicted
/// components are retained and reported as blocked rather than silently optimized.
///
/// # Errors
///
/// Returns a stable basis, upstream-validation, gauge, bound, budget, arithmetic, codec, or replay
/// error.
pub fn refine_pose_translation(
    basis: PoseRefinementBasis,
    policy: PoseRefinementPolicy,
    initialization: GlobalPoseInitialization,
) -> Result<PoseRefinementGeneration, PoseRefinementError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    initialization.validate()?;
    let observed_initialization_digest = initialization.digest()?;
    if observed_initialization_digest != basis.initialization_digest {
        return Err(PoseRefinementError::BasisMismatch {
            expected: basis.initialization_digest,
            observed: observed_initialization_digest,
        });
    }

    let mut budget = OperationBudget::new(policy.max_operations);
    let pose_by_node = initialization
        .poses
        .iter()
        .map(|pose| (pose.node_id, pose))
        .collect::<BTreeMap<_, _>>();
    let component_by_root = initialization
        .components
        .iter()
        .map(|component| (component.component_root_node_id, component))
        .collect::<BTreeMap<_, _>>();
    let scale_by_edge = initialization
        .edge_scale
        .scales
        .iter()
        .map(|scale| (scale.edge_id, scale))
        .collect::<BTreeMap<_, _>>();
    let initial_positions = initialization
        .poses
        .iter()
        .map(|pose| (pose.node_id, pose.camera_center_from_root_nano))
        .collect::<BTreeMap<_, _>>();
    let mut positions = initial_positions.clone();
    for pose in &initialization.poses {
        validate_position(
            pose.node_id,
            pose.camera_center_from_root_nano,
            policy.max_camera_center_abs_nano,
        )?;
    }

    let mut factors = Vec::with_capacity(initialization.pose_graph.edges.len());
    for edge in &initialization.pose_graph.edges {
        budget.charge("factor_setup")?;
        let left_pose = pose_by_node
            .get(&edge.left_node_id)
            .copied()
            .ok_or(PoseRefinementError::InternalLookup {
                field: "left_pose",
            })?;
        let right_pose = pose_by_node
            .get(&edge.right_node_id)
            .copied()
            .ok_or(PoseRefinementError::InternalLookup {
                field: "right_pose",
            })?;
        if left_pose.component_root_node_id != right_pose.component_root_node_id {
            return Err(PoseRefinementError::CrossComponentEdge {
                edge_id: edge.edge_id,
                left_component_root_node_id: left_pose.component_root_node_id,
                right_component_root_node_id: right_pose.component_root_node_id,
            });
        }
        let component = component_by_root
            .get(&left_pose.component_root_node_id)
            .copied()
            .ok_or(PoseRefinementError::InternalLookup {
                field: "factor_component",
            })?;
        let scale = scale_by_edge
            .get(&edge.edge_id)
            .copied()
            .ok_or(PoseRefinementError::InternalLookup {
                field: "factor_edge_scale",
            })?;
        let comparable =
            component.scale_component_root_edge_id == Some(scale.scale_component_root_edge_id);
        let blocked_upstream_conflict = matches!(
            component.status,
            GlobalPoseComponentStatus::OrientationConflicted
                | GlobalPoseComponentStatus::ScaleConflicted
        );
        let measured = measured_edge_displacement_root(
            edge,
            scale,
            right_pose.rotation_node_from_root_nano,
        )?;
        let measured_displacement_nano = comparable.then_some(measured);
        let base_weight = edge.supported_match_count.min(policy.max_factor_weight);
        let robust_weight = if comparable && !blocked_upstream_conflict {
            let left = *initial_positions.get(&edge.left_node_id).ok_or(
                PoseRefinementError::InternalLookup {
                    field: "initial_left_position",
                },
            )?;
            let right = *initial_positions.get(&edge.right_node_id).ok_or(
                PoseRefinementError::InternalLookup {
                    field: "initial_right_position",
                },
            )?;
            let implied = subtract_vectors(right, left, "initial_implied_displacement")?;
            let residual = residual_linf(implied, measured)?;
            initial_robust_weight(base_weight, policy.huber_delta_nano, residual)?
        } else {
            0
        };
        factors.push(RefinementFactor {
            edge_id: edge.edge_id,
            component_root_node_id: component.component_root_node_id,
            left_node_id: edge.left_node_id,
            right_node_id: edge.right_node_id,
            base_weight,
            robust_weight,
            measured_displacement_nano,
            blocked_upstream_conflict,
        });
    }
    factors.sort_by_key(|factor| factor.edge_id);

    let mut adjacency = initialization
        .poses
        .iter()
        .map(|pose| (pose.node_id, Vec::<usize>::new()))
        .collect::<BTreeMap<_, _>>();
    for (index, factor) in factors.iter().enumerate() {
        if factor.robust_weight == 0 {
            continue;
        }
        adjacency
            .get_mut(&factor.left_node_id)
            .ok_or(PoseRefinementError::InternalLookup {
                field: "left_factor_adjacency",
            })?
            .push(index);
        adjacency
            .get_mut(&factor.right_node_id)
            .ok_or(PoseRefinementError::InternalLookup {
                field: "right_factor_adjacency",
            })?
            .push(index);
    }

    let mut components = Vec::with_capacity(initialization.components.len());
    let mut iteration_count = 0_u64;
    for component in &initialization.components {
        budget.charge("component_setup")?;
        let factor_indices = factors
            .iter()
            .enumerate()
            .filter_map(|(index, factor)| {
                (factor.component_root_node_id == component.component_root_node_id).then_some(index)
            })
            .collect::<Vec<_>>();
        let active_factor_indices = factor_indices
            .iter()
            .copied()
            .filter(|index| factors[*index].robust_weight > 0)
            .collect::<Vec<_>>();
        let factor_edge_ids = factor_indices
            .iter()
            .map(|index| factors[*index].edge_id)
            .collect::<Vec<_>>();
        let active_factor_edge_ids = active_factor_indices
            .iter()
            .map(|index| factors[*index].edge_id)
            .collect::<Vec<_>>();
        let blocked = matches!(
            component.status,
            GlobalPoseComponentStatus::OrientationConflicted
                | GlobalPoseComponentStatus::ScaleConflicted
        );

        let (
            status,
            decision,
            reason,
            iterations,
            initial_rms_residual_nano,
            final_rms_residual_nano,
            rms_improvement_nano,
        ) = if blocked {
            (
                PoseRefinementComponentStatus::BlockedUpstreamConflict,
                PoseRefinementDecision::Blocked,
                PoseRefinementReason::UpstreamConflict,
                0,
                None,
                None,
                None,
            )
        } else if component.node_ids.len() == 1 {
            (
                PoseRefinementComponentStatus::Singleton,
                PoseRefinementDecision::RetainInitialization,
                PoseRefinementReason::Singleton,
                0,
                None,
                None,
                None,
            )
        } else if active_factor_indices.is_empty() {
            (
                PoseRefinementComponentStatus::NoComparableFactors,
                PoseRefinementDecision::RetainInitialization,
                PoseRefinementReason::NoComparableFactors,
                0,
                None,
                None,
                None,
            )
        } else {
            let initial_objective = component_objective(
                &positions,
                &factors,
                &active_factor_indices,
                &mut budget,
            )?;
            let initial_rms = initial_objective.rms_nano()?;
            if initial_objective.weighted_squared_error == 0 {
                (
                    PoseRefinementComponentStatus::AlreadySatisfied,
                    PoseRefinementDecision::RetainInitialization,
                    PoseRefinementReason::AlreadySatisfied,
                    0,
                    Some(initial_rms),
                    Some(initial_rms),
                    Some(0),
                )
            } else {
                let mut iterations = 0_u32;
                let mut terminal_status = PoseRefinementComponentStatus::IterationLimit;
                loop {
                    if iterations >= policy.max_iterations {
                        break;
                    }
                    let before_positions = snapshot_positions(&component.node_ids, &positions)?;
                    let before_objective = component_objective(
                        &positions,
                        &factors,
                        &active_factor_indices,
                        &mut budget,
                    )?;
                    let mut max_step = 0_u64;
                    for node_id in &component.node_ids {
                        if *node_id == component.component_root_node_id {
                            continue;
                        }
                        budget.charge("node_update")?;
                        let incident = adjacency
                            .get(node_id)
                            .ok_or(PoseRefinementError::InternalLookup {
                                field: "node_factor_adjacency",
                            })?;
                        if incident.is_empty() {
                            continue;
                        }
                        let current = *positions.get(node_id).ok_or(
                            PoseRefinementError::InternalLookup {
                                field: "current_node_position",
                            },
                        )?;
                        let target = weighted_node_target(
                            *node_id,
                            current,
                            incident,
                            &positions,
                            &factors,
                            policy.damping_weight,
                            &mut budget,
                        )?;
                        validate_position(
                            *node_id,
                            target,
                            policy.max_camera_center_abs_nano,
                        )?;
                        max_step = max_step.max(vector_linf_difference(current, target)?);
                        positions.insert(*node_id, target);
                    }
                    iterations = iterations.checked_add(1).ok_or(
                        PoseRefinementError::ArithmeticOverflow {
                            field: "component_iterations",
                        },
                    )?;
                    let after_objective = component_objective(
                        &positions,
                        &factors,
                        &active_factor_indices,
                        &mut budget,
                    )?;
                    if after_objective.weighted_squared_error
                        >= before_objective.weighted_squared_error
                    {
                        restore_positions(&before_positions, &mut positions);
                        terminal_status = PoseRefinementComponentStatus::NoImprovement;
                        break;
                    }
                    if max_step <= policy.convergence_delta_nano {
                        terminal_status = PoseRefinementComponentStatus::Converged;
                        break;
                    }
                    if iterations >= policy.max_iterations {
                        terminal_status = PoseRefinementComponentStatus::IterationLimit;
                        break;
                    }
                }
                let final_objective = component_objective(
                    &positions,
                    &factors,
                    &active_factor_indices,
                    &mut budget,
                )?;
                let final_rms = final_objective.rms_nano()?;
                let improved =
                    final_objective.weighted_squared_error < initial_objective.weighted_squared_error;
                let decision = if improved {
                    PoseRefinementDecision::AcceptRefined
                } else {
                    PoseRefinementDecision::RetainInitialization
                };
                let reason = if improved {
                    PoseRefinementReason::ObjectiveImproved
                } else {
                    PoseRefinementReason::NoImprovement
                };
                (
                    terminal_status,
                    decision,
                    reason,
                    iterations,
                    Some(initial_rms),
                    Some(final_rms),
                    Some(initial_rms.saturating_sub(final_rms)),
                )
            }
        };

        iteration_count = iteration_count.checked_add(u64::from(iterations)).ok_or(
            PoseRefinementError::ArithmeticOverflow {
                field: "iteration_count",
            },
        )?;
        let max_adjustment_nano =
            component_max_adjustment(&component.node_ids, &initial_positions, &positions)?;
        let mut downweighted_edge_ids = Vec::new();
        for index in &active_factor_indices {
            let factor = factors
                .get(*index)
                .ok_or(PoseRefinementError::InternalLookup {
                    field: "component_active_factor",
                })?;
            if factor.robust_weight < factor.base_weight {
                downweighted_edge_ids.push(factor.edge_id);
            }
        }
        components.push(PoseRefinementComponent {
            component_root_node_id: component.component_root_node_id,
            scale_component_root_edge_id: component.scale_component_root_edge_id,
            status,
            decision,
            reason,
            iterations,
            initial_rms_residual_nano,
            final_rms_residual_nano,
            rms_improvement_nano,
            max_adjustment_nano,
            do_nothing_dominates: decision != PoseRefinementDecision::AcceptRefined,
            node_ids: component.node_ids.clone(),
            factor_edge_ids,
            active_factor_edge_ids,
            downweighted_edge_ids,
        });
    }
    components.sort_by_key(|component| component.component_root_node_id);

    let mut poses = Vec::with_capacity(initialization.poses.len());
    for pose in &initialization.poses {
        budget.charge("pose_output")?;
        let refined = *positions
            .get(&pose.node_id)
            .ok_or(PoseRefinementError::InternalLookup {
                field: "refined_pose_position",
            })?;
        let adjustment = subtract_vectors(
            refined,
            pose.camera_center_from_root_nano,
            "pose_adjustment",
        )?;
        poses.push(RefinedCameraPose {
            node_id: pose.node_id,
            sample_index: pose.sample_index,
            component_root_node_id: pose.component_root_node_id,
            scale_component_root_edge_id: pose.scale_component_root_edge_id,
            rotation_node_from_root_nano: pose.rotation_node_from_root_nano,
            initial_camera_center_from_root_nano: pose.camera_center_from_root_nano,
            refined_camera_center_from_root_nano: refined,
            adjustment_nano: adjustment,
            root_pinned: pose.node_id == pose.component_root_node_id,
        });
    }
    poses.sort_by_key(|pose| pose.node_id);

    let mut assessments = Vec::with_capacity(factors.len());
    for factor in &factors {
        budget.charge("factor_output")?;
        let initial_left = *initial_positions.get(&factor.left_node_id).ok_or(
            PoseRefinementError::InternalLookup {
                field: "assessment_initial_left",
            },
        )?;
        let initial_right = *initial_positions.get(&factor.right_node_id).ok_or(
            PoseRefinementError::InternalLookup {
                field: "assessment_initial_right",
            },
        )?;
        let refined_left =
            *positions
                .get(&factor.left_node_id)
                .ok_or(PoseRefinementError::InternalLookup {
                    field: "assessment_refined_left",
                })?;
        let refined_right = *positions.get(&factor.right_node_id).ok_or(
            PoseRefinementError::InternalLookup {
                field: "assessment_refined_right",
            },
        )?;
        let initial_implied = subtract_vectors(
            initial_right,
            initial_left,
            "assessment_initial_implied",
        )?;
        let refined_implied = subtract_vectors(
            refined_right,
            refined_left,
            "assessment_refined_implied",
        )?;
        let (initial_residual_nano, final_residual_nano) =
            if let Some(measured) = factor.measured_displacement_nano {
                (
                    Some(residual_linf(initial_implied, measured)?),
                    Some(residual_linf(refined_implied, measured)?),
                )
            } else {
                (None, None)
            };
        let disposition = if factor.blocked_upstream_conflict {
            PoseFactorDisposition::BlockedUpstreamConflict
        } else if factor.measured_displacement_nano.is_none() {
            PoseFactorDisposition::IncomparableScaleGauge
        } else if factor.robust_weight < factor.base_weight {
            PoseFactorDisposition::ActiveDownweighted
        } else {
            PoseFactorDisposition::ActiveFullWeight
        };
        assessments.push(PoseFactorAssessment {
            edge_id: factor.edge_id,
            component_root_node_id: factor.component_root_node_id,
            left_node_id: factor.left_node_id,
            right_node_id: factor.right_node_id,
            base_weight: factor.base_weight,
            robust_weight: factor.robust_weight,
            measured_displacement_nano: factor.measured_displacement_nano,
            initial_implied_displacement_nano: initial_implied,
            refined_implied_displacement_nano: refined_implied,
            initial_residual_nano,
            final_residual_nano,
            disposition,
        });
    }
    assessments.sort_by_key(|factor| factor.edge_id);

    Ok(PoseRefinementGeneration {
        basis,
        policy,
        initialization,
        poses,
        factors: assessments,
        components,
        iteration_count,
        operation_count: budget.used,
    })
}

fn validate_basis(basis: &PoseRefinementBasis) -> Result<(), PoseRefinementError> {
    reject_zero_digest("initialization_digest", &basis.initialization_digest)?;
    reject_zero_digest("policy_digest", &basis.policy_digest)?;
    validate_safe_nonzero("generation", basis.generation)
}

fn validate_policy(policy: PoseRefinementPolicy) -> Result<(), PoseRefinementError> {
    if policy.max_iterations == 0 {
        return Err(PoseRefinementError::ZeroValue {
            field: "max_iterations",
        });
    }
    if policy.max_iterations > MAX_REFINEMENT_ITERATIONS {
        return Err(PoseRefinementError::BoundExceeded {
            field: "max_iterations",
            actual: u128::from(policy.max_iterations),
            maximum: u128::from(MAX_REFINEMENT_ITERATIONS),
        });
    }
    if policy.convergence_delta_nano > MAX_REFINEMENT_POSITION_ABS_NANO {
        return Err(PoseRefinementError::BoundExceeded {
            field: "convergence_delta_nano",
            actual: u128::from(policy.convergence_delta_nano),
            maximum: u128::from(MAX_REFINEMENT_POSITION_ABS_NANO),
        });
    }
    if policy.huber_delta_nano == 0 {
        return Err(PoseRefinementError::ZeroValue {
            field: "huber_delta_nano",
        });
    }
    if policy.huber_delta_nano > MAX_REFINEMENT_HUBER_DELTA_NANO {
        return Err(PoseRefinementError::BoundExceeded {
            field: "huber_delta_nano",
            actual: u128::from(policy.huber_delta_nano),
            maximum: u128::from(MAX_REFINEMENT_HUBER_DELTA_NANO),
        });
    }
    validate_weight(
        "damping_weight",
        policy.damping_weight,
        MAX_REFINEMENT_DAMPING_WEIGHT,
    )?;
    validate_weight(
        "max_factor_weight",
        policy.max_factor_weight,
        MAX_REFINEMENT_FACTOR_WEIGHT,
    )?;
    if policy.max_camera_center_abs_nano == 0 {
        return Err(PoseRefinementError::ZeroValue {
            field: "max_camera_center_abs_nano",
        });
    }
    if policy.max_camera_center_abs_nano > MAX_REFINEMENT_POSITION_ABS_NANO {
        return Err(PoseRefinementError::BoundExceeded {
            field: "max_camera_center_abs_nano",
            actual: u128::from(policy.max_camera_center_abs_nano),
            maximum: u128::from(MAX_REFINEMENT_POSITION_ABS_NANO),
        });
    }
    if policy.max_operations == 0 {
        return Err(PoseRefinementError::ZeroValue {
            field: "max_operations",
        });
    }
    if policy.max_operations > MAX_REFINEMENT_OPERATIONS {
        return Err(PoseRefinementError::BoundExceeded {
            field: "max_operations",
            actual: u128::from(policy.max_operations),
            maximum: u128::from(MAX_REFINEMENT_OPERATIONS),
        });
    }
    Ok(())
}

fn validate_weight(
    field: &'static str,
    value: u32,
    maximum: u32,
) -> Result<(), PoseRefinementError> {
    if value == 0 {
        return Err(PoseRefinementError::ZeroValue { field });
    }
    if value > maximum {
        return Err(PoseRefinementError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(maximum),
        });
    }
    Ok(())
}

fn initial_robust_weight(
    base_weight: u32,
    huber_delta_nano: u64,
    residual_nano: u64,
) -> Result<u32, PoseRefinementError> {
    if residual_nano <= huber_delta_nano {
        return Ok(base_weight);
    }
    let numerator = u128::from(base_weight)
        .checked_mul(u128::from(huber_delta_nano))
        .ok_or(PoseRefinementError::ArithmeticOverflow {
            field: "robust_weight_numerator",
        })?;
    let reduced = numerator / u128::from(residual_nano);
    let clamped = reduced.clamp(1, u128::from(base_weight));
    u32::try_from(clamped).map_err(|_| PoseRefinementError::ArithmeticOverflow {
        field: "robust_weight",
    })
}

fn component_objective(
    positions: &BTreeMap<u64, [i64; 3]>,
    factors: &[RefinementFactor],
    factor_indices: &[usize],
    budget: &mut OperationBudget,
) -> Result<Objective, PoseRefinementError> {
    let mut weighted_squared_error = 0_u128;
    let mut coordinate_weight = 0_u128;
    for index in factor_indices {
        budget.charge("objective_factor")?;
        let factor = factors
            .get(*index)
            .ok_or(PoseRefinementError::InternalLookup {
                field: "objective_factor",
            })?;
        if factor.robust_weight == 0 {
            continue;
        }
        let measured = factor.measured_displacement_nano.ok_or(
            PoseRefinementError::InternalLookup {
                field: "active_measured_displacement",
            },
        )?;
        let left =
            *positions
                .get(&factor.left_node_id)
                .ok_or(PoseRefinementError::InternalLookup {
                    field: "objective_left_position",
                })?;
        let right = *positions.get(&factor.right_node_id).ok_or(
            PoseRefinementError::InternalLookup {
                field: "objective_right_position",
            },
        )?;
        let implied = subtract_vectors(right, left, "objective_implied_displacement")?;
        let residual = subtract_vectors(implied, measured, "objective_residual")?;
        let weight = u128::from(factor.robust_weight);
        for coordinate in residual {
            let magnitude = u128::from(coordinate.unsigned_abs());
            let square = magnitude.checked_mul(magnitude).ok_or(
                PoseRefinementError::ArithmeticOverflow {
                    field: "objective_residual_square",
                },
            )?;
            weighted_squared_error = weighted_squared_error
                .checked_add(square.checked_mul(weight).ok_or(
                    PoseRefinementError::ArithmeticOverflow {
                        field: "objective_weighted_square",
                    },
                )?)
                .ok_or(PoseRefinementError::ArithmeticOverflow {
                    field: "objective_sum",
                })?;
        }
        coordinate_weight = coordinate_weight
            .checked_add(weight.checked_mul(3).ok_or(
                PoseRefinementError::ArithmeticOverflow {
                    field: "objective_coordinate_weight",
                },
            )?)
            .ok_or(PoseRefinementError::ArithmeticOverflow {
                field: "objective_coordinate_weight",
            })?;
    }
    Ok(Objective {
        weighted_squared_error,
        coordinate_weight,
    })
}

fn weighted_node_target(
    node_id: u64,
    current: [i64; 3],
    incident: &[usize],
    positions: &BTreeMap<u64, [i64; 3]>,
    factors: &[RefinementFactor],
    damping_weight: u32,
    budget: &mut OperationBudget,
) -> Result<[i64; 3], PoseRefinementError> {
    let mut total_weight = u128::from(damping_weight);
    let mut sums = [0_i128; 3];
    for coordinate in 0..3 {
        sums[coordinate] = i128::from(current[coordinate])
            .checked_mul(i128::from(damping_weight))
            .ok_or(PoseRefinementError::ArithmeticOverflow {
                field: "damping_position_product",
            })?;
    }
    for index in incident {
        budget.charge("node_factor_proposal")?;
        let factor = factors
            .get(*index)
            .ok_or(PoseRefinementError::InternalLookup {
                field: "node_factor",
            })?;
        if factor.robust_weight == 0 {
            continue;
        }
        let measured = factor.measured_displacement_nano.ok_or(
            PoseRefinementError::InternalLookup {
                field: "node_factor_measured_displacement",
            },
        )?;
        let proposal = if factor.left_node_id == node_id {
            let right = *positions.get(&factor.right_node_id).ok_or(
                PoseRefinementError::InternalLookup {
                    field: "proposal_right_position",
                },
            )?;
            subtract_vectors(right, measured, "left_node_proposal")?
        } else if factor.right_node_id == node_id {
            let left =
                *positions
                    .get(&factor.left_node_id)
                    .ok_or(PoseRefinementError::InternalLookup {
                        field: "proposal_left_position",
                    })?;
            add_vectors(left, measured, "right_node_proposal")?
        } else {
            return Err(PoseRefinementError::InternalLookup {
                field: "incident_factor_endpoint",
            });
        };
        let weight = u128::from(factor.robust_weight);
        total_weight =
            total_weight
                .checked_add(weight)
                .ok_or(PoseRefinementError::ArithmeticOverflow {
                    field: "node_total_weight",
                })?;
        let signed_weight =
            i128::try_from(weight).map_err(|_| PoseRefinementError::ArithmeticOverflow {
                field: "node_signed_weight",
            })?;
        for coordinate in 0..3 {
            let term = i128::from(proposal[coordinate])
                .checked_mul(signed_weight)
                .ok_or(PoseRefinementError::ArithmeticOverflow {
                    field: "proposal_weighted_position",
                })?;
            sums[coordinate] =
                sums[coordinate]
                    .checked_add(term)
                    .ok_or(PoseRefinementError::ArithmeticOverflow {
                        field: "proposal_position_sum",
                    })?;
        }
    }
    let denominator =
        i128::try_from(total_weight).map_err(|_| PoseRefinementError::ArithmeticOverflow {
            field: "node_total_weight",
        })?;
    let mut target = [0_i64; 3];
    for coordinate in 0..3 {
        target[coordinate] = i128_to_i64(
            divide_round_i128(sums[coordinate], denominator, "weighted_node_target")?,
            "weighted_node_target",
        )?;
    }
    Ok(target)
}

fn snapshot_positions(
    node_ids: &[u64],
    positions: &BTreeMap<u64, [i64; 3]>,
) -> Result<Vec<(u64, [i64; 3])>, PoseRefinementError> {
    node_ids
        .iter()
        .map(|node_id| {
            positions
                .get(node_id)
                .copied()
                .map(|position| (*node_id, position))
                .ok_or(PoseRefinementError::InternalLookup {
                    field: "snapshot_position",
                })
        })
        .collect()
}

fn restore_positions(
    snapshot: &[(u64, [i64; 3])],
    positions: &mut BTreeMap<u64, [i64; 3]>,
) {
    for (node_id, position) in snapshot {
        positions.insert(*node_id, *position);
    }
}

fn component_max_adjustment(
    node_ids: &[u64],
    initial_positions: &BTreeMap<u64, [i64; 3]>,
    final_positions: &BTreeMap<u64, [i64; 3]>,
) -> Result<u64, PoseRefinementError> {
    let mut maximum = 0_u64;
    for node_id in node_ids {
        let initial = *initial_positions.get(node_id).ok_or(
            PoseRefinementError::InternalLookup {
                field: "component_initial_position",
            },
        )?;
        let final_position = *final_positions.get(node_id).ok_or(
            PoseRefinementError::InternalLookup {
                field: "component_final_position",
            },
        )?;
        maximum = maximum.max(vector_linf_difference(initial, final_position)?);
    }
    Ok(maximum)
}

fn measured_edge_displacement_root(
    edge: &RelativePoseEdge,
    scale: &ReconciledEdgeScale,
    right_rotation_node_from_root_nano: [i64; 9],
) -> Result<[i64; 3], PoseRefinementError> {
    let scaled_translation = scale_direction(
        edge.translation_right_from_left_direction_nano,
        scale.relative_scale_nano,
    )?;
    let in_root =
        transpose_matrix_vector(right_rotation_node_from_root_nano, scaled_translation)?;
    negate_vector(in_root, "measured_edge_displacement")
}

fn scale_direction(
    direction: [i64; 3],
    scale_nano: u64,
) -> Result<[i64; 3], PoseRefinementError> {
    let scale = i128::from(scale_nano);
    let denominator = i128::from(UNIT_NANO);
    let mut output = [0_i64; 3];
    for coordinate in 0..3 {
        let product = i128::from(direction[coordinate])
            .checked_mul(scale)
            .ok_or(PoseRefinementError::ArithmeticOverflow {
                field: "scaled_translation",
            })?;
        output[coordinate] = i128_to_i64(
            divide_round_i128(product, denominator, "scaled_translation")?,
            "scaled_translation",
        )?;
    }
    Ok(output)
}

fn transpose_matrix_vector(
    matrix: [i64; 9],
    vector: [i64; 3],
) -> Result<[i64; 3], PoseRefinementError> {
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
                        .ok_or(PoseRefinementError::ArithmeticOverflow {
                            field: "transpose_matrix_vector_product",
                        })?,
                )
                .ok_or(PoseRefinementError::ArithmeticOverflow {
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

fn add_vectors(
    left: [i64; 3],
    right: [i64; 3],
    field: &'static str,
) -> Result<[i64; 3], PoseRefinementError> {
    Ok([
        left[0]
            .checked_add(right[0])
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
        left[1]
            .checked_add(right[1])
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
        left[2]
            .checked_add(right[2])
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
    ])
}

fn subtract_vectors(
    left: [i64; 3],
    right: [i64; 3],
    field: &'static str,
) -> Result<[i64; 3], PoseRefinementError> {
    Ok([
        left[0]
            .checked_sub(right[0])
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
        left[1]
            .checked_sub(right[1])
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
        left[2]
            .checked_sub(right[2])
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
    ])
}

fn negate_vector(
    value: [i64; 3],
    field: &'static str,
) -> Result<[i64; 3], PoseRefinementError> {
    Ok([
        value[0]
            .checked_neg()
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
        value[1]
            .checked_neg()
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
        value[2]
            .checked_neg()
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?,
    ])
}

fn residual_linf(
    implied: [i64; 3],
    measured: [i64; 3],
) -> Result<u64, PoseRefinementError> {
    let residual = subtract_vectors(implied, measured, "factor_residual")?;
    Ok(residual
        .iter()
        .map(|coordinate| coordinate.unsigned_abs())
        .max()
        .unwrap_or(0))
}

fn vector_linf_difference(
    left: [i64; 3],
    right: [i64; 3],
) -> Result<u64, PoseRefinementError> {
    residual_linf(left, right)
}

fn validate_position(
    node_id: u64,
    position: [i64; 3],
    maximum: u64,
) -> Result<(), PoseRefinementError> {
    for coordinate in position {
        let actual = coordinate.unsigned_abs();
        if actual > maximum {
            return Err(PoseRefinementError::PositionBoundExceeded {
                node_id,
                actual: u128::from(actual),
                maximum: u128::from(maximum),
            });
        }
    }
    Ok(())
}

fn divide_round_i128(
    value: i128,
    denominator: i128,
    field: &'static str,
) -> Result<i128, PoseRefinementError> {
    if denominator <= 0 {
        return Err(PoseRefinementError::ZeroValue { field });
    }
    let half = denominator / 2;
    if value >= 0 {
        value
            .checked_add(half)
            .map(|rounded| rounded / denominator)
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })
    } else {
        let magnitude = value
            .checked_neg()
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })?;
        magnitude
            .checked_add(half)
            .map(|rounded| -(rounded / denominator))
            .ok_or(PoseRefinementError::ArithmeticOverflow { field })
    }
}

fn i128_to_i64(value: i128, field: &'static str) -> Result<i64, PoseRefinementError> {
    i64::try_from(value).map_err(|_| PoseRefinementError::ArithmeticOverflow { field })
}

fn integer_sqrt(value: u128) -> Result<u64, PoseRefinementError> {
    if value < 2 {
        return u64::try_from(value).map_err(|_| PoseRefinementError::ArithmeticOverflow {
            field: "integer_sqrt",
        });
    }
    let bit_length = 128_u32.saturating_sub(value.leading_zeros());
    let shift = bit_length.div_ceil(2);
    let mut current = 1_u128
        .checked_shl(shift)
        .ok_or(PoseRefinementError::ArithmeticOverflow {
            field: "integer_sqrt_seed",
        })?;
    loop {
        let next = current
            .checked_add(value / current)
            .ok_or(PoseRefinementError::ArithmeticOverflow {
                field: "integer_sqrt_iteration",
            })?
            / 2;
        if next >= current {
            return u64::try_from(current).map_err(|_| {
                PoseRefinementError::ArithmeticOverflow {
                    field: "integer_sqrt",
                }
            });
        }
        current = next;
    }
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), PoseRefinementError> {
    if digest.to_bytes() == [0; 32] {
        Err(PoseRefinementError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn validate_safe_nonzero(
    field: &'static str,
    value: u64,
) -> Result<(), PoseRefinementError> {
    if value == 0 {
        return Err(PoseRefinementError::ZeroValue { field });
    }
    if value > MAX_SAFE_JSON_INTEGER {
        return Err(PoseRefinementError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        });
    }
    Ok(())
}
