impl BundleAdmissionGeneration {
    /// Replays every derived field from retained exact inputs.
    ///
    /// # Errors
    ///
    /// Returns a stable basis, structural, audit, budget, codec, or derived mismatch.
    pub fn validate(&self) -> Result<(), BundleAdmissionError> {
        let rebuilt = audit_bundle_admission(
            self.basis.clone(),
            self.policy,
            self.bundle_problem.clone(),
            self.camera_domains.clone(),
            self.seed_provenance.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(BundleAdmissionError::DerivedMismatch {
                field: "bundle_admission_generation",
            })
        }
    }

    /// Returns deterministic canonical semantic bytes.
    ///
    /// Successful execution ceilings and observed operation count remain visible cost evidence but
    /// are excluded from semantic identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, BundleAdmissionError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(
            2_048_usize
                .saturating_add(self.camera_domains.len().saturating_mul(96))
                .saturating_add(self.seed_provenance.len().saturating_mul(96))
                .saturating_add(self.observations.len().saturating_mul(64))
                .saturating_add(self.landmarks.len().saturating_mul(128))
                .saturating_add(self.components.len().saturating_mul(256)),
        );
        encoder.put_str(BUNDLE_ADMISSION_SCHEMA)?;
        encoder.put_digest(&self.basis.bundle_problem_digest);
        encoder.put_digest(&self.basis.camera_domain_basis_digest);
        encoder.put_digest(&self.basis.seed_provenance_basis_digest);
        encoder.put_digest(&self.basis.policy_digest);
        encoder.put_u64(self.basis.generation);
        encode_semantic_policy(&mut encoder, self.policy);

        encoder.put_u64(usize_to_u64(self.camera_domains.len())?);
        for domain in &self.camera_domains {
            encode_camera_domain(&mut encoder, domain);
        }
        encoder.put_u64(usize_to_u64(self.seed_provenance.len())?);
        for provenance in &self.seed_provenance {
            encode_seed_provenance(&mut encoder, provenance)?;
        }
        encoder.put_u64(usize_to_u64(self.observations.len())?);
        for audit in &self.observations {
            encoder.put_u64(audit.observation_id);
            encoder.put_u64(audit.landmark_id);
            encoder.put_u64(audit.camera_node_id);
            encoder.put_bool(audit.within_image_domain);
            encoder.put_bool(audit.camera_active);
            encoder.put_bool(audit.landmark_active);
            encoder.put_bool(audit.seed_support_used);
            encoder.put_u8(audit.disposition.code());
        }
        encoder.put_u64(usize_to_u64(self.landmarks.len())?);
        for audit in &self.landmarks {
            encoder.put_u64(audit.landmark_id);
            encode_u64s(&mut encoder, &audit.declared_support_observation_ids)?;
            encode_u64s(&mut encoder, &audit.surviving_support_observation_ids)?;
            encode_u64s(&mut encoder, &audit.surviving_support_camera_node_ids)?;
            encoder.put_bool(audit.seed_uncertainty_within_policy);
            encoder.put_bool(audit.provenance_proven);
        }
        encoder.put_u64(usize_to_u64(self.components.len())?);
        for component in &self.components {
            encoder.put_u64(component.component_root_node_id);
            encode_optional_u64(&mut encoder, component.scale_component_root_edge_id);
            encoder.put_u8(component.status.code());
            encoder.put_u8(component.decision.code());
            encoder.put_u8(component.recommendation.code());
            encode_u64s(&mut encoder, &component.invalid_image_observation_ids)?;
            encode_u64s(&mut encoder, &component.unproven_seed_landmark_ids)?;
            encode_u64s(&mut encoder, &component.inactive_held_out_observation_ids)?;
            encode_u64s(
                &mut encoder,
                &component.independent_held_out_observation_ids,
            )?;
            encode_u64s(
                &mut encoder,
                &component.independent_held_out_camera_node_ids,
            )?;
        }
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated semantic audit identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, BundleAdmissionError> {
        let domain = DigestDomain::parse(BUNDLE_ADMISSION_SCHEMA)?;
        Ok(hash_domain(&domain, &self.to_canonical_bytes()?)?)
    }

    /// Renders deterministic field-ordered JSON for agents and public tooling.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, BundleAdmissionError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{BUNDLE_ADMISSION_SCHEMA}\",\"bundle_admission_digest\":\"{digest}\",\"authority\":\"{BUNDLE_ADMISSION_AUTHORITY}\",\"bundle_problem_digest\":\"{}\",\"camera_domain_basis_digest\":\"{}\",\"seed_provenance_basis_digest\":\"{}\",\"policy_digest\":\"{}\",\"generation\":{},\"camera_domain_count\":{},\"seed_provenance_count\":{},\"observation_audit_count\":{},\"landmark_audit_count\":{},\"component_count\":{},\"operation_count\":{},\"policy\":{{\"max_seed_uncertainty_nano\":{},\"min_seed_support_observations\":{},\"min_seed_support_cameras\":{},\"require_active_held_out_camera\":{},\"max_operations\":{}}},\"camera_domains\":[",
            self.basis.bundle_problem_digest,
            self.basis.camera_domain_basis_digest,
            self.basis.seed_provenance_basis_digest,
            self.basis.policy_digest,
            self.basis.generation,
            self.camera_domains.len(),
            self.seed_provenance.len(),
            self.observations.len(),
            self.landmarks.len(),
            self.components.len(),
            self.operation_count,
            self.policy.max_seed_uncertainty_nano,
            self.policy.min_seed_support_observations,
            self.policy.min_seed_support_cameras,
            self.policy.require_active_held_out_camera,
            self.policy.max_operations,
        );
        for (index, domain) in self.camera_domains.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"camera_node_id\":{},\"frame_digest\":\"{}\",\"effective_calibration_digest\":\"{}\",\"image_width\":{},\"image_height\":{}}}",
                domain.camera_node_id,
                domain.frame_digest,
                domain.effective_calibration_digest,
                domain.image_width,
                domain.image_height,
            )
            .map_err(json_error)?;
        }
        output.push_str("],\"seed_provenance\":[");
        for (index, provenance) in self.seed_provenance.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"landmark_id\":{},\"support_observation_ids\":[",
                provenance.landmark_id,
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &provenance.support_observation_ids)?;
            output.push_str("]}");
        }
        output.push_str("],\"observations\":[");
        for (index, audit) in self.observations.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"observation_id\":{},\"landmark_id\":{},\"camera_node_id\":{},\"within_image_domain\":{},\"camera_active\":{},\"landmark_active\":{},\"seed_support_used\":{},\"disposition\":\"{}\"}}",
                audit.observation_id,
                audit.landmark_id,
                audit.camera_node_id,
                audit.within_image_domain,
                audit.camera_active,
                audit.landmark_active,
                audit.seed_support_used,
                audit.disposition.as_str(),
            )
            .map_err(json_error)?;
        }
        output.push_str("],\"landmarks\":[");
        for (index, audit) in self.landmarks.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"landmark_id\":{},\"declared_support_observation_ids\":[",
                audit.landmark_id,
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &audit.declared_support_observation_ids)?;
            output.push_str("],\"surviving_support_observation_ids\":[");
            push_u64s(&mut output, &audit.surviving_support_observation_ids)?;
            output.push_str("],\"surviving_support_camera_node_ids\":[");
            push_u64s(&mut output, &audit.surviving_support_camera_node_ids)?;
            write!(
                output,
                "],\"seed_uncertainty_within_policy\":{},\"provenance_proven\":{}}}",
                audit.seed_uncertainty_within_policy,
                audit.provenance_proven,
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
                "{{\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"status\":\"{}\",\"decision\":\"{}\",\"recommendation\":\"{}\",\"invalid_image_observation_ids\":[",
                component.component_root_node_id,
                optional_u64_json(component.scale_component_root_edge_id),
                component.status.as_str(),
                component.decision.as_str(),
                component.recommendation.as_str(),
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &component.invalid_image_observation_ids)?;
            output.push_str("],\"unproven_seed_landmark_ids\":[");
            push_u64s(&mut output, &component.unproven_seed_landmark_ids)?;
            output.push_str("],\"inactive_held_out_observation_ids\":[");
            push_u64s(&mut output, &component.inactive_held_out_observation_ids)?;
            output.push_str("],\"independent_held_out_observation_ids\":[");
            push_u64s(
                &mut output,
                &component.independent_held_out_observation_ids,
            )?;
            output.push_str("],\"independent_held_out_camera_node_ids\":[");
            push_u64s(
                &mut output,
                &component.independent_held_out_camera_node_ids,
            )?;
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

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |value| value.to_string())
}

fn push_u64s(output: &mut String, values: &[u64]) -> Result<(), BundleAdmissionError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn json_error(error: fmt::Error) -> BundleAdmissionError {
    BundleAdmissionError::JsonRendering(error.to_string())
}
