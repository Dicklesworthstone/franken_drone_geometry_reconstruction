impl EpipolarVerification {
    /// Replays candidate normalization, residual scoring, ranking, and admission.
    ///
    /// # Errors
    ///
    /// Returns a validation, evaluation, or deterministic replay mismatch.
    pub fn validate(&self) -> Result<(), EpipolarError> {
        let rebuilt = verify_epipolar_candidates(
            self.basis.clone(),
            self.policy,
            self.observations.clone(),
            self.candidates.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(EpipolarError::DerivedMismatch {
                field: "epipolar_verification",
            })
        }
    }

    /// Returns deterministic canonical semantic bytes.
    ///
    /// Operation budget and observed evaluation count are deliberately excluded after successful
    /// completion; they remain visible cost evidence in the public JSON projection.
    ///
    /// # Errors
    ///
    /// Returns a validation or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, EpipolarError> {
        self.validate()?;
        let capacity = 2_048_usize
            .saturating_add(self.observations.len().saturating_mul(96))
            .saturating_add(self.candidates.len().saturating_mul(96))
            .saturating_add(
                self.evaluations
                    .iter()
                    .map(|evaluation| evaluation.residuals.len().saturating_mul(48))
                    .sum::<usize>(),
            );
        let mut encoder = Encoder::with_capacity(capacity);
        encoder.put_str(EPIPOLAR_VERIFICATION_SCHEMA)?;
        encode_basis(&mut encoder, &self.basis);
        encode_policy_semantics(&mut encoder, self.policy);
        encoder.put_u64(usize_to_u64(self.observations.len())?);
        for observation in &self.observations {
            encode_observation(&mut encoder, observation);
        }
        encoder.put_u64(usize_to_u64(self.candidates.len())?);
        for candidate in &self.candidates {
            encode_candidate(&mut encoder, candidate);
        }
        encoder.put_u64(usize_to_u64(self.evaluations.len())?);
        for evaluation in &self.evaluations {
            encode_evaluation(&mut encoder, evaluation)?;
        }
        encoder.put_u8(self.decision.code());
        encode_optional_u64(&mut encoder, self.best_candidate_id);
        encode_optional_u64(&mut encoder, self.runner_up_candidate_id);
        encode_optional_u64(&mut encoder, self.admitted_candidate_id);
        encoder.put_u32(self.inlier_margin);
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated semantic generation identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, EpipolarError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(EPIPOLAR_VERIFICATION_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Renders deterministic field-ordered JSON.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, EpipolarError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{EPIPOLAR_VERIFICATION_SCHEMA}\",\"verification_digest\":\"{digest}\",\"correspondence_generation_digest\":\"{}\",\"calibration_digest\":\"{}\",\"observation_basis_digest\":\"{}\",\"candidate_basis_digest\":\"{}\",\"policy_digest\":\"{}\",\"pair_id\":{},\"left_sample_index\":{},\"right_sample_index\":{},\"generation\":{},\"observation_count\":{},\"candidate_count\":{},\"evaluation_count\":{},\"decision\":\"{}\",\"best_candidate_id\":{},\"runner_up_candidate_id\":{},\"admitted_candidate_id\":{},\"inlier_margin\":{},\"policy\":{{\"max_residual_ppm\":{},\"min_inliers\":{},\"min_inlier_ratio_ppm\":{},\"min_spatial_bins_per_image\":{},\"max_determinant_residual_ppm\":{},\"min_inlier_margin\":{},\"max_evaluations\":{}}},\"candidates\":[",
            self.basis.correspondence_generation_digest,
            self.basis.calibration_digest,
            self.basis.observation_basis_digest,
            self.basis.candidate_basis_digest,
            self.basis.policy_digest,
            self.basis.pair_id,
            self.basis.left_sample_index,
            self.basis.right_sample_index,
            self.basis.generation,
            self.observations.len(),
            self.candidates.len(),
            self.evaluation_count,
            self.decision.as_str(),
            optional_u64_json(self.best_candidate_id),
            optional_u64_json(self.runner_up_candidate_id),
            optional_u64_json(self.admitted_candidate_id),
            self.inlier_margin,
            self.policy.max_residual_ppm,
            self.policy.min_inliers,
            self.policy.min_inlier_ratio_ppm,
            self.policy.min_spatial_bins_per_image,
            self.policy.max_determinant_residual_ppm,
            self.policy.min_inlier_margin,
            self.policy.max_evaluations,
        );
        for (position, candidate) in self.candidates.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"candidate_id\":{},\"source\":\"{}\",\"matrix\":[",
                candidate.candidate_id,
                candidate.source.as_str(),
            )
            .map_err(json_rendering)?;
            push_i64_json_array(&mut output, &candidate.matrix)?;
            output.push_str("]}");
        }
        output.push_str("],\"evaluations\":[");
        for (position, evaluation) in self.evaluations.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"candidate_id\":{},\"determinant_residual_ppm\":{},\"determinant_gate_passed\":{},\"inlier_count\":{},\"outlier_count\":{},\"inlier_ratio_ppm\":{},\"left_spatial_bin_count\":{},\"right_spatial_bin_count\":{},\"median_inlier_residual_ppm\":{},\"maximum_inlier_residual_ppm\":{},\"passes_admission_gates\":{},\"inlier_match_ids\":[",
                evaluation.candidate_id,
                evaluation.determinant_residual_ppm,
                evaluation.determinant_gate_passed,
                evaluation.inlier_count,
                evaluation.outlier_count,
                evaluation.inlier_ratio_ppm,
                evaluation.left_spatial_bin_count,
                evaluation.right_spatial_bin_count,
                evaluation.median_inlier_residual_ppm,
                evaluation.maximum_inlier_residual_ppm,
                evaluation.passes_admission_gates,
            )
            .map_err(json_rendering)?;
            push_u64_json_array(&mut output, &evaluation.inlier_match_ids)?;
            output.push_str("],\"outlier_match_ids\":[");
            push_u64_json_array(&mut output, &evaluation.outlier_match_ids)?;
            output.push_str("],\"residuals\":[");
            for (residual_position, residual) in evaluation.residuals.iter().enumerate() {
                if residual_position > 0 {
                    output.push(',');
                }
                write!(
                    output,
                    "{{\"match_id\":{},\"residual_ppm\":{},\"gate_ppm\":{},\"disposition\":\"{}\"}}",
                    residual.match_id,
                    optional_u64_json(residual.residual_ppm),
                    residual.gate_ppm,
                    residual.disposition.as_str(),
                )
                .map_err(json_rendering)?;
            }
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_basis(encoder: &mut Encoder, basis: &EpipolarBasis) {
    encoder.put_digest(&basis.correspondence_generation_digest);
    encoder.put_digest(&basis.calibration_digest);
    encoder.put_digest(&basis.observation_basis_digest);
    encoder.put_digest(&basis.candidate_basis_digest);
    encoder.put_digest(&basis.policy_digest);
    encoder.put_u64(basis.pair_id);
    encoder.put_u64(basis.left_sample_index);
    encoder.put_u64(basis.right_sample_index);
    encoder.put_u64(basis.generation);
}

fn encode_policy_semantics(encoder: &mut Encoder, policy: EpipolarPolicy) {
    encoder.put_u64(policy.max_residual_ppm);
    encoder.put_u32(policy.min_inliers);
    encoder.put_u32(policy.min_inlier_ratio_ppm);
    encoder.put_u16(policy.min_spatial_bins_per_image);
    encoder.put_u64(policy.max_determinant_residual_ppm);
    encoder.put_u32(policy.min_inlier_margin);
}

fn encode_observation(encoder: &mut Encoder, observation: &NormalizedCorrespondence) {
    encoder.put_u64(observation.match_id);
    encoder.put_u64(observation.pair_id);
    encoder.put_u64(observation.left_observation_id);
    encoder.put_u64(observation.right_observation_id);
    encoder.put_i64(observation.left_x_ppm);
    encoder.put_i64(observation.left_y_ppm);
    encoder.put_i64(observation.right_x_ppm);
    encoder.put_i64(observation.right_y_ppm);
    encoder.put_u32(observation.uncertainty_ppm);
    encoder.put_u16(observation.left_spatial_bin);
    encoder.put_u16(observation.right_spatial_bin);
}

fn encode_candidate(encoder: &mut Encoder, candidate: &EssentialCandidate) {
    encoder.put_u64(candidate.candidate_id);
    encoder.put_u8(candidate.source.code());
    for coefficient in candidate.matrix {
        encoder.put_i64(coefficient);
    }
}

fn encode_evaluation(
    encoder: &mut Encoder,
    evaluation: &CandidateEvaluation,
) -> Result<(), EpipolarError> {
    encoder.put_u64(evaluation.candidate_id);
    encoder.put_u64(evaluation.determinant_residual_ppm);
    encoder.put_bool(evaluation.determinant_gate_passed);
    encoder.put_u32(evaluation.inlier_count);
    encoder.put_u32(evaluation.outlier_count);
    encoder.put_u32(evaluation.inlier_ratio_ppm);
    encoder.put_u16(evaluation.left_spatial_bin_count);
    encoder.put_u16(evaluation.right_spatial_bin_count);
    encoder.put_u64(evaluation.median_inlier_residual_ppm);
    encoder.put_u64(evaluation.maximum_inlier_residual_ppm);
    encoder.put_bool(evaluation.passes_admission_gates);
    encode_u64_sequence(encoder, &evaluation.inlier_match_ids)?;
    encode_u64_sequence(encoder, &evaluation.outlier_match_ids)?;
    encoder.put_u64(usize_to_u64(evaluation.residuals.len())?);
    for residual in &evaluation.residuals {
        encoder.put_u64(residual.candidate_id);
        encoder.put_u64(residual.match_id);
        encode_optional_u64(encoder, residual.residual_ppm);
        encoder.put_u64(residual.gate_ppm);
        encoder.put_u8(residual.disposition.code());
    }
    Ok(())
}

fn encode_u64_sequence(
    encoder: &mut Encoder,
    values: &[u64],
) -> Result<(), EpipolarError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
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

fn push_u64_json_array(output: &mut String, values: &[u64]) -> Result<(), EpipolarError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}

fn push_i64_json_array<const N: usize>(
    output: &mut String,
    values: &[i64; N],
) -> Result<(), EpipolarError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}
