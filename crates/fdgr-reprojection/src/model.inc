impl ReprojectionEvaluation {
    /// Returns the maximum authority earned by the complete generation.
    ///
    /// Positive baseline authority requires every component to be admitted. Mixed, diagnostic,
    /// blocked, or empty component sets remain evidence-only.
    #[must_use]
    pub fn authority(&self) -> &'static str {
        if !self.components.is_empty()
            && self
                .components
                .iter()
                .all(|component| component.decision == ReprojectionDecision::Admit)
        {
            REPROJECTION_AUTHORITY
        } else {
            REPROJECTION_EVIDENCE_AUTHORITY
        }
    }

    /// Replays every derived field from retained exact inputs.
    ///
    /// # Errors
    ///
    /// Returns a stable upstream, basis, calibration, budget, arithmetic, or derived mismatch.
    pub fn validate(&self) -> Result<(), ReprojectionError> {
        let rebuilt = evaluate_reprojection(
            self.basis.clone(),
            self.policy,
            self.bundle_admission.clone(),
            self.calibrations.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(ReprojectionError::DerivedMismatch {
                field: "reprojection_evaluation",
            })
        }
    }

    /// Returns deterministic canonical semantic bytes.
    ///
    /// The successful operation ceiling and observed operation count remain cost evidence and are
    /// excluded from semantic identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, ReprojectionError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(
            2_048_usize
                .saturating_add(self.calibrations.len().saturating_mul(112))
                .saturating_add(self.observations.len().saturating_mul(192))
                .saturating_add(self.components.len().saturating_mul(256)),
        );
        encoder.put_str(REPROJECTION_EVALUATION_SCHEMA)?;
        encoder.put_digest(&self.basis.bundle_admission_digest);
        encoder.put_digest(&self.basis.calibration_table_digest);
        encoder.put_digest(&self.basis.algorithm_digest);
        encoder.put_digest(&self.basis.policy_digest);
        encoder.put_u64(self.basis.generation);
        encode_semantic_policy(&mut encoder, self.policy);
        encoder.put_u64(usize_to_u64(self.calibrations.len())?);
        for calibration in &self.calibrations {
            encode_calibration_identity(&mut encoder, calibration);
        }
        encoder.put_u64(usize_to_u64(self.observations.len())?);
        for observation in &self.observations {
            encoder.put_u64(observation.observation_id);
            encoder.put_u64(observation.landmark_id);
            encoder.put_u64(observation.camera_node_id);
            encoder.put_u64(observation.component_root_node_id);
            encoder.put_u8(role_code(observation.role));
            encode_optional_i64(&mut encoder, observation.projected_x_nano_pixels);
            encode_optional_i64(&mut encoder, observation.projected_y_nano_pixels);
            encode_optional_i64(&mut encoder, observation.depth_nano);
            encode_optional_i64(&mut encoder, observation.residual_x_nano_pixels);
            encode_optional_i64(&mut encoder, observation.residual_y_nano_pixels);
            encode_optional_u64(&mut encoder, observation.residual_nano_pixels);
            encode_optional_u64(&mut encoder, observation.normalized_residual_ppm);
            encoder.put_u64(observation.observation_uncertainty_nano_pixels);
            encoder.put_u64(observation.calibration_uncertainty_nano_pixels);
            match observation.within_image_domain {
                Some(value) => {
                    encoder.put_bool(true);
                    encoder.put_bool(value);
                }
                None => encoder.put_bool(false),
            }
            encoder.put_u8(observation.disposition.code());
        }
        encoder.put_u64(usize_to_u64(self.components.len())?);
        for component in &self.components {
            encoder.put_u64(component.component_root_node_id);
            encode_optional_u64(&mut encoder, component.scale_component_root_edge_id);
            encoder.put_u8(component.status.code());
            encoder.put_u8(component.decision.code());
            encoder.put_u8(component.recommendation.code());
            encode_residual_summary(&mut encoder, &component.optimize);
            encode_residual_summary(&mut encoder, &component.held_out);
            encode_u64s(&mut encoder, &component.unprojectable_observation_ids)?;
            encode_u64s(&mut encoder, &component.conflicting_observation_ids)?;
        }
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated semantic evaluation identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, ReprojectionError> {
        let domain = DigestDomain::parse(REPROJECTION_EVALUATION_SCHEMA)?;
        Ok(hash_domain(&domain, &self.to_canonical_bytes()?)?)
    }

    /// Renders deterministic field-ordered JSON for agents and public tooling.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, ReprojectionError> {
        let digest = self.digest()?;
        let authority = self.authority();
        let mut output = format!(
            "{{\"schema\":\"{REPROJECTION_EVALUATION_SCHEMA}\",\"reprojection_digest\":\"{digest}\",\"authority\":\"{authority}\",\"bundle_admission_digest\":\"{}\",\"calibration_table_digest\":\"{}\",\"algorithm_digest\":\"{}\",\"policy_digest\":\"{}\",\"generation\":{},\"calibration_count\":{},\"observation_count\":{},\"component_count\":{},\"operation_count\":{},\"policy\":{{\"minimum_positive_depth_nano\":{},\"max_normalized_coordinate_abs_nano\":{},\"max_projected_coordinate_abs_nano_pixels\":{},\"max_residual_nano_pixels\":{},\"max_normalized_residual_ppm\":{},\"min_optimize_observations_per_component\":{},\"min_held_out_observations_per_component\":{},\"max_operations\":{}}},\"calibrations\":[",
            self.basis.bundle_admission_digest,
            self.basis.calibration_table_digest,
            self.basis.algorithm_digest,
            self.basis.policy_digest,
            self.basis.generation,
            self.calibrations.len(),
            self.observations.len(),
            self.components.len(),
            self.operation_count,
            self.policy.projection.minimum_positive_depth_nano,
            self.policy.projection.max_normalized_coordinate_abs_nano,
            self.policy
                .projection
                .max_projected_coordinate_abs_nano_pixels,
            self.policy.max_residual_nano_pixels,
            self.policy.max_normalized_residual_ppm,
            self.policy.min_optimize_observations_per_component,
            self.policy.min_held_out_observations_per_component,
            self.policy.max_operations,
        );
        for (index, calibration) in self.calibrations.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"camera_node_id\":{},\"frame_digest\":\"{}\",\"effective_calibration_digest\":\"{}\",\"source_calibration_digest\":\"{}\",\"image_width\":{},\"image_height\":{},\"distortion_model\":\"{}\",\"rolling_readout\":{},\"declared_uncertainty_nano_pixels\":{}}}",
                calibration.camera_node_id,
                calibration.frame_digest,
                calibration.effective_calibration_digest,
                calibration.calibration.source_calibration_digest,
                calibration.calibration.derivation.output_width,
                calibration.calibration.derivation.output_height,
                calibration.calibration.distortion.as_str(),
                calibration.calibration.readout.rolling,
                calibration.calibration.declared_uncertainty_nano_pixels,
            )
            .map_err(json_error)?;
        }
        output.push_str("],\"observations\":[");
        for (index, observation) in self.observations.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"observation_id\":{},\"landmark_id\":{},\"camera_node_id\":{},\"component_root_node_id\":{},\"role\":\"{}\",\"projected_x_nano_pixels\":{},\"projected_y_nano_pixels\":{},\"depth_nano\":{},\"residual_x_nano_pixels\":{},\"residual_y_nano_pixels\":{},\"residual_nano_pixels\":{},\"normalized_residual_ppm\":{},\"observation_uncertainty_nano_pixels\":{},\"calibration_uncertainty_nano_pixels\":{},\"within_image_domain\":{},\"disposition\":\"{}\"}}",
                observation.observation_id,
                observation.landmark_id,
                observation.camera_node_id,
                observation.component_root_node_id,
                observation.role.as_str(),
                optional_i64_json(observation.projected_x_nano_pixels),
                optional_i64_json(observation.projected_y_nano_pixels),
                optional_i64_json(observation.depth_nano),
                optional_i64_json(observation.residual_x_nano_pixels),
                optional_i64_json(observation.residual_y_nano_pixels),
                optional_u64_json(observation.residual_nano_pixels),
                optional_u64_json(observation.normalized_residual_ppm),
                observation.observation_uncertainty_nano_pixels,
                observation.calibration_uncertainty_nano_pixels,
                optional_bool_json(observation.within_image_domain),
                observation.disposition.as_str(),
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
                "{{\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"status\":\"{}\",\"decision\":\"{}\",\"recommendation\":\"{}\",\"optimize\":",
                component.component_root_node_id,
                optional_u64_json(component.scale_component_root_edge_id),
                component.status.as_str(),
                component.decision.as_str(),
                component.recommendation.as_str(),
            )
            .map_err(json_error)?;
            push_residual_summary_json(&mut output, &component.optimize)?;
            output.push_str(",\"held_out\":");
            push_residual_summary_json(&mut output, &component.held_out)?;
            output.push_str(",\"unprojectable_observation_ids\":[");
            push_u64s(&mut output, &component.unprojectable_observation_ids)?;
            output.push_str("],\"conflicting_observation_ids\":[");
            push_u64s(&mut output, &component.conflicting_observation_ids)?;
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_residual_summary(
    encoder: &mut Encoder,
    summary: &ReprojectionResidualSummary,
) {
    encoder.put_u64(summary.eligible_observation_count);
    encoder.put_u64(summary.projected_observation_count);
    encoder.put_u64(summary.inlier_observation_count);
    encode_optional_u64(encoder, summary.rms_residual_nano_pixels);
    encode_optional_u64(encoder, summary.median_residual_nano_pixels);
    encode_optional_u64(encoder, summary.maximum_residual_nano_pixels);
}

fn push_residual_summary_json(
    output: &mut String,
    summary: &ReprojectionResidualSummary,
) -> Result<(), ReprojectionError> {
    write!(
        output,
        "{{\"eligible_observation_count\":{},\"projected_observation_count\":{},\"inlier_observation_count\":{},\"rms_residual_nano_pixels\":{},\"median_residual_nano_pixels\":{},\"maximum_residual_nano_pixels\":{}}}",
        summary.eligible_observation_count,
        summary.projected_observation_count,
        summary.inlier_observation_count,
        optional_u64_json(summary.rms_residual_nano_pixels),
        optional_u64_json(summary.median_residual_nano_pixels),
        optional_u64_json(summary.maximum_residual_nano_pixels),
    )
    .map_err(json_error)
}
