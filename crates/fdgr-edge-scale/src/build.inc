#[derive(Clone, Debug)]
struct CleanGroup {
    correlation_group_id: u64,
    representative_witness_id: u64,
    ratio_numerator: u64,
    ratio_denominator: u64,
    witness_ids: Vec<u64>,
    uncertainty_ppm: u32,
    evidence_rank: u64,
    support_count: u64,
    internal_residual_ppm: u64,
}

#[derive(Clone, Debug)]
struct ConsensusCandidate {
    group_index: usize,
    inlier_indices: Vec<usize>,
    rank_sum: u64,
    support_sum: u64,
    residual_sum: u64,
}

/// Derives the exact edge-subject table from a validated pose-graph generation.
///
/// # Errors
///
/// Returns a pose-graph validation, lookup, canonical codec, or identity-domain error.
pub fn derive_edge_scale_subjects(
    pose_graph: &PoseGraphGeneration,
) -> Result<(EvidenceDigest, Vec<EdgeScaleSubject>), EdgeScaleError> {
    pose_graph.validate()?;
    let component_by_edge = pose_graph
        .components
        .iter()
        .flat_map(|component| {
            component
                .edge_ids
                .iter()
                .map(move |edge_id| (*edge_id, component.component_root_node_id))
        })
        .collect::<BTreeMap<_, _>>();
    let mut subjects = Vec::with_capacity(pose_graph.edges.len());
    for edge in &pose_graph.edges {
        let pose_component_root_node_id = *component_by_edge
            .get(&edge.edge_id)
            .ok_or(EdgeScaleError::InternalLookup {
                field: "pose_component_for_edge",
            })?;
        subjects.push(EdgeScaleSubject {
            edge_id: edge.edge_id,
            pose_verification_digest: edge.verification_digest.clone(),
            pose_component_root_node_id,
        });
    }
    let subjects = canonicalize_subjects(subjects)?;
    let digest = edge_subject_table_digest(&subjects)?;
    Ok((digest, subjects))
}

/// Computes the exact semantic identity of a canonical edge-subject table.
///
/// # Errors
///
/// Returns a subject validation, canonical codec, or identity-domain error.
pub fn edge_subject_table_digest(
    subjects: &[EdgeScaleSubject],
) -> Result<EvidenceDigest, EdgeScaleError> {
    let canonical = canonicalize_subjects(subjects.to_vec())?;
    let mut encoder = Encoder::with_capacity(128 + canonical.len().saturating_mul(56));
    encoder.put_str("fdgr.edge_scale_subjects/1")?;
    encoder.put_u64(usize_to_u64(canonical.len())?);
    for subject in &canonical {
        encoder.put_u64(subject.edge_id);
        encoder.put_digest(&subject.pose_verification_digest);
        encoder.put_u64(subject.pose_component_root_node_id);
    }
    let domain = DigestDomain::parse("fdgr.edge_scale_subjects/1")?;
    Ok(hash_domain(&domain, &encoder.into_bytes())?)
}

/// Computes the exact semantic identity of a canonical raw ratio-witness table.
///
/// The subject table is required because witness normalization validates endpoint existence and
/// representability against the exact pose-edge universe.
///
/// # Errors
///
/// Returns a subject, witness, canonical-codec, or identity-domain error.
pub fn edge_witness_table_digest(
    subjects: &[EdgeScaleSubject],
    witnesses: &[EdgeRatioWitness],
) -> Result<EvidenceDigest, EdgeScaleError> {
    let subjects = canonicalize_subjects(subjects.to_vec())?;
    let witnesses = canonicalize_witnesses(witnesses.to_vec(), &subjects)?;
    edge_witness_table_digest_canonical(&witnesses)
}

fn edge_witness_table_digest_canonical(
    witnesses: &[EdgeRatioWitness],
) -> Result<EvidenceDigest, EdgeScaleError> {
    let mut encoder = Encoder::with_capacity(128 + witnesses.len().saturating_mul(128));
    encoder.put_str("fdgr.edge_scale_witnesses/1")?;
    encoder.put_u64(usize_to_u64(witnesses.len())?);
    for witness in witnesses {
        encoder.put_u64(witness.witness_id);
        encoder.put_digest(&witness.evidence_digest);
        encoder.put_u64(witness.correlation_group_id);
        encoder.put_u64(witness.lower_edge_id);
        encoder.put_u64(witness.higher_edge_id);
        encoder.put_u64(witness.ratio_numerator);
        encoder.put_u64(witness.ratio_denominator);
        encoder.put_u32(witness.uncertainty_ppm);
        encoder.put_u32(witness.support_count);
        encoder.put_u8(witness.source.code());
    }
    let domain = DigestDomain::parse("fdgr.edge_scale_witnesses/1")?;
    Ok(hash_domain(&domain, &encoder.into_bytes())?)
}

/// Reconciles edge scales directly from an exact pose-graph generation.
///
/// This integrated entry point independently derives and verifies the pose-graph, edge-subject,
/// and witness identities before admitting any ratio evidence.
///
/// # Errors
///
/// Returns a pose-graph, basis, witness, consensus, graph, or arithmetic error.
pub fn reconcile_pose_graph_edge_scales(
    basis: EdgeScaleBasis,
    policy: EdgeScalePolicy,
    pose_graph: &PoseGraphGeneration,
    witnesses: Vec<EdgeRatioWitness>,
) -> Result<EdgeScaleGeneration, EdgeScaleError> {
    let observed_pose_graph_digest = pose_graph.digest()?;
    if observed_pose_graph_digest != basis.pose_graph_generation_digest {
        return Err(EdgeScaleError::BasisMismatch {
            field: "pose_graph_generation_digest",
            expected: basis.pose_graph_generation_digest,
            observed: observed_pose_graph_digest,
        });
    }
    let (observed_subject_digest, subjects) = derive_edge_scale_subjects(pose_graph)?;
    if observed_subject_digest != basis.edge_subject_basis_digest {
        return Err(EdgeScaleError::BasisMismatch {
            field: "edge_subject_basis_digest",
            expected: basis.edge_subject_basis_digest,
            observed: observed_subject_digest,
        });
    }
    reconcile_edge_scales(basis, policy, subjects, witnesses)
}

/// Reconciles pose-edge magnitude ratios in one arbitrary gauge per connected scale component.
///
/// # Errors
///
/// Returns a stable basis, policy, subject, witness, graph, arithmetic, or replay error.
pub fn reconcile_edge_scales(
    basis: EdgeScaleBasis,
    policy: EdgeScalePolicy,
    subjects: Vec<EdgeScaleSubject>,
    witnesses: Vec<EdgeRatioWitness>,
) -> Result<EdgeScaleGeneration, EdgeScaleError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    let subjects = canonicalize_subjects(subjects)?;
    let observed_subject_digest = edge_subject_table_digest(&subjects)?;
    if observed_subject_digest != basis.edge_subject_basis_digest {
        return Err(EdgeScaleError::BasisMismatch {
            field: "edge_subject_basis_digest",
            expected: basis.edge_subject_basis_digest,
            observed: observed_subject_digest,
        });
    }
    let witnesses = canonicalize_witnesses(witnesses, &subjects)?;
    let observed_witness_digest = edge_witness_table_digest_canonical(&witnesses)?;
    if observed_witness_digest != basis.witness_basis_digest {
        return Err(EdgeScaleError::BasisMismatch {
            field: "witness_basis_digest",
            expected: basis.witness_basis_digest,
            observed: observed_witness_digest,
        });
    }
    let (relations, witness_assessments) = aggregate_relations(&subjects, &witnesses, policy)?;
    let topology = analyze_relation_graph(&basis, &subjects, &relations, policy)?;
    let scales = propagate_scales(&subjects, &relations, &topology, policy)?;
    let relation_assessments = assess_relations(&relations, &topology, &scales, policy)?;
    let components = build_scale_components(
        &subjects,
        &relations,
        &topology,
        &relation_assessments,
        policy,
    )?;
    Ok(EdgeScaleGeneration {
        basis,
        policy,
        subjects,
        witnesses,
        witness_assessments,
        relations,
        topology,
        scales,
        relation_assessments,
        components,
    })
}

fn validate_basis(basis: &EdgeScaleBasis) -> Result<(), EdgeScaleError> {
    for (field, digest) in [
        (
            "pose_graph_generation_digest",
            &basis.pose_graph_generation_digest,
        ),
        (
            "edge_subject_basis_digest",
            &basis.edge_subject_basis_digest,
        ),
        ("witness_basis_digest", &basis.witness_basis_digest),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_nonzero("generation", basis.generation)
}

fn validate_policy(policy: EdgeScalePolicy) -> Result<(), EdgeScaleError> {
    for (field, value) in [
        (
            "max_within_group_residual_ppm",
            policy.max_within_group_residual_ppm,
        ),
        (
            "max_consensus_residual_ppm",
            policy.max_consensus_residual_ppm,
        ),
        ("max_cycle_residual_ppm", policy.max_cycle_residual_ppm),
    ] {
        if value > MAX_SCALE_RESIDUAL_PPM {
            return Err(EdgeScaleError::BoundExceeded {
                field,
                actual: u128::from(value),
                maximum: u128::from(MAX_SCALE_RESIDUAL_PPM),
            });
        }
    }
    if policy.max_within_group_residual_ppm > policy.max_consensus_residual_ppm {
        return Err(EdgeScaleError::InvalidPolicy {
            field: "within_group_must_not_exceed_consensus",
        });
    }
    if policy.min_cross_validation_groups == 0 {
        return Err(EdgeScaleError::ZeroValue {
            field: "min_cross_validation_groups",
        });
    }
    if policy.max_relative_scale_nano < UNIT_NANO
        || policy.max_relative_scale_nano > MAX_RELATIVE_SCALE_NANO
    {
        return Err(EdgeScaleError::InvalidPolicy {
            field: "max_relative_scale_nano",
        });
    }
    if policy.max_path_expansions == 0 {
        return Err(EdgeScaleError::ZeroValue {
            field: "max_path_expansions",
        });
    }
    Ok(())
}

fn canonicalize_subjects(
    mut subjects: Vec<EdgeScaleSubject>,
) -> Result<Vec<EdgeScaleSubject>, EdgeScaleError> {
    if subjects.len() > MAX_SCALE_EDGES {
        return Err(EdgeScaleError::BoundExceeded {
            field: "subject_count",
            actual: usize_to_u128(subjects.len()),
            maximum: usize_to_u128(MAX_SCALE_EDGES),
        });
    }
    subjects.sort_by_key(|subject| subject.edge_id);
    let mut edge_ids = BTreeSet::new();
    let mut verification_ids = BTreeSet::new();
    for subject in &subjects {
        validate_safe_nonzero("edge_id", subject.edge_id)?;
        validate_safe_nonzero(
            "pose_component_root_node_id",
            subject.pose_component_root_node_id,
        )?;
        reject_zero_digest(
            "pose_verification_digest",
            &subject.pose_verification_digest,
        )?;
        if !edge_ids.insert(subject.edge_id) {
            return Err(EdgeScaleError::DuplicateSubject {
                field: "edge_id",
                value: subject.edge_id,
            });
        }
        if !verification_ids.insert(subject.pose_verification_digest.clone()) {
            return Err(EdgeScaleError::DuplicateSubject {
                field: "pose_verification_digest",
                value: subject.edge_id,
            });
        }
    }
    Ok(subjects)
}
