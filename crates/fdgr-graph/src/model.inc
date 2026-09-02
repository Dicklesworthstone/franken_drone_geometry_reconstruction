impl GraphAnalysis {
    /// Replays every derived field.
    ///
    /// # Errors
    ///
    /// Returns a validation, budget, topology, or deterministic mismatch.
    pub fn validate(&self) -> Result<(), GraphError> {
        let rebuilt = analyze_graph(
            self.basis.clone(),
            GraphBudget {
                max_path_expansions: self.path_expansions.max(1),
            },
            self.nodes.clone(),
            self.edges.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(GraphError::DerivedMismatch {
                field: "graph_analysis",
            })
        }
    }

    /// Deterministic canonical semantic bytes.
    ///
    /// Operation ceilings and observed path expansions are excluded.
    ///
    /// # Errors
    ///
    /// Returns a validation or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, GraphError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(
            1_024 + self.nodes.len() * 48 + self.edges.len() * 80,
        );
        encoder.put_str(GRAPH_ANALYSIS_SCHEMA)?;
        encoder.put_digest(&self.basis.node_basis_digest);
        encoder.put_digest(&self.basis.edge_basis_digest);
        encoder.put_digest(&self.basis.selection_policy_digest);
        encoder.put_u64(self.basis.generation);
        encoder.put_u64(usize_to_u64(self.nodes.len())?);
        for node in &self.nodes {
            encoder.put_u64(node.node_id);
            encoder.put_digest(&node.evidence_digest);
        }
        encoder.put_u64(usize_to_u64(self.edges.len())?);
        for edge in &self.edges {
            encoder.put_u64(edge.edge_id);
            encoder.put_u64(edge.left_node_id);
            encoder.put_u64(edge.right_node_id);
            encoder.put_u64(edge.priority.primary);
            encoder.put_u64(edge.priority.secondary);
            encoder.put_digest(&edge.evidence_digest);
        }
        encode_u64s(&mut encoder, &self.forest_edge_ids)?;
        encode_u64s(&mut encoder, &self.non_forest_edge_ids)?;
        encoder.put_u64(usize_to_u64(self.components.len())?);
        for component in &self.components {
            encoder.put_u64(component.component_id);
            encode_u64s(&mut encoder, &component.node_ids)?;
            encode_u64s(&mut encoder, &component.edge_ids)?;
            encode_u64s(&mut encoder, &component.forest_edge_ids)?;
        }
        encoder.put_u64(usize_to_u64(self.fundamental_cycles.len())?);
        for cycle in &self.fundamental_cycles {
            encoder.put_u64(cycle.closing_edge_id);
            encoder.put_u64(cycle.left_node_id);
            encoder.put_u64(cycle.right_node_id);
            encode_u64s(&mut encoder, &cycle.forest_path_edge_ids)?;
        }
        encode_u64s(&mut encoder, &self.bridge_edge_ids)?;
        encoder.put_u64(self.cycle_rank);
        Ok(encoder.into_bytes())
    }

    /// Domain-separated graph-analysis identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, GraphError> {
        let domain = DigestDomain::parse(GRAPH_ANALYSIS_SCHEMA)?;
        Ok(hash_domain(&domain, &self.to_canonical_bytes()?)?)
    }

    /// Deterministic field-ordered JSON for agents and public tooling.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, GraphError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{GRAPH_ANALYSIS_SCHEMA}\",\"analysis_digest\":\"{digest}\",\"node_basis_digest\":\"{}\",\"edge_basis_digest\":\"{}\",\"selection_policy_digest\":\"{}\",\"generation\":{},\"node_count\":{},\"edge_count\":{},\"component_count\":{},\"cycle_rank\":{},\"path_expansions\":{},\"forest_edge_ids\":[",
            self.basis.node_basis_digest,
            self.basis.edge_basis_digest,
            self.basis.selection_policy_digest,
            self.basis.generation,
            self.nodes.len(),
            self.edges.len(),
            self.components.len(),
            self.cycle_rank,
            self.path_expansions,
        );
        push_u64s(&mut output, &self.forest_edge_ids)?;
        output.push_str("],\"non_forest_edge_ids\":[");
        push_u64s(&mut output, &self.non_forest_edge_ids)?;
        output.push_str("],\"bridge_edge_ids\":[");
        push_u64s(&mut output, &self.bridge_edge_ids)?;
        output.push_str("],\"components\":[");
        for (index, component) in self.components.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"component_id\":{},\"node_ids\":[",
                component.component_id
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &component.node_ids)?;
            output.push_str("],\"edge_ids\":[");
            push_u64s(&mut output, &component.edge_ids)?;
            output.push_str("],\"forest_edge_ids\":[");
            push_u64s(&mut output, &component.forest_edge_ids)?;
            output.push_str("]}");
        }
        output.push_str("],\"fundamental_cycles\":[");
        for (index, cycle) in self.fundamental_cycles.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"closing_edge_id\":{},\"left_node_id\":{},\"right_node_id\":{},\"forest_path_edge_ids\":[",
                cycle.closing_edge_id,
                cycle.left_node_id,
                cycle.right_node_id
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &cycle.forest_path_edge_ids)?;
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_u64s(encoder: &mut Encoder, values: &[u64]) -> Result<(), GraphError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn push_u64s(output: &mut String, values: &[u64]) -> Result<(), GraphError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn json_error(error: fmt::Error) -> GraphError {
    GraphError::JsonRendering(error.to_string())
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), GraphError> {
    if digest.to_bytes() == [0; 32] {
        Err(GraphError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn validate_safe_nonzero(field: &'static str, value: u64) -> Result<(), GraphError> {
    if value == 0 {
        return Err(GraphError::ZeroValue { field });
    }
    validate_safe_number(field, value)
}

fn validate_safe_number(field: &'static str, value: u64) -> Result<(), GraphError> {
    if value > MAX_SAFE_JSON_INTEGER {
        Err(GraphError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        })
    } else {
        Ok(())
    }
}

fn usize_to_u64(value: usize) -> Result<u64, GraphError> {
    u64::try_from(value).map_err(|_| GraphError::ArithmeticOverflow {
        field: "collection_length",
    })
}
