#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct OperationBudget {
    used: u64,
    maximum: u64,
}

impl OperationBudget {
    const fn new(maximum: u64) -> Self {
        Self { used: 0, maximum }
    }

    fn charge(&mut self) -> Result<(), BundleAdmissionError> {
        let attempted = self
            .used
            .checked_add(1)
            .ok_or(BundleAdmissionError::ArithmeticOverflow {
                field: "operation_count",
            })?;
        if attempted > self.maximum {
            return Err(BundleAdmissionError::OperationBudgetExceeded {
                attempted,
                maximum: self.maximum,
            });
        }
        self.used = attempted;
        Ok(())
    }
}

/// Computes the semantic audit-policy identity.
///
/// The successful operation ceiling is deliberately excluded.
///
/// # Errors
///
/// Returns a stable policy, domain, or codec error.
pub fn bundle_admission_policy_digest(
    policy: BundleAdmissionPolicy,
) -> Result<EvidenceDigest, BundleAdmissionError> {
    validate_policy(policy)?;
    let mut encoder = Encoder::with_capacity(128);
    encoder.put_str(BUNDLE_ADMISSION_POLICY_SCHEMA)?;
    encode_semantic_policy(&mut encoder, policy);
    hash_encoder(BUNDLE_ADMISSION_POLICY_SCHEMA, encoder)
}

/// Computes the exact canonical camera-domain table identity.
///
/// # Errors
///
/// Returns a stable identity, duplicate, bound, domain, or codec error.
pub fn camera_domain_table_digest(
    domains: &[BundleCameraDomain],
) -> Result<EvidenceDigest, BundleAdmissionError> {
    let domains = canonicalize_camera_domains(domains.to_vec())?;
    let mut encoder = Encoder::with_capacity(64_usize.saturating_add(domains.len() * 96));
    encoder.put_str(CAMERA_DOMAIN_TABLE_SCHEMA)?;
    encoder.put_u64(usize_to_u64(domains.len())?);
    for domain in &domains {
        encode_camera_domain(&mut encoder, domain);
    }
    hash_encoder(CAMERA_DOMAIN_TABLE_SCHEMA, encoder)
}

/// Computes the exact canonical landmark seed-provenance table identity.
///
/// # Errors
///
/// Returns a stable identity, duplicate, bound, domain, or codec error.
pub fn seed_provenance_table_digest(
    provenance: &[LandmarkSeedProvenance],
) -> Result<EvidenceDigest, BundleAdmissionError> {
    let provenance = canonicalize_seed_provenance(provenance.to_vec())?;
    let mut encoder = Encoder::with_capacity(64_usize.saturating_add(provenance.len() * 96));
    encoder.put_str(SEED_PROVENANCE_TABLE_SCHEMA)?;
    encoder.put_u64(usize_to_u64(provenance.len())?);
    for record in &provenance {
        encode_seed_provenance(&mut encoder, record)?;
    }
    hash_encoder(SEED_PROVENANCE_TABLE_SCHEMA, encoder)
}

/// Audits one exact structural bundle problem for image-domain, seed-partition, and held-out
/// independence correctness.
///
/// # Errors
///
/// Returns a stable basis, structural, binding, provenance, budget, arithmetic, codec, or replay
/// error.
pub fn audit_bundle_admission(
    basis: BundleAdmissionBasis,
    policy: BundleAdmissionPolicy,
    bundle_problem: BundleProblemGeneration,
    camera_domains: Vec<BundleCameraDomain>,
    seed_provenance: Vec<LandmarkSeedProvenance>,
) -> Result<BundleAdmissionGeneration, BundleAdmissionError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    bundle_problem.validate()?;
    let observed_problem_digest = bundle_problem.digest()?;
    require_basis_match(
        "bundle_problem_digest",
        &basis.bundle_problem_digest,
        &observed_problem_digest,
    )?;

    let camera_domains = canonicalize_camera_domains(camera_domains)?;
    let seed_provenance = canonicalize_seed_provenance(seed_provenance)?;
    let observed_camera_digest = camera_domain_table_digest(&camera_domains)?;
    require_basis_match(
        "camera_domain_basis_digest",
        &basis.camera_domain_basis_digest,
        &observed_camera_digest,
    )?;
    let observed_seed_digest = seed_provenance_table_digest(&seed_provenance)?;
    require_basis_match(
        "seed_provenance_basis_digest",
        &basis.seed_provenance_basis_digest,
        &observed_seed_digest,
    )?;
    let observed_policy_digest = bundle_admission_policy_digest(policy)?;
    require_basis_match(
        "policy_digest",
        &basis.policy_digest,
        &observed_policy_digest,
    )?;

    let mut budget = OperationBudget::new(policy.max_operations);
    let camera_by_node = bundle_problem
        .cameras
        .iter()
        .map(|camera| (camera.binding.camera_node_id, camera))
        .collect::<BTreeMap<_, _>>();
    let landmark_by_id = bundle_problem
        .landmarks
        .iter()
        .map(|landmark| (landmark.seed.landmark_id, landmark))
        .collect::<BTreeMap<_, _>>();
    let observation_by_id = bundle_problem
        .observations
        .iter()
        .map(|observation| (observation.observation.observation_id, observation))
        .collect::<BTreeMap<_, _>>();
    let domain_by_camera = camera_domains
        .iter()
        .map(|domain| (domain.camera_node_id, domain))
        .collect::<BTreeMap<_, _>>();
    let provenance_by_landmark = seed_provenance
        .iter()
        .map(|record| (record.landmark_id, record))
        .collect::<BTreeMap<_, _>>();

    validate_camera_domains(
        &camera_by_node,
        &domain_by_camera,
        &camera_domains,
        &mut budget,
    )?;
    validate_seed_provenance(
        &landmark_by_id,
        &observation_by_id,
        &provenance_by_landmark,
        &seed_provenance,
        &mut budget,
    )?;

    let support_observation_ids = seed_provenance
        .iter()
        .flat_map(|record| record.support_observation_ids.iter().copied())
        .collect::<BTreeSet<_>>();
    let observations = compile_observation_audits(
        policy,
        &bundle_problem,
        &domain_by_camera,
        &support_observation_ids,
        &mut budget,
    )?;
    let observation_audit_by_id = observations
        .iter()
        .map(|audit| (audit.observation_id, audit))
        .collect::<BTreeMap<_, _>>();
    let landmarks = compile_landmark_audits(
        policy,
        &bundle_problem,
        &provenance_by_landmark,
        &observation_audit_by_id,
        &observation_by_id,
        &mut budget,
    )?;
    let components = compile_component_audits(
        &bundle_problem,
        &observations,
        &landmarks,
        &mut budget,
    )?;

    Ok(BundleAdmissionGeneration {
        basis,
        policy,
        bundle_problem,
        camera_domains,
        seed_provenance,
        observations,
        landmarks,
        components,
        operation_count: budget.used,
    })
}

fn validate_basis(basis: &BundleAdmissionBasis) -> Result<(), BundleAdmissionError> {
    for (field, digest) in [
        ("bundle_problem_digest", &basis.bundle_problem_digest),
        (
            "camera_domain_basis_digest",
            &basis.camera_domain_basis_digest,
        ),
        (
            "seed_provenance_basis_digest",
            &basis.seed_provenance_basis_digest,
        ),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_nonzero("generation", basis.generation)
}

fn validate_policy(policy: BundleAdmissionPolicy) -> Result<(), BundleAdmissionError> {
    validate_bounded_nonzero(
        "max_seed_uncertainty_nano",
        policy.max_seed_uncertainty_nano,
        MAX_SEED_UNCERTAINTY_NANO,
    )?;
    for (field, value) in [
        (
            "min_seed_support_observations",
            policy.min_seed_support_observations,
        ),
        ("min_seed_support_cameras", policy.min_seed_support_cameras),
    ] {
        if value == 0 {
            return Err(BundleAdmissionError::ZeroValue { field });
        }
    }
    if policy.min_seed_support_cameras > policy.min_seed_support_observations {
        return Err(BundleAdmissionError::BoundExceeded {
            field: "min_seed_support_cameras",
            actual: u128::from(policy.min_seed_support_cameras),
            maximum: u128::from(policy.min_seed_support_observations),
        });
    }
    validate_bounded_nonzero(
        "max_operations",
        policy.max_operations,
        MAX_ADMISSION_OPERATIONS,
    )
}

fn canonicalize_camera_domains(
    mut domains: Vec<BundleCameraDomain>,
) -> Result<Vec<BundleCameraDomain>, BundleAdmissionError> {
    if domains.len() > MAX_CAMERA_DOMAINS {
        return Err(BundleAdmissionError::BoundExceeded {
            field: "camera_domain_count",
            actual: domains.len() as u128,
            maximum: MAX_CAMERA_DOMAINS as u128,
        });
    }
    domains.sort_by_key(|domain| domain.camera_node_id);
    let mut ids = BTreeSet::new();
    for domain in &domains {
        validate_safe_nonzero("camera_node_id", domain.camera_node_id)?;
        reject_zero_digest("camera_domain_frame_digest", &domain.frame_digest)?;
        reject_zero_digest(
            "camera_domain_effective_calibration_digest",
            &domain.effective_calibration_digest,
        )?;
        validate_image_dimension("image_width", domain.image_width)?;
        validate_image_dimension("image_height", domain.image_height)?;
        if !ids.insert(domain.camera_node_id) {
            return Err(BundleAdmissionError::DuplicateCameraDomain {
                camera_node_id: domain.camera_node_id,
            });
        }
    }
    Ok(domains)
}

fn canonicalize_seed_provenance(
    mut provenance: Vec<LandmarkSeedProvenance>,
) -> Result<Vec<LandmarkSeedProvenance>, BundleAdmissionError> {
    if provenance.len() > MAX_SEED_PROVENANCES {
        return Err(BundleAdmissionError::BoundExceeded {
            field: "seed_provenance_count",
            actual: provenance.len() as u128,
            maximum: MAX_SEED_PROVENANCES as u128,
        });
    }
    for record in &mut provenance {
        validate_safe_nonzero("seed_provenance_landmark_id", record.landmark_id)?;
        if record.support_observation_ids.len() > MAX_SEED_SUPPORT_OBSERVATIONS {
            return Err(BundleAdmissionError::BoundExceeded {
                field: "seed_support_observation_count",
                actual: record.support_observation_ids.len() as u128,
                maximum: MAX_SEED_SUPPORT_OBSERVATIONS as u128,
            });
        }
        record.support_observation_ids.sort_unstable();
        let mut ids = BTreeSet::new();
        for observation_id in &record.support_observation_ids {
            validate_safe_nonzero("seed_support_observation_id", *observation_id)?;
            if !ids.insert(*observation_id) {
                return Err(BundleAdmissionError::DuplicateSeedSupportObservation {
                    landmark_id: record.landmark_id,
                    observation_id: *observation_id,
                });
            }
        }
    }
    provenance.sort_by_key(|record| record.landmark_id);
    let mut landmark_ids = BTreeSet::new();
    for record in &provenance {
        if !landmark_ids.insert(record.landmark_id) {
            return Err(BundleAdmissionError::DuplicateSeedProvenance {
                landmark_id: record.landmark_id,
            });
        }
    }
    Ok(provenance)
}

fn validate_camera_domains(
    camera_by_node: &BTreeMap<u64, &fdgr_bundle_problem::CompiledBundleCamera>,
    domain_by_camera: &BTreeMap<u64, &BundleCameraDomain>,
    domains: &[BundleCameraDomain],
    budget: &mut OperationBudget,
) -> Result<(), BundleAdmissionError> {
    for (camera_node_id, camera) in camera_by_node {
        budget.charge()?;
        let domain = domain_by_camera.get(camera_node_id).copied().ok_or(
            BundleAdmissionError::MissingCameraDomain {
                camera_node_id: *camera_node_id,
            },
        )?;
        if domain.frame_digest != camera.binding.frame_digest {
            return Err(BundleAdmissionError::CameraFrameMismatch {
                camera_node_id: *camera_node_id,
            });
        }
        if domain.effective_calibration_digest != camera.binding.effective_calibration_digest {
            return Err(BundleAdmissionError::CameraCalibrationMismatch {
                camera_node_id: *camera_node_id,
            });
        }
    }
    for domain in domains {
        budget.charge()?;
        if !camera_by_node.contains_key(&domain.camera_node_id) {
            return Err(BundleAdmissionError::ExtraCameraDomain {
                camera_node_id: domain.camera_node_id,
            });
        }
    }
    Ok(())
}

fn validate_seed_provenance(
    landmark_by_id: &BTreeMap<u64, &fdgr_bundle_problem::CompiledBundleLandmark>,
    observation_by_id: &BTreeMap<u64, &fdgr_bundle_problem::CompiledBundleObservation>,
    provenance_by_landmark: &BTreeMap<u64, &LandmarkSeedProvenance>,
    provenance: &[LandmarkSeedProvenance],
    budget: &mut OperationBudget,
) -> Result<(), BundleAdmissionError> {
    for landmark_id in landmark_by_id.keys() {
        budget.charge()?;
        if !provenance_by_landmark.contains_key(landmark_id) {
            return Err(BundleAdmissionError::MissingSeedProvenance {
                landmark_id: *landmark_id,
            });
        }
    }
    for record in provenance {
        budget.charge()?;
        if !landmark_by_id.contains_key(&record.landmark_id) {
            return Err(BundleAdmissionError::ExtraSeedProvenance {
                landmark_id: record.landmark_id,
            });
        }
        for observation_id in &record.support_observation_ids {
            budget.charge()?;
            let compiled = observation_by_id.get(observation_id).copied().ok_or(
                BundleAdmissionError::UnknownSeedSupportObservation {
                    landmark_id: record.landmark_id,
                    observation_id: *observation_id,
                },
            )?;
            let observation = &compiled.observation;
            if observation.landmark_id != record.landmark_id {
                return Err(BundleAdmissionError::SeedSupportLandmarkMismatch {
                    landmark_id: record.landmark_id,
                    observation_id: *observation_id,
                    observed_landmark_id: observation.landmark_id,
                });
            }
            if observation.role != BundleObservationRole::Optimize {
                return Err(BundleAdmissionError::SeedSupportUsesHeldOut {
                    landmark_id: record.landmark_id,
                    observation_id: *observation_id,
                });
            }
        }
    }
    Ok(())
}

fn compile_observation_audits(
    policy: BundleAdmissionPolicy,
    problem: &BundleProblemGeneration,
    domain_by_camera: &BTreeMap<u64, &BundleCameraDomain>,
    support_observation_ids: &BTreeSet<u64>,
    budget: &mut OperationBudget,
) -> Result<Vec<BundleObservationAudit>, BundleAdmissionError> {
    let camera_by_node = problem
        .cameras
        .iter()
        .map(|camera| (camera.binding.camera_node_id, camera))
        .collect::<BTreeMap<_, _>>();
    let landmark_by_id = problem
        .landmarks
        .iter()
        .map(|landmark| (landmark.seed.landmark_id, landmark))
        .collect::<BTreeMap<_, _>>();
    let mut output = Vec::with_capacity(problem.observations.len());
    for compiled in &problem.observations {
        budget.charge()?;
        let observation = &compiled.observation;
        let camera = camera_by_node
            .get(&observation.camera_node_id)
            .copied()
            .ok_or(BundleAdmissionError::InternalLookup {
                field: "observation_camera",
            })?;
        let landmark = landmark_by_id
            .get(&observation.landmark_id)
            .copied()
            .ok_or(BundleAdmissionError::InternalLookup {
                field: "observation_landmark",
            })?;
        let domain = domain_by_camera
            .get(&observation.camera_node_id)
            .copied()
            .ok_or(BundleAdmissionError::InternalLookup {
                field: "observation_camera_domain",
            })?;
        let camera_active = camera.disposition != BundleCameraDisposition::PrunedInsufficientLandmarkSupport;
        let landmark_active = landmark.disposition == BundleLandmarkDisposition::ActiveTraining;
        let within_image_domain = coordinate_in_domain(
            observation.x_nano_pixels,
            observation.y_nano_pixels,
            domain.image_width,
            domain.image_height,
        );
        let seed_support_used = support_observation_ids.contains(&observation.observation_id);
        let disposition = if !within_image_domain {
            ObservationAuditDisposition::OutsideImageDomain
        } else if compiled.disposition == BundleObservationDisposition::ActiveOptimize {
            ObservationAuditDisposition::ActiveOptimizeInDomain
        } else if compiled.disposition == BundleObservationDisposition::EligibleHeldOut
            && policy.require_active_held_out_camera
            && !camera_active
        {
            ObservationAuditDisposition::HeldOutInactiveCamera
        } else if compiled.disposition == BundleObservationDisposition::EligibleHeldOut {
            ObservationAuditDisposition::EligibleHeldOutIndependent
        } else {
            ObservationAuditDisposition::UpstreamExcluded
        };
        output.push(BundleObservationAudit {
            observation_id: observation.observation_id,
            landmark_id: observation.landmark_id,
            camera_node_id: observation.camera_node_id,
            within_image_domain,
            camera_active,
            landmark_active,
            seed_support_used,
            disposition,
        });
    }
    output.sort_by_key(|audit| audit.observation_id);
    Ok(output)
}

fn compile_landmark_audits(
    policy: BundleAdmissionPolicy,
    problem: &BundleProblemGeneration,
    provenance_by_landmark: &BTreeMap<u64, &LandmarkSeedProvenance>,
    audit_by_observation: &BTreeMap<u64, &BundleObservationAudit>,
    observation_by_id: &BTreeMap<u64, &fdgr_bundle_problem::CompiledBundleObservation>,
    budget: &mut OperationBudget,
) -> Result<Vec<LandmarkSeedAudit>, BundleAdmissionError> {
    let mut output = Vec::with_capacity(problem.landmarks.len());
    for landmark in &problem.landmarks {
        budget.charge()?;
        let landmark_id = landmark.seed.landmark_id;
        let provenance = provenance_by_landmark
            .get(&landmark_id)
            .copied()
            .ok_or(BundleAdmissionError::InternalLookup {
                field: "landmark_seed_provenance",
            })?;
        let mut surviving_support_observation_ids = Vec::new();
        let mut surviving_support_camera_node_ids = BTreeSet::new();
        for observation_id in &provenance.support_observation_ids {
            budget.charge()?;
            let audit = audit_by_observation
                .get(observation_id)
                .copied()
                .ok_or(BundleAdmissionError::InternalLookup {
                    field: "landmark_support_observation_audit",
                })?;
            if audit.disposition == ObservationAuditDisposition::ActiveOptimizeInDomain {
                surviving_support_observation_ids.push(*observation_id);
                let compiled = observation_by_id
                    .get(observation_id)
                    .copied()
                    .ok_or(BundleAdmissionError::InternalLookup {
                        field: "landmark_support_observation",
                    })?;
                surviving_support_camera_node_ids.insert(compiled.observation.camera_node_id);
            }
        }
        let surviving_support_camera_node_ids = surviving_support_camera_node_ids
            .into_iter()
            .collect::<Vec<_>>();
        let seed_uncertainty_within_policy =
            landmark.seed.seed_uncertainty_nano <= policy.max_seed_uncertainty_nano;
        let provenance_proven = seed_uncertainty_within_policy
            && surviving_support_observation_ids.len()
                >= policy.min_seed_support_observations as usize
            && surviving_support_camera_node_ids.len()
                >= policy.min_seed_support_cameras as usize;
        output.push(LandmarkSeedAudit {
            landmark_id,
            declared_support_observation_ids: provenance.support_observation_ids.clone(),
            surviving_support_observation_ids,
            surviving_support_camera_node_ids,
            seed_uncertainty_within_policy,
            provenance_proven,
        });
    }
    output.sort_by_key(|audit| audit.landmark_id);
    Ok(output)
}

fn compile_component_audits(
    problem: &BundleProblemGeneration,
    observations: &[BundleObservationAudit],
    landmarks: &[LandmarkSeedAudit],
    budget: &mut OperationBudget,
) -> Result<Vec<BundleAdmissionComponent>, BundleAdmissionError> {
    let camera_root_by_node = problem
        .cameras
        .iter()
        .map(|camera| (camera.binding.camera_node_id, camera.component_root_node_id))
        .collect::<BTreeMap<_, _>>();
    let landmark_root_by_id = problem
        .landmarks
        .iter()
        .map(|landmark| (landmark.seed.landmark_id, landmark.seed.component_root_node_id))
        .collect::<BTreeMap<_, _>>();
    let active_landmark_ids = problem
        .landmarks
        .iter()
        .filter(|landmark| landmark.disposition == BundleLandmarkDisposition::ActiveTraining)
        .map(|landmark| landmark.seed.landmark_id)
        .collect::<BTreeSet<_>>();
    let upstream_observation_by_id = problem
        .observations
        .iter()
        .map(|compiled| (compiled.observation.observation_id, compiled))
        .collect::<BTreeMap<_, _>>();
    let landmark_audit_by_id = landmarks
        .iter()
        .map(|audit| (audit.landmark_id, audit))
        .collect::<BTreeMap<_, _>>();
    let mut output = Vec::with_capacity(problem.components.len());
    for component in &problem.components {
        budget.charge()?;
        let root = component.component_root_node_id;
        let mut invalid_image_observation_ids = Vec::new();
        let mut inactive_held_out_observation_ids = Vec::new();
        let mut independent_held_out_observation_ids = Vec::new();
        let mut independent_held_out_camera_node_ids = BTreeSet::new();
        for audit in observations {
            budget.charge()?;
            let observation_root = camera_root_by_node
                .get(&audit.camera_node_id)
                .copied()
                .ok_or(BundleAdmissionError::InternalLookup {
                    field: "component_observation_root",
                })?;
            if observation_root != root {
                continue;
            }
            let upstream = upstream_observation_by_id
                .get(&audit.observation_id)
                .copied()
                .ok_or(BundleAdmissionError::InternalLookup {
                    field: "component_upstream_observation",
                })?;
            if audit.disposition == ObservationAuditDisposition::OutsideImageDomain
                && matches!(
                    upstream.disposition,
                    BundleObservationDisposition::ActiveOptimize
                        | BundleObservationDisposition::EligibleHeldOut
                )
            {
                invalid_image_observation_ids.push(audit.observation_id);
            }
            match audit.disposition {
                ObservationAuditDisposition::HeldOutInactiveCamera => {
                    inactive_held_out_observation_ids.push(audit.observation_id);
                }
                ObservationAuditDisposition::EligibleHeldOutIndependent => {
                    independent_held_out_observation_ids.push(audit.observation_id);
                    independent_held_out_camera_node_ids.insert(audit.camera_node_id);
                }
                _ => {}
            }
        }
        let mut unproven_seed_landmark_ids = Vec::new();
        for landmark_id in &active_landmark_ids {
            budget.charge()?;
            if landmark_root_by_id.get(landmark_id).copied() != Some(root) {
                continue;
            }
            let audit = landmark_audit_by_id
                .get(landmark_id)
                .copied()
                .ok_or(BundleAdmissionError::InternalLookup {
                    field: "component_landmark_audit",
                })?;
            if !audit.provenance_proven {
                unproven_seed_landmark_ids.push(*landmark_id);
            }
        }
        invalid_image_observation_ids.sort_unstable();
        inactive_held_out_observation_ids.sort_unstable();
        independent_held_out_observation_ids.sort_unstable();
        unproven_seed_landmark_ids.sort_unstable();
        let independent_held_out_camera_node_ids = independent_held_out_camera_node_ids
            .into_iter()
            .collect::<Vec<_>>();
        let held_out_sufficient = independent_held_out_observation_ids.len()
            >= problem.policy.min_held_out_observations_per_component as usize
            && independent_held_out_camera_node_ids.len()
                >= problem.policy.min_held_out_cameras_per_component as usize;
        let status = if component.decision == BundleProblemDecision::Block {
            BundleAdmissionComponentStatus::BlockedUpstream
        } else if !invalid_image_observation_ids.is_empty() {
            BundleAdmissionComponentStatus::InvalidImageDomain
        } else if !unproven_seed_landmark_ids.is_empty() {
            BundleAdmissionComponentStatus::UnprovenSeedPartition
        } else if component.decision == BundleProblemDecision::AdmitDiagnostic {
            BundleAdmissionComponentStatus::UpstreamDiagnostic
        } else if !held_out_sufficient {
            BundleAdmissionComponentStatus::InsufficientIndependentHeldOut
        } else {
            BundleAdmissionComponentStatus::Admitted
        };
        let (decision, recommendation) = component_decision(status);
        output.push(BundleAdmissionComponent {
            component_root_node_id: root,
            scale_component_root_edge_id: component.scale_component_root_edge_id,
            status,
            decision,
            recommendation,
            invalid_image_observation_ids,
            unproven_seed_landmark_ids,
            inactive_held_out_observation_ids,
            independent_held_out_observation_ids,
            independent_held_out_camera_node_ids,
        });
    }
    output.sort_by_key(|component| component.component_root_node_id);
    Ok(output)
}

const fn component_decision(
    status: BundleAdmissionComponentStatus,
) -> (BundleAdmissionDecision, BundleAdmissionRecommendation) {
    match status {
        BundleAdmissionComponentStatus::BlockedUpstream => (
            BundleAdmissionDecision::Block,
            BundleAdmissionRecommendation::ResolveUpstreamStructuralProblem,
        ),
        BundleAdmissionComponentStatus::InvalidImageDomain => (
            BundleAdmissionDecision::Block,
            BundleAdmissionRecommendation::BindCorrectImageDomain,
        ),
        BundleAdmissionComponentStatus::UnprovenSeedPartition => (
            BundleAdmissionDecision::AdmitDiagnostic,
            BundleAdmissionRecommendation::RebuildSeedsFromOptimizePartition,
        ),
        BundleAdmissionComponentStatus::InsufficientIndependentHeldOut => (
            BundleAdmissionDecision::AdmitDiagnostic,
            BundleAdmissionRecommendation::ReserveIndependentHeldOutViews,
        ),
        BundleAdmissionComponentStatus::UpstreamDiagnostic => (
            BundleAdmissionDecision::AdmitDiagnostic,
            BundleAdmissionRecommendation::FollowUpstreamDiagnostic,
        ),
        BundleAdmissionComponentStatus::Admitted => (
            BundleAdmissionDecision::Admit,
            BundleAdmissionRecommendation::ProceedToBoundedBundleOptimization,
        ),
    }
}

fn coordinate_in_domain(x: i64, y: i64, width: u32, height: u32) -> bool {
    if x < 0 || y < 0 {
        return false;
    }
    let width_nano = i128::from(width) * i128::from(NANO_PIXEL_SCALE);
    let height_nano = i128::from(height) * i128::from(NANO_PIXEL_SCALE);
    i128::from(x) < width_nano && i128::from(y) < height_nano
}

fn validate_image_dimension(
    field: &'static str,
    value: u32,
) -> Result<(), BundleAdmissionError> {
    if value == 0 {
        return Err(BundleAdmissionError::ZeroValue { field });
    }
    if value > MAX_IMAGE_DIMENSION {
        return Err(BundleAdmissionError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_IMAGE_DIMENSION),
        });
    }
    Ok(())
}

fn validate_safe_nonzero(
    field: &'static str,
    value: u64,
) -> Result<(), BundleAdmissionError> {
    if value == 0 {
        return Err(BundleAdmissionError::ZeroValue { field });
    }
    if value > MAX_SAFE_JSON_INTEGER {
        return Err(BundleAdmissionError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        });
    }
    Ok(())
}

fn validate_bounded_nonzero(
    field: &'static str,
    value: u64,
    maximum: u64,
) -> Result<(), BundleAdmissionError> {
    if value == 0 {
        return Err(BundleAdmissionError::ZeroValue { field });
    }
    if value > maximum {
        return Err(BundleAdmissionError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(maximum),
        });
    }
    Ok(())
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), BundleAdmissionError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(BundleAdmissionError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn require_basis_match(
    field: &'static str,
    expected: &EvidenceDigest,
    observed: &EvidenceDigest,
) -> Result<(), BundleAdmissionError> {
    if expected == observed {
        Ok(())
    } else {
        Err(BundleAdmissionError::BasisMismatch {
            field,
            expected: expected.clone(),
            observed: observed.clone(),
        })
    }
}

fn encode_semantic_policy(encoder: &mut Encoder, policy: BundleAdmissionPolicy) {
    encoder.put_u64(policy.max_seed_uncertainty_nano);
    encoder.put_u32(policy.min_seed_support_observations);
    encoder.put_u32(policy.min_seed_support_cameras);
    encoder.put_bool(policy.require_active_held_out_camera);
}

fn encode_camera_domain(encoder: &mut Encoder, domain: &BundleCameraDomain) {
    encoder.put_u64(domain.camera_node_id);
    encoder.put_digest(&domain.frame_digest);
    encoder.put_digest(&domain.effective_calibration_digest);
    encoder.put_u32(domain.image_width);
    encoder.put_u32(domain.image_height);
}

fn encode_seed_provenance(
    encoder: &mut Encoder,
    provenance: &LandmarkSeedProvenance,
) -> Result<(), BundleAdmissionError> {
    encoder.put_u64(provenance.landmark_id);
    encode_u64s(encoder, &provenance.support_observation_ids)
}

fn encode_u64s(
    encoder: &mut Encoder,
    values: &[u64],
) -> Result<(), BundleAdmissionError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn hash_encoder(
    schema: &str,
    encoder: Encoder,
) -> Result<EvidenceDigest, BundleAdmissionError> {
    let domain = DigestDomain::parse(schema)?;
    Ok(hash_domain(&domain, encoder.as_bytes())?)
}

fn usize_to_u64(value: usize) -> Result<u64, BundleAdmissionError> {
    u64::try_from(value).map_err(|_| BundleAdmissionError::ArithmeticOverflow {
        field: "usize_to_u64",
    })
}
