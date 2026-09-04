const fn component_decision(
    status: BundleProblemComponentStatus,
) -> (BundleProblemDecision, BundleProblemRecommendation) {
    match status {
        BundleProblemComponentStatus::BlockedPoseRefinement => (
            BundleProblemDecision::Block,
            BundleProblemRecommendation::ResolveUpstreamPoseConflict,
        ),
        BundleProblemComponentStatus::NoTrainingCore => (
            BundleProblemDecision::Block,
            BundleProblemRecommendation::AddMultiViewLandmarkSupport,
        ),
        BundleProblemComponentStatus::GaugeRootUnobserved => (
            BundleProblemDecision::Block,
            BundleProblemRecommendation::ObserveGaugeRootNeighborhood,
        ),
        BundleProblemComponentStatus::DisconnectedFromGaugeRoot => (
            BundleProblemDecision::Block,
            BundleProblemRecommendation::CaptureBridgingView,
        ),
        BundleProblemComponentStatus::TreeLike => (
            BundleProblemDecision::AdmitDiagnostic,
            BundleProblemRecommendation::ReinforceBridgeObservations,
        ),
        BundleProblemComponentStatus::Fragile => (
            BundleProblemDecision::AdmitDiagnostic,
            BundleProblemRecommendation::ReinforceBridgeObservations,
        ),
        BundleProblemComponentStatus::MissingHeldOutEvidence => (
            BundleProblemDecision::AdmitDiagnostic,
            BundleProblemRecommendation::ReserveIndependentHeldOutViews,
        ),
        BundleProblemComponentStatus::Redundant => (
            BundleProblemDecision::Admit,
            BundleProblemRecommendation::ProceedToBoundedBundleOptimization,
        ),
    }
}

fn observation_quality_eligible(
    policy: BundleProblemPolicy,
    observation: &BundleObservation,
) -> bool {
    !observation.dynamic_masked
        && observation.localization_uncertainty_nano_pixels
            <= policy.max_observation_uncertainty_nano_pixels
}

fn bundle_graph_node_table_digest(
    mappings: &[BundleGraphNodeMapping],
) -> Result<EvidenceDigest, BundleProblemError> {
    let mut encoder = Encoder::with_capacity(64_usize.saturating_add(mappings.len() * 96));
    encoder.put_str(BUNDLE_GRAPH_NODE_TABLE_SCHEMA)?;
    encoder.put_u64(usize_to_u64(mappings.len())?);
    for mapping in mappings {
        encoder.put_u64(mapping.graph_node_id);
        encoder.put_u8(mapping.kind.code());
        encoder.put_u64(mapping.domain_id);
        encoder.put_u64(mapping.component_root_node_id);
        encoder.put_digest(&mapping.evidence_digest);
    }
    hash_encoder(BUNDLE_GRAPH_NODE_TABLE_SCHEMA, encoder)
}

fn bundle_graph_edge_table_digest(
    edges: &[GraphEdge],
) -> Result<EvidenceDigest, BundleProblemError> {
    let mut encoder = Encoder::with_capacity(64_usize.saturating_add(edges.len() * 96));
    encoder.put_str(BUNDLE_GRAPH_EDGE_TABLE_SCHEMA)?;
    encoder.put_u64(usize_to_u64(edges.len())?);
    for edge in edges {
        encoder.put_u64(edge.edge_id);
        encoder.put_u64(edge.left_node_id);
        encoder.put_u64(edge.right_node_id);
        encoder.put_u64(edge.priority.primary);
        encoder.put_u64(edge.priority.secondary);
        encoder.put_digest(&edge.evidence_digest);
    }
    hash_encoder(BUNDLE_GRAPH_EDGE_TABLE_SCHEMA, encoder)
}

fn camera_binding_row_digest(
    binding: &BundleCameraBinding,
) -> Result<EvidenceDigest, BundleProblemError> {
    let mut encoder = Encoder::with_capacity(128);
    encoder.put_str(CAMERA_BINDING_ROW_SCHEMA)?;
    encode_camera_binding(&mut encoder, binding);
    hash_encoder(CAMERA_BINDING_ROW_SCHEMA, encoder)
}

fn landmark_seed_row_digest(
    seed: &BundleLandmarkSeed,
) -> Result<EvidenceDigest, BundleProblemError> {
    let mut encoder = Encoder::with_capacity(160);
    encoder.put_str(LANDMARK_SEED_ROW_SCHEMA)?;
    encode_landmark_seed(&mut encoder, seed);
    hash_encoder(LANDMARK_SEED_ROW_SCHEMA, encoder)
}

fn observation_row_digest(
    observation: &BundleObservation,
) -> Result<EvidenceDigest, BundleProblemError> {
    let mut encoder = Encoder::with_capacity(160);
    encoder.put_str(BUNDLE_OBSERVATION_ROW_SCHEMA)?;
    encode_observation(&mut encoder, observation);
    hash_encoder(BUNDLE_OBSERVATION_ROW_SCHEMA, encoder)
}

fn encode_camera_binding(encoder: &mut Encoder, binding: &BundleCameraBinding) {
    encoder.put_u64(binding.camera_node_id);
    encoder.put_u64(binding.sample_index);
    encoder.put_digest(&binding.frame_digest);
    encoder.put_digest(&binding.effective_calibration_digest);
}

fn encode_landmark_seed(encoder: &mut Encoder, seed: &BundleLandmarkSeed) {
    encoder.put_u64(seed.landmark_id);
    encoder.put_u64(seed.source_track_id);
    encoder.put_u64(seed.component_root_node_id);
    encode_optional_u64(encoder, seed.scale_component_root_edge_id);
    encoder.put_digest(&seed.seed_evidence_digest);
    for coordinate in seed.seed_position_from_root_nano {
        encoder.put_i64(coordinate);
    }
    encoder.put_u64(seed.seed_uncertainty_nano);
}

fn encode_observation(encoder: &mut Encoder, observation: &BundleObservation) {
    encoder.put_u64(observation.observation_id);
    encoder.put_u64(observation.landmark_id);
    encoder.put_u64(observation.camera_node_id);
    encoder.put_u64(observation.sample_index);
    encoder.put_digest(&observation.frame_digest);
    encoder.put_u64(observation.source_feature_observation_id);
    encoder.put_digest(&observation.evidence_digest);
    encoder.put_i64(observation.x_nano_pixels);
    encoder.put_i64(observation.y_nano_pixels);
    encoder.put_u64(observation.localization_uncertainty_nano_pixels);
    encoder.put_bool(observation.dynamic_masked);
    encoder.put_u8(observation.role.code());
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

fn hash_encoder(
    schema: &str,
    encoder: Encoder,
) -> Result<EvidenceDigest, BundleProblemError> {
    let domain = DigestDomain::parse(schema)?;
    Ok(hash_domain(&domain, encoder.as_bytes())?)
}

fn require_basis_match(
    field: &'static str,
    expected: &EvidenceDigest,
    observed: &EvidenceDigest,
) -> Result<(), BundleProblemError> {
    if expected == observed {
        Ok(())
    } else {
        Err(BundleProblemError::BasisMismatch {
            field,
            expected: expected.clone(),
            observed: observed.clone(),
        })
    }
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), BundleProblemError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(BundleProblemError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn validate_safe_nonzero(
    field: &'static str,
    value: u64,
) -> Result<(), BundleProblemError> {
    if value == 0 {
        return Err(BundleProblemError::ZeroValue { field });
    }
    if value > MAX_SAFE_JSON_INTEGER {
        return Err(BundleProblemError::BoundExceeded {
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
) -> Result<(), BundleProblemError> {
    if value == 0 {
        return Err(BundleProblemError::ZeroValue { field });
    }
    if value > maximum {
        return Err(BundleProblemError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(maximum),
        });
    }
    Ok(())
}

fn usize_to_u64(value: usize) -> Result<u64, BundleProblemError> {
    u64::try_from(value).map_err(|_| BundleProblemError::ArithmeticOverflow {
        field: "usize_to_u64",
    })
}

fn usize_to_u32(
    value: usize,
    field: &'static str,
) -> Result<u32, BundleProblemError> {
    u32::try_from(value).map_err(|_| BundleProblemError::ArithmeticOverflow { field })
}
