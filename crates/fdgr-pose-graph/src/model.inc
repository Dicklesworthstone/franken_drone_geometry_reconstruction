impl PoseGraphGeneration {
    /// Replays topology, orientation propagation, and cycle assessment.
    ///
    /// # Errors
    ///
    /// Returns a construction failure or deterministic mismatch.
    pub fn validate(&self) -> Result<(), PoseGraphError> {
        let rebuilt = build_pose_graph(
            self.basis.clone(),
            self.policy,
            self.nodes.clone(),
            self.edges.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(PoseGraphError::DerivedMismatch {
                field: "pose_graph_generation",
            })
        }
    }

    /// Returns deterministic canonical semantic bytes.
    ///
    /// The path-work ceiling and observed graph path expansions are cost evidence and are not
    /// included in the semantic identity after successful construction.
    ///
    /// # Errors
    ///
    /// Returns a construction or canonical-codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, PoseGraphError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(
            2_048_usize
                .saturating_add(self.nodes.len().saturating_mul(64))
                .saturating_add(self.edges.len().saturating_mul(192))
                .saturating_add(self.orientations.len().saturating_mul(128)),
        );
        encoder.put_str(POSE_GRAPH_GENERATION_SCHEMA)?;
        encode_basis(&mut encoder, &self.basis);
        encode_semantic_policy(&mut encoder, self.policy);
        encoder.put_digest(&self.topology.digest()?);
        encoder.put_u64(usize_to_u64(self.nodes.len())?);
        for node in &self.nodes {
            encode_node(&mut encoder, node);
        }
        encoder.put_u64(usize_to_u64(self.edges.len())?);
        for edge in &self.edges {
            encode_edge(&mut encoder, edge);
        }
        encoder.put_u64(usize_to_u64(self.orientations.len())?);
        for orientation in &self.orientations {
            encode_orientation(&mut encoder, orientation);
        }
        encoder.put_u64(usize_to_u64(self.cycle_assessments.len())?);
        for assessment in &self.cycle_assessments {
            encode_cycle(&mut encoder, assessment)?;
        }
        encoder.put_u64(usize_to_u64(self.components.len())?);
        for component in &self.components {
            encode_component(&mut encoder, component)?;
        }
        encoder.put_u8(self.translation_status.code());
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated pose-graph generation identity.
    ///
    /// # Errors
    ///
    /// Returns a construction, graph, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, PoseGraphError> {
        let domain = DigestDomain::parse(POSE_GRAPH_GENERATION_SCHEMA)?;
        Ok(hash_domain(&domain, &self.to_canonical_bytes()?)?)
    }

    /// Renders deterministic field-ordered JSON.
    ///
    /// No camera centers or physical positions are emitted because pairwise unit translation
    /// directions do not determine mutually comparable edge baselines.
    ///
    /// # Errors
    ///
    /// Returns a construction, identity, graph, or formatting error.
    pub fn to_json(&self) -> Result<String, PoseGraphError> {
        let digest = self.digest()?;
        let topology_digest = self.topology.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{POSE_GRAPH_GENERATION_SCHEMA}\",\"generation_digest\":\"{digest}\",\"node_basis_digest\":\"{}\",\"edge_basis_digest\":\"{}\",\"graph_selection_policy_digest\":\"{}\",\"rotation_policy_digest\":\"{}\",\"topology_digest\":\"{}\",\"generation\":{},\"node_count\":{},\"edge_count\":{},\"component_count\":{},\"cycle_count\":{},\"translation_status\":\"{}\",\"policy\":{{\"max_rotation_cycle_residual_ppm\":{},\"max_orientation_drift_ppm\":{},\"max_path_expansions\":{}}},\"forest_edge_ids\":[",
            self.basis.node_basis_digest,
            self.basis.edge_basis_digest,
            self.basis.graph_selection_policy_digest,
            self.basis.rotation_policy_digest,
            topology_digest,
            self.basis.generation,
            self.nodes.len(),
            self.edges.len(),
            self.components.len(),
            self.cycle_assessments.len(),
            self.translation_status.as_str(),
            self.policy.max_rotation_cycle_residual_ppm,
            self.policy.max_orientation_drift_ppm,
            self.policy.max_path_expansions,
        );
        push_u64_array(&mut output, &self.topology.forest_edge_ids)?;
        output.push_str("],\"bridge_edge_ids\":[");
        push_u64_array(&mut output, &self.topology.bridge_edge_ids)?;
        output.push_str("],\"orientations\":[");
        for (index, orientation) in self.orientations.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"node_id\":{},\"sample_index\":{},\"component_root_node_id\":{},\"parent_node_id\":{},\"parent_edge_id\":{},\"rotation_node_from_root_nano\":[",
                orientation.node_id,
                orientation.sample_index,
                orientation.component_root_node_id,
                optional_u64_json(orientation.parent_node_id),
                optional_u64_json(orientation.parent_edge_id),
            )
            .map_err(json_error)?;
            push_i64_array(&mut output, &orientation.rotation_node_from_root_nano)?;
            output.push_str("]}");
        }
        output.push_str("],\"cycle_assessments\":[");
        for (index, assessment) in self.cycle_assessments.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"closing_edge_id\":{},\"rotation_residual_ppm\":{},\"status\":\"{}\",\"forest_path_edge_ids\":[",
                assessment.closing_edge_id,
                assessment.rotation_residual_ppm,
                assessment.status.as_str(),
            )
            .map_err(json_error)?;
            push_u64_array(&mut output, &assessment.forest_path_edge_ids)?;
            output.push_str("]}");
        }
        output.push_str("],\"components\":[");
        for (index, component) in self.components.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"component_root_node_id\":{},\"orientation_status\":\"{}\",\"node_ids\":[",
                component.component_root_node_id,
                component.orientation_status.as_str(),
            )
            .map_err(json_error)?;
            push_u64_array(&mut output, &component.node_ids)?;
            output.push_str("],\"edge_ids\":[");
            push_u64_array(&mut output, &component.edge_ids)?;
            output.push_str("],\"forest_edge_ids\":[");
            push_u64_array(&mut output, &component.forest_edge_ids)?;
            output.push_str("],\"conflicting_edge_ids\":[");
            push_u64_array(&mut output, &component.conflicting_edge_ids)?;
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_basis(encoder: &mut Encoder, basis: &PoseGraphBasis) {
    encoder.put_digest(&basis.node_basis_digest);
    encoder.put_digest(&basis.edge_basis_digest);
    encoder.put_digest(&basis.graph_selection_policy_digest);
    encoder.put_digest(&basis.rotation_policy_digest);
    encoder.put_u64(basis.generation);
}

fn encode_semantic_policy(encoder: &mut Encoder, policy: PoseGraphPolicy) {
    encoder.put_u64(policy.max_rotation_cycle_residual_ppm);
    encoder.put_u64(policy.max_orientation_drift_ppm);
}

fn encode_node(encoder: &mut Encoder, node: &PoseNode) {
    encoder.put_u64(node.node_id);
    encoder.put_u64(node.sample_index);
    encoder.put_digest(&node.keyframe_digest);
}

fn encode_edge(encoder: &mut Encoder, edge: &RelativePoseEdge) {
    encoder.put_u64(edge.edge_id);
    encoder.put_digest(&edge.verification_digest);
    encoder.put_u64(edge.admitted_candidate_id);
    encoder.put_u64(edge.left_node_id);
    encoder.put_u64(edge.right_node_id);
    encoder.put_u64(edge.left_sample_index);
    encoder.put_u64(edge.right_sample_index);
    for value in edge.rotation_right_from_left_nano {
        encoder.put_i64(value);
    }
    for value in edge.translation_right_from_left_direction_nano {
        encoder.put_i64(value);
    }
    encoder.put_u32(edge.supported_match_count);
    encoder.put_u64(edge.median_residual_nano);
}

fn encode_orientation(encoder: &mut Encoder, orientation: &NodeOrientation) {
    encoder.put_u64(orientation.node_id);
    encoder.put_u64(orientation.sample_index);
    encoder.put_u64(orientation.component_root_node_id);
    for value in orientation.rotation_node_from_root_nano {
        encoder.put_i64(value);
    }
    encode_optional_u64(encoder, orientation.parent_node_id);
    encode_optional_u64(encoder, orientation.parent_edge_id);
}

fn encode_cycle(
    encoder: &mut Encoder,
    assessment: &RotationCycleAssessment,
) -> Result<(), PoseGraphError> {
    encoder.put_u64(assessment.closing_edge_id);
    encode_u64_array(encoder, &assessment.forest_path_edge_ids)?;
    encoder.put_u64(assessment.rotation_residual_ppm);
    encoder.put_u8(assessment.status.code());
    Ok(())
}

fn encode_component(
    encoder: &mut Encoder,
    component: &PoseGraphComponent,
) -> Result<(), PoseGraphError> {
    encoder.put_u64(component.component_root_node_id);
    encode_u64_array(encoder, &component.node_ids)?;
    encode_u64_array(encoder, &component.edge_ids)?;
    encode_u64_array(encoder, &component.forest_edge_ids)?;
    encoder.put_u8(component.orientation_status.code());
    encode_u64_array(encoder, &component.conflicting_edge_ids)
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

fn encode_u64_array(encoder: &mut Encoder, values: &[u64]) -> Result<(), PoseGraphError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn push_u64_array(output: &mut String, values: &[u64]) -> Result<(), PoseGraphError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn push_i64_array<const N: usize>(
    output: &mut String,
    values: &[i64; N],
) -> Result<(), PoseGraphError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn json_error(error: fmt::Error) -> PoseGraphError {
    PoseGraphError::JsonRendering(error.to_string())
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), PoseGraphError> {
    if digest.to_bytes() == [0; 32] {
        Err(PoseGraphError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn validate_safe_nonzero(field: &'static str, value: u64) -> Result<(), PoseGraphError> {
    if value == 0 {
        return Err(PoseGraphError::ZeroValue { field });
    }
    validate_safe_number(field, value)
}

fn validate_safe_number(field: &'static str, value: u64) -> Result<(), PoseGraphError> {
    if value > MAX_SAFE_JSON_INTEGER {
        Err(PoseGraphError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        })
    } else {
        Ok(())
    }
}

fn usize_to_u64(value: usize) -> Result<u64, PoseGraphError> {
    u64::try_from(value).map_err(|_| PoseGraphError::ArithmeticOverflow {
        field: "collection_length",
    })
}
