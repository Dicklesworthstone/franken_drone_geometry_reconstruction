/// Returns the exact identity of the scalar fixed-point reprojection algorithm.
///
/// # Errors
///
/// Returns an identity-domain or canonical hashing error.
pub fn reprojection_algorithm_digest() -> Result<EvidenceDigest, ReprojectionError> {
    let domain = DigestDomain::parse(REPROJECTION_ALGORITHM_SCHEMA)?;
    Ok(hash_domain(
        &domain,
        b"root_to_camera_i128_round_nearest;calibrated_projection_v1;euclidean_residual_integer_sqrt;uncertainty=max(observation,calibration,1);component_decision_v1",
    )?)
}

/// Returns the semantic identity of one reprojection policy.
///
/// The operation ceiling is deliberately excluded.
///
/// # Errors
///
/// Returns a policy-validation, identity-domain, or canonical hashing error.
pub fn reprojection_policy_digest(
    policy: ReprojectionPolicy,
) -> Result<EvidenceDigest, ReprojectionError> {
    validate_policy(policy)?;
    let mut encoder = Encoder::with_capacity(128);
    encoder.put_str(REPROJECTION_POLICY_SCHEMA)?;
    encode_semantic_policy(&mut encoder, policy);
    let domain = DigestDomain::parse(REPROJECTION_POLICY_SCHEMA)?;
    Ok(hash_domain(&domain, &encoder.into_bytes())?)
}

/// Returns the identity of a canonical effective-calibration table.
///
/// Input order is not semantic; camera identity defines canonical order.
///
/// # Errors
///
/// Returns a calibration, duplicate, bound, identity-domain, or codec error.
pub fn reprojection_calibration_table_digest(
    calibrations: &[ReprojectionCalibration],
) -> Result<EvidenceDigest, ReprojectionError> {
    let canonical = canonicalize_calibrations(calibrations.to_vec())?;
    let mut encoder = Encoder::with_capacity(
        64_usize.saturating_add(canonical.len().saturating_mul(112)),
    );
    encoder.put_str(REPROJECTION_CALIBRATION_TABLE_SCHEMA)?;
    encoder.put_u64(usize_to_u64(canonical.len())?);
    for calibration in &canonical {
        encode_calibration_identity(&mut encoder, calibration);
    }
    let domain = DigestDomain::parse(REPROJECTION_CALIBRATION_TABLE_SCHEMA)?;
    Ok(hash_domain(&domain, &encoder.into_bytes())?)
}

/// Evaluates the currently selected relative camera and landmark state through exact calibrations.
///
/// Projection failures and residual conflicts are retained as evidence rather than erased. The
/// function does not optimize state or upgrade arbitrary component gauges to metric authority.
///
/// # Errors
///
/// Returns a stable upstream, basis, calibration, bound, budget, arithmetic, or identity error.
pub fn evaluate_reprojection(
    basis: ReprojectionBasis,
    policy: ReprojectionPolicy,
    bundle_admission: BundleAdmissionGeneration,
    calibrations: Vec<ReprojectionCalibration>,
) -> Result<ReprojectionEvaluation, ReprojectionError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    bundle_admission.validate()?;
    let observed_admission_digest = bundle_admission.digest()?;
    compare_basis_digest(
        "bundle_admission_digest",
        &basis.bundle_admission_digest,
        &observed_admission_digest,
    )?;
    let expected_algorithm_digest = reprojection_algorithm_digest()?;
    compare_basis_digest(
        "algorithm_digest",
        &basis.algorithm_digest,
        &expected_algorithm_digest,
    )?;
    let expected_policy_digest = reprojection_policy_digest(policy)?;
    compare_basis_digest(
        "policy_digest",
        &basis.policy_digest,
        &expected_policy_digest,
    )?;
    let calibrations = canonicalize_calibrations(calibrations)?;
    let observed_calibration_table_digest =
        reprojection_calibration_table_digest(&calibrations)?;
    compare_basis_digest(
        "calibration_table_digest",
        &basis.calibration_table_digest,
        &observed_calibration_table_digest,
    )?;

    let mut budget = OperationBudget::new(policy.max_operations);
    validate_calibrations_against_admission(
        &bundle_admission,
        &calibrations,
        &mut budget,
    )?;

    let calibration_by_camera = calibrations
        .iter()
        .map(|calibration| (calibration.camera_node_id, calibration))
        .collect::<BTreeMap<_, _>>();
    let camera_by_node = bundle_admission
        .bundle_problem
        .cameras
        .iter()
        .map(|camera| (camera.binding.camera_node_id, camera))
        .collect::<BTreeMap<_, _>>();
    let landmark_by_id = bundle_admission
        .bundle_problem
        .landmarks
        .iter()
        .map(|landmark| (landmark.seed.landmark_id, landmark))
        .collect::<BTreeMap<_, _>>();
    let audit_by_observation = bundle_admission
        .observations
        .iter()
        .map(|audit| (audit.observation_id, audit))
        .collect::<BTreeMap<_, _>>();

    let mut observations = Vec::with_capacity(bundle_admission.bundle_problem.observations.len());
    for compiled in &bundle_admission.bundle_problem.observations {
        budget.charge("observation_evaluation")?;
        let observation = &compiled.observation;
        let camera = camera_by_node
            .get(&observation.camera_node_id)
            .copied()
            .ok_or(ReprojectionError::InternalLookup {
                field: "observation_camera",
            })?;
        let audit = audit_by_observation
            .get(&observation.observation_id)
            .copied()
            .ok_or(ReprojectionError::InternalLookup {
                field: "observation_audit",
            })?;
        let eligible = matches!(
            audit.disposition,
            ObservationAuditDisposition::ActiveOptimizeInDomain
                | ObservationAuditDisposition::EligibleHeldOutIndependent
        );
        if !eligible {
            observations.push(ReprojectionObservationEvidence {
                observation_id: observation.observation_id,
                landmark_id: observation.landmark_id,
                camera_node_id: observation.camera_node_id,
                component_root_node_id: camera.component_root_node_id,
                role: observation.role,
                projected_x_nano_pixels: None,
                projected_y_nano_pixels: None,
                depth_nano: None,
                residual_x_nano_pixels: None,
                residual_y_nano_pixels: None,
                residual_nano_pixels: None,
                normalized_residual_ppm: None,
                observation_uncertainty_nano_pixels: observation
                    .localization_uncertainty_nano_pixels,
                calibration_uncertainty_nano_pixels: 0,
                within_image_domain: None,
                disposition: ReprojectionObservationDisposition::UpstreamExcluded,
            });
            continue;
        }
        if !matches!(
            compiled.disposition,
            BundleObservationDisposition::ActiveOptimize
                | BundleObservationDisposition::EligibleHeldOut
        ) {
            return Err(ReprojectionError::InternalLookup {
                field: "eligible_observation_disposition",
            });
        }
        let landmark = landmark_by_id
            .get(&observation.landmark_id)
            .copied()
            .ok_or(ReprojectionError::InternalLookup {
                field: "observation_landmark",
            })?;
        let calibration = calibration_by_camera
            .get(&observation.camera_node_id)
            .copied()
            .ok_or(ReprojectionError::MissingCalibration {
                camera_node_id: observation.camera_node_id,
            })?;
        observations.push(evaluate_observation(
            observation,
            camera,
            landmark,
            calibration,
            policy,
        )?);
    }

    let mut components = Vec::with_capacity(bundle_admission.components.len());
    for admission_component in &bundle_admission.components {
        budget.charge("component_summary")?;
        let component_evidence = observations
            .iter()
            .filter(|entry| {
                entry.component_root_node_id == admission_component.component_root_node_id
            })
            .collect::<Vec<_>>();
        let optimize = component_evidence
            .iter()
            .copied()
            .filter(|entry| entry.role == BundleObservationRole::Optimize)
            .collect::<Vec<_>>();
        let held_out = component_evidence
            .iter()
            .copied()
            .filter(|entry| entry.role == BundleObservationRole::HeldOut)
            .collect::<Vec<_>>();
        let optimize_eligible = count_eligible(&optimize)?;
        let held_out_eligible = count_eligible(&held_out)?;
        let optimize_summary = summarize_residuals(optimize_eligible, &optimize)?;
        let held_out_summary = summarize_residuals(held_out_eligible, &held_out)?;
        let mut unprojectable_observation_ids = component_evidence
            .iter()
            .filter(|entry| entry.disposition.is_projection_failure())
            .map(|entry| entry.observation_id)
            .collect::<Vec<_>>();
        let mut conflicting_observation_ids = component_evidence
            .iter()
            .filter(|entry| {
                entry.disposition.is_projected()
                    && entry.disposition != ReprojectionObservationDisposition::ProjectedInlier
            })
            .map(|entry| entry.observation_id)
            .collect::<Vec<_>>();
        unprojectable_observation_ids.sort_unstable();
        conflicting_observation_ids.sort_unstable();
        let (status, decision, recommendation) = component_decision(
            admission_component.decision,
            &optimize_summary,
            &held_out_summary,
            &unprojectable_observation_ids,
            &conflicting_observation_ids,
            policy,
        );
        components.push(ReprojectionComponent {
            component_root_node_id: admission_component.component_root_node_id,
            scale_component_root_edge_id: admission_component.scale_component_root_edge_id,
            status,
            decision,
            recommendation,
            optimize: optimize_summary,
            held_out: held_out_summary,
            unprojectable_observation_ids,
            conflicting_observation_ids,
        });
    }

    Ok(ReprojectionEvaluation {
        basis,
        policy,
        bundle_admission,
        calibrations,
        observations,
        components,
        operation_count: budget.operations,
    })
}

fn evaluate_observation(
    observation: &fdgr_bundle_problem::BundleObservation,
    camera: &fdgr_bundle_problem::CompiledBundleCamera,
    landmark: &fdgr_bundle_problem::CompiledBundleLandmark,
    calibration: &ReprojectionCalibration,
    policy: ReprojectionPolicy,
) -> Result<ReprojectionObservationEvidence, ReprojectionError> {
    let point_camera_nano = root_point_to_camera(
        camera.rotation_node_from_root_nano,
        camera.selected_camera_center_from_root_nano,
        landmark.seed.seed_position_from_root_nano,
    )?;
    let calibration_uncertainty = calibration.calibration.declared_uncertainty_nano_pixels;
    let projection = project_calibrated_camera_point(
        &calibration.calibration,
        point_camera_nano,
        policy.projection,
    );
    let projected = match projection {
        Ok(projected) => projected,
        Err(ProjectionError::Calibration(error)) => return Err(error.into()),
        Err(error) => {
            return Ok(ReprojectionObservationEvidence {
                observation_id: observation.observation_id,
                landmark_id: observation.landmark_id,
                camera_node_id: observation.camera_node_id,
                component_root_node_id: camera.component_root_node_id,
                role: observation.role,
                projected_x_nano_pixels: None,
                projected_y_nano_pixels: None,
                depth_nano: None,
                residual_x_nano_pixels: None,
                residual_y_nano_pixels: None,
                residual_nano_pixels: None,
                normalized_residual_ppm: None,
                observation_uncertainty_nano_pixels: observation
                    .localization_uncertainty_nano_pixels,
                calibration_uncertainty_nano_pixels: calibration_uncertainty,
                within_image_domain: None,
                disposition: projection_error_disposition(&error),
            });
        }
    };
    let residual_x = projected
        .x_nano_pixels
        .checked_sub(observation.x_nano_pixels)
        .ok_or(ReprojectionError::ArithmeticOverflow {
            field: "residual_x",
        })?;
    let residual_y = projected
        .y_nano_pixels
        .checked_sub(observation.y_nano_pixels)
        .ok_or(ReprojectionError::ArithmeticOverflow {
            field: "residual_y",
        })?;
    let residual = residual_magnitude(residual_x, residual_y)?;
    let normalized = normalized_residual_ppm(
        residual,
        observation.localization_uncertainty_nano_pixels,
        calibration_uncertainty,
    )?;
    let disposition = if !projected.within_image_domain {
        ReprojectionObservationDisposition::ProjectedOutsideImageDomain
    } else if residual > policy.max_residual_nano_pixels {
        ReprojectionObservationDisposition::ResidualExceeded
    } else if normalized > policy.max_normalized_residual_ppm {
        ReprojectionObservationDisposition::NormalizedResidualExceeded
    } else {
        ReprojectionObservationDisposition::ProjectedInlier
    };
    Ok(ReprojectionObservationEvidence {
        observation_id: observation.observation_id,
        landmark_id: observation.landmark_id,
        camera_node_id: observation.camera_node_id,
        component_root_node_id: camera.component_root_node_id,
        role: observation.role,
        projected_x_nano_pixels: Some(projected.x_nano_pixels),
        projected_y_nano_pixels: Some(projected.y_nano_pixels),
        depth_nano: Some(projected.depth_nano),
        residual_x_nano_pixels: Some(residual_x),
        residual_y_nano_pixels: Some(residual_y),
        residual_nano_pixels: Some(residual),
        normalized_residual_ppm: Some(normalized),
        observation_uncertainty_nano_pixels: observation.localization_uncertainty_nano_pixels,
        calibration_uncertainty_nano_pixels: calibration_uncertainty,
        within_image_domain: Some(projected.within_image_domain),
        disposition,
    })
}

fn canonicalize_calibrations(
    mut calibrations: Vec<ReprojectionCalibration>,
) -> Result<Vec<ReprojectionCalibration>, ReprojectionError> {
    if calibrations.len() > MAX_REPROJECTION_CALIBRATIONS {
        return Err(ReprojectionError::BoundExceeded {
            field: "calibration_count",
            actual: calibrations.len() as u128,
            maximum: MAX_REPROJECTION_CALIBRATIONS as u128,
        });
    }
    calibrations.sort_by_key(|calibration| calibration.camera_node_id);
    let mut camera_ids = BTreeSet::new();
    for calibration in &calibrations {
        validate_safe_nonzero("camera_node_id", calibration.camera_node_id)?;
        reject_zero_digest("frame_digest", &calibration.frame_digest)?;
        reject_zero_digest(
            "effective_calibration_digest",
            &calibration.effective_calibration_digest,
        )?;
        calibration.calibration.validate()?;
        if calibration.calibration.readout.observed_readout_time_ns > MAX_READOUT_TIME_NS {
            return Err(ReprojectionError::BoundExceeded {
                field: "observed_readout_time_ns",
                actual: u128::from(
                    calibration.calibration.readout.observed_readout_time_ns,
                ),
                maximum: u128::from(MAX_READOUT_TIME_NS),
            });
        }
        if calibration.calibration.digest()? != calibration.effective_calibration_digest {
            return Err(ReprojectionError::CalibrationDigestMismatch {
                camera_node_id: calibration.camera_node_id,
            });
        }
        if !camera_ids.insert(calibration.camera_node_id) {
            return Err(ReprojectionError::DuplicateCalibration {
                camera_node_id: calibration.camera_node_id,
            });
        }
    }
    Ok(calibrations)
}

fn validate_calibrations_against_admission(
    admission: &BundleAdmissionGeneration,
    calibrations: &[ReprojectionCalibration],
    budget: &mut OperationBudget,
) -> Result<(), ReprojectionError> {
    let camera_by_node = admission
        .bundle_problem
        .cameras
        .iter()
        .map(|camera| (camera.binding.camera_node_id, camera))
        .collect::<BTreeMap<_, _>>();
    let domain_by_node = admission
        .camera_domains
        .iter()
        .map(|domain| (domain.camera_node_id, domain))
        .collect::<BTreeMap<_, _>>();
    let calibration_by_node = calibrations
        .iter()
        .map(|calibration| (calibration.camera_node_id, calibration))
        .collect::<BTreeMap<_, _>>();
    for (camera_node_id, camera) in &camera_by_node {
        budget.charge("required_calibration_validation")?;
        let domain = domain_by_node
            .get(camera_node_id)
            .copied()
            .ok_or(ReprojectionError::InternalLookup {
                field: "camera_domain",
            })?;
        let calibration = calibration_by_node
            .get(camera_node_id)
            .copied()
            .ok_or(ReprojectionError::MissingCalibration {
                camera_node_id: *camera_node_id,
            })?;
        if calibration.frame_digest != camera.binding.frame_digest
            || calibration.frame_digest != domain.frame_digest
        {
            return Err(ReprojectionError::CalibrationFrameMismatch {
                camera_node_id: *camera_node_id,
            });
        }
        if calibration.effective_calibration_digest
            != camera.binding.effective_calibration_digest
            || calibration.effective_calibration_digest != domain.effective_calibration_digest
        {
            return Err(ReprojectionError::CalibrationDigestMismatch {
                camera_node_id: *camera_node_id,
            });
        }
        let observed_width = calibration.calibration.derivation.output_width;
        let observed_height = calibration.calibration.derivation.output_height;
        if observed_width != domain.image_width || observed_height != domain.image_height {
            return Err(ReprojectionError::CalibrationDimensionMismatch {
                camera_node_id: *camera_node_id,
                expected_width: domain.image_width,
                expected_height: domain.image_height,
                observed_width,
                observed_height,
            });
        }
    }
    for calibration in calibrations {
        budget.charge("extra_calibration_validation")?;
        if !camera_by_node.contains_key(&calibration.camera_node_id) {
            return Err(ReprojectionError::ExtraCalibration {
                camera_node_id: calibration.camera_node_id,
            });
        }
    }
    Ok(())
}

fn count_eligible(
    evidence: &[&ReprojectionObservationEvidence],
) -> Result<u64, ReprojectionError> {
    u64::try_from(
        evidence
            .iter()
            .filter(|entry| {
                entry.disposition != ReprojectionObservationDisposition::UpstreamExcluded
            })
            .count(),
    )
    .map_err(|_| ReprojectionError::ArithmeticOverflow {
        field: "eligible_observation_count",
    })
}

fn component_decision(
    admission_decision: BundleAdmissionDecision,
    optimize: &ReprojectionResidualSummary,
    held_out: &ReprojectionResidualSummary,
    unprojectable: &[u64],
    conflicting: &[u64],
    policy: ReprojectionPolicy,
) -> (
    ReprojectionComponentStatus,
    ReprojectionDecision,
    ReprojectionRecommendation,
) {
    if admission_decision != BundleAdmissionDecision::Admit {
        return (
            ReprojectionComponentStatus::BlockedAdmission,
            ReprojectionDecision::Block,
            ReprojectionRecommendation::ResolveBundleAdmission,
        );
    }
    if !unprojectable.is_empty() {
        return (
            ReprojectionComponentStatus::ProjectionConflict,
            ReprojectionDecision::Block,
            ReprojectionRecommendation::SupplyExactProjectionModel,
        );
    }
    if optimize.projected_observation_count == 0 {
        return (
            ReprojectionComponentStatus::NoOptimizeEvidence,
            ReprojectionDecision::Block,
            ReprojectionRecommendation::AddOptimizeEvidence,
        );
    }
    if optimize.projected_observation_count
        < u64::from(policy.min_optimize_observations_per_component)
    {
        return (
            ReprojectionComponentStatus::InsufficientOptimizeEvidence,
            ReprojectionDecision::AdmitDiagnostic,
            ReprojectionRecommendation::AddOptimizeEvidence,
        );
    }
    if held_out.projected_observation_count
        < u64::from(policy.min_held_out_observations_per_component)
    {
        return (
            ReprojectionComponentStatus::InsufficientHeldOutEvidence,
            ReprojectionDecision::AdmitDiagnostic,
            ReprojectionRecommendation::ReserveIndependentHeldOutViews,
        );
    }
    if !conflicting.is_empty() {
        return (
            ReprojectionComponentStatus::ResidualConflict,
            ReprojectionDecision::AdmitDiagnostic,
            ReprojectionRecommendation::RevisitStateOrCalibration,
        );
    }
    (
        ReprojectionComponentStatus::Consistent,
        ReprojectionDecision::Admit,
        ReprojectionRecommendation::ProceedToBoundedOptimization,
    )
}

fn projection_error_disposition(error: &ProjectionError) -> ReprojectionObservationDisposition {
    match error {
        ProjectionError::UnsupportedDistortion { .. } => {
            ReprojectionObservationDisposition::ProjectionModelUnsupported
        }
        ProjectionError::RollingShutterRequiresMotionModel => {
            ReprojectionObservationDisposition::RollingShutterUnsupported
        }
        ProjectionError::InsufficientPositiveDepth { .. } => {
            ReprojectionObservationDisposition::InsufficientPositiveDepth
        }
        ProjectionError::NormalizedCoordinateExceeded { .. } => {
            ReprojectionObservationDisposition::NormalizedCoordinateExceeded
        }
        ProjectionError::ZeroValue { .. } | ProjectionError::BoundExceeded { .. } => {
            ReprojectionObservationDisposition::ProjectionBoundExceeded
        }
        ProjectionError::ArithmeticOverflow { .. } => {
            ReprojectionObservationDisposition::ProjectionArithmeticFailure
        }
        ProjectionError::Calibration(_) => {
            ReprojectionObservationDisposition::ProjectionArithmeticFailure
        }
    }
}

fn compare_basis_digest(
    field: &'static str,
    expected: &EvidenceDigest,
    observed: &EvidenceDigest,
) -> Result<(), ReprojectionError> {
    if expected == observed {
        Ok(())
    } else {
        Err(ReprojectionError::BasisMismatch {
            field,
            expected: expected.clone(),
            observed: observed.clone(),
        })
    }
}

fn encode_semantic_policy(encoder: &mut Encoder, policy: ReprojectionPolicy) {
    encoder.put_u64(policy.projection.minimum_positive_depth_nano);
    encoder.put_u64(policy.projection.max_normalized_coordinate_abs_nano);
    encoder.put_u64(
        policy
            .projection
            .max_projected_coordinate_abs_nano_pixels,
    );
    encoder.put_u64(policy.max_residual_nano_pixels);
    encoder.put_u64(policy.max_normalized_residual_ppm);
    encoder.put_u32(policy.min_optimize_observations_per_component);
    encoder.put_u32(policy.min_held_out_observations_per_component);
}

fn encode_calibration_identity(
    encoder: &mut Encoder,
    calibration: &ReprojectionCalibration,
) {
    encoder.put_u64(calibration.camera_node_id);
    encoder.put_digest(&calibration.frame_digest);
    encoder.put_digest(&calibration.effective_calibration_digest);
}
