impl EdgeScaleGeneration {
    /// Replays dependence collapse, consensus, graph analysis, gauge propagation, and cycle checks.
    ///
    /// # Errors
    ///
    /// Returns a stable construction failure or deterministic mismatch.
    pub fn validate(&self) -> Result<(), EdgeScaleError> {
        let rebuilt = reconcile_edge_scales(
            self.basis.clone(),
            self.policy,
            self.subjects.clone(),
            self.witnesses.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(EdgeScaleError::DerivedMismatch {
                field: "edge_scale_generation",
            })
        }
    }

    /// Returns deterministic canonical semantic bytes.
    ///
    /// The path-expansion ceiling and observed graph path expansions are operation evidence and do
    /// not alter successful semantic identity.
    ///
    /// # Errors
    ///
    /// Returns a construction, graph, domain, or canonical-codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, EdgeScaleError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(
            4_096_usize
                .saturating_add(self.subjects.len().saturating_mul(64))
                .saturating_add(self.witnesses.len().saturating_mul(128))
                .saturating_add(self.relations.len().saturating_mul(192))
                .saturating_add(self.scales.len().saturating_mul(64)),
        );
        encoder.put_str(EDGE_SCALE_GENERATION_SCHEMA)?;
        encode_basis(&mut encoder, &self.basis);
        encode_semantic_policy(&mut encoder, self.policy);
        encoder.put_digest(&self.topology.digest()?);
        encode_subjects(&mut encoder, &self.subjects)?;
        encode_witnesses(&mut encoder, &self.witnesses)?;
        encode_witness_assessments(&mut encoder, &self.witness_assessments)?;
        encode_relations(&mut encoder, &self.relations)?;
        encode_scales(&mut encoder, &self.scales)?;
        encode_relation_assessments(&mut encoder, &self.relation_assessments)?;
        encode_components(&mut encoder, &self.components)?;
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated semantic generation identity.
    ///
    /// # Errors
    ///
    /// Returns a construction, graph, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, EdgeScaleError> {
        let domain = DigestDomain::parse(EDGE_SCALE_GENERATION_SCHEMA)?;
        Ok(hash_domain(&domain, &self.to_canonical_bytes()?)?)
    }

    /// Returns the reconciled relative scale for one exact pose edge.
    ///
    /// # Errors
    ///
    /// Returns [`EdgeScaleError::InternalLookup`] when the edge is absent from this generation.
    pub fn scale_for_edge(&self, edge_id: u64) -> Result<&ReconciledEdgeScale, EdgeScaleError> {
        self.scales
            .binary_search_by_key(&edge_id, |scale| scale.edge_id)
            .ok()
            .and_then(|index| self.scales.get(index))
            .ok_or(EdgeScaleError::InternalLookup {
                field: "scale_for_edge",
            })
    }

    /// Returns the component containing one exact pose edge.
    ///
    /// # Errors
    ///
    /// Returns [`EdgeScaleError::InternalLookup`] when the edge is absent from every component.
    pub fn component_for_edge(
        &self,
        edge_id: u64,
    ) -> Result<&EdgeScaleComponent, EdgeScaleError> {
        self.components
            .iter()
            .find(|component| component.edge_ids.binary_search(&edge_id).is_ok())
            .ok_or(EdgeScaleError::InternalLookup {
                field: "component_for_edge",
            })
    }

    /// Renders deterministic field-ordered JSON for agents and public tooling.
    ///
    /// # Errors
    ///
    /// Returns a construction, identity, graph, or formatting error.
    pub fn to_json(&self) -> Result<String, EdgeScaleError> {
        let digest = self.digest()?;
        let topology_digest = self.topology.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{EDGE_SCALE_GENERATION_SCHEMA}\",\"generation_digest\":\"{digest}\",\"pose_graph_generation_digest\":\"{}\",\"edge_subject_basis_digest\":\"{}\",\"witness_basis_digest\":\"{}\",\"policy_digest\":\"{}\",\"topology_digest\":\"{}\",\"generation\":{},\"unit\":\"component_edge_scale_unit\",\"subject_count\":{},\"witness_count\":{},\"relation_count\":{},\"component_count\":{},\"path_expansions\":{},\"policy\":{{\"max_within_group_residual_ppm\":{},\"max_consensus_residual_ppm\":{},\"max_cycle_residual_ppm\":{},\"min_cross_validation_groups\":{},\"max_relative_scale_nano\":{},\"max_path_expansions\":{}}},\"witness_assessments\":[",
            self.basis.pose_graph_generation_digest,
            self.basis.edge_subject_basis_digest,
            self.basis.witness_basis_digest,
            self.basis.policy_digest,
            topology_digest,
            self.basis.generation,
            self.subjects.len(),
            self.witnesses.len(),
            self.relations.len(),
            self.components.len(),
            self.topology.path_expansions,
            self.policy.max_within_group_residual_ppm,
            self.policy.max_consensus_residual_ppm,
            self.policy.max_cycle_residual_ppm,
            self.policy.min_cross_validation_groups,
            self.policy.max_relative_scale_nano,
            self.policy.max_path_expansions,
        );
        for (index, assessment) in self.witness_assessments.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"witness_id\":{},\"relation_id\":{},\"disposition\":\"{}\",\"residual_ppm\":{}}}",
                assessment.witness_id,
                optional_u64_json(assessment.relation_id),
                assessment.disposition.as_str(),
                optional_u64_json(assessment.residual_ppm),
            )
            .map_err(json_rendering)?;
        }
        output.push_str("],\"relations\":[");
        for (index, relation) in self.relations.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"relation_id\":{},\"lower_edge_id\":{},\"higher_edge_id\":{},\"ratio_lower_over_higher_nano\":{},\"uncertainty_ppm\":{},\"evidence_rank\":{},\"independent_support_count\":{},\"supporting_witness_ids\":[",
                relation.relation_id,
                relation.lower_edge_id,
                relation.higher_edge_id,
                relation.ratio_lower_over_higher_nano,
                relation.uncertainty_ppm,
                relation.evidence_rank,
                relation.independent_support_count,
            )
            .map_err(json_rendering)?;
            push_u64_json_array(&mut output, &relation.supporting_witness_ids)?;
            output.push_str("],\"supporting_correlation_group_ids\":[");
            push_u64_json_array(
                &mut output,
                &relation.supporting_correlation_group_ids,
            )?;
            output.push_str("],\"rejected_witness_ids\":[");
            push_u64_json_array(&mut output, &relation.rejected_witness_ids)?;
            output.push_str("]}");
        }
        output.push_str("],\"scales\":[");
        for (index, scale) in self.scales.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"edge_id\":{},\"pose_component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"relative_scale_nano\":{},\"parent_edge_id\":{},\"parent_relation_id\":{}}}",
                scale.edge_id,
                scale.pose_component_root_node_id,
                scale.scale_component_root_edge_id,
                scale.relative_scale_nano,
                optional_u64_json(scale.parent_edge_id),
                optional_u64_json(scale.parent_relation_id),
            )
            .map_err(json_rendering)?;
        }
        output.push_str("],\"relation_assessments\":[");
        for (index, assessment) in self.relation_assessments.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"relation_id\":{},\"implied_ratio_nano\":{},\"residual_ppm\":{},\"status\":\"{}\"}}",
                assessment.relation_id,
                assessment.implied_ratio_nano,
                assessment.residual_ppm,
                assessment.status.as_str(),
            )
            .map_err(json_rendering)?;
        }
        output.push_str("],\"components\":[");
        for (index, component) in self.components.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"scale_component_root_edge_id\":{},\"pose_component_root_node_id\":{},\"status\":\"{}\",\"cross_validated\":{},\"edge_ids\":[",
                component.scale_component_root_edge_id,
                component.pose_component_root_node_id,
                component.status.as_str(),
                component.cross_validated,
            )
            .map_err(json_rendering)?;
            push_u64_json_array(&mut output, &component.edge_ids)?;
            output.push_str("],\"relation_ids\":[");
            push_u64_json_array(&mut output, &component.relation_ids)?;
            output.push_str("],\"forest_relation_ids\":[");
            push_u64_json_array(&mut output, &component.forest_relation_ids)?;
            output.push_str("],\"independent_correlation_group_ids\":[");
            push_u64_json_array(
                &mut output,
                &component.independent_correlation_group_ids,
            )?;
            output.push_str("],\"conflicting_relation_ids\":[");
            push_u64_json_array(&mut output, &component.conflicting_relation_ids)?;
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}
