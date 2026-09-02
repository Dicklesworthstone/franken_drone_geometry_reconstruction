impl RelativePoseVerification {
    /// Replays candidate evaluation and compares every retained field.
    ///
    /// # Errors
    ///
    /// Returns a validation/evaluation failure or deterministic mismatch.
    pub fn validate(&self) -> Result<(), RelativePoseError> {
        let rebuilt = verify_relative_pose(
            self.basis.clone(),
            self.policy,
            self.matches.clone(),
            self.candidates.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(RelativePoseError::DerivedMismatch {
                field: "relative_pose_verification",
            })
        }
    }

    /// Returns deterministic canonical bytes.
    ///
    /// Operation ceilings and the observed evaluation count are excluded after successful
    /// completion, allowing semantically equivalent optimized implementations to retain identity.
    ///
    /// # Errors
    ///
    /// Returns a validation or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, RelativePoseError> {
        self.validate()?;
        let capacity = 1_024_usize
            .saturating_add(self.matches.len().saturating_mul(160))
            .saturating_add(self.candidates.len().saturating_mul(192))
            .saturating_add(
                self.evaluations
                    .iter()
                    .map(|value| value.assessments.len().saturating_mul(48))
                    .sum::<usize>(),
            );
        let mut encoder = Encoder::with_capacity(capacity);
        encoder.put_str(RELATIVE_POSE_VERIFICATION_SCHEMA)?;
        encode_basis(&mut encoder, &self.basis);
        encode_semantic_policy(&mut encoder, self.policy);
        encoder.put_u64(usize_to_u64(self.matches.len())?);
        for value in &self.matches {
            encode_match(&mut encoder, value);
        }
        encoder.put_u64(usize_to_u64(self.candidates.len())?);
        for value in &self.candidates {
            encode_candidate(&mut encoder, value);
        }
        encoder.put_u64(usize_to_u64(self.evaluations.len())?);
        for value in &self.evaluations {
            encode_evaluation(&mut encoder, value)?;
        }
        encoder.put_u8(self.status.code());
        encode_optional_u64(&mut encoder, self.selected_candidate_id);
        encode_u64_sequence(&mut encoder, &self.ambiguous_candidate_ids)?;
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated verification identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, RelativePoseError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(RELATIVE_POSE_VERIFICATION_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Returns the uniquely selected candidate, if geometric verification resolved one.
    #[must_use]
    pub fn selected_candidate(&self) -> Option<&RelativePoseCandidate> {
        let selected = self.selected_candidate_id?;
        self.candidates
            .iter()
            .find(|candidate| candidate.candidate_id == selected)
    }

    /// Renders deterministic field-ordered JSON.
    ///
    /// Candidate transforms and calibrated bearing tables remain named by exact basis digests;
    /// evaluations retain the decision-relevant residual, parallax, depth, and rejection evidence.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, RelativePoseError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{RELATIVE_POSE_VERIFICATION_SCHEMA}\",\"verification_digest\":\"{digest}\",\"correspondence_generation_digest\":\"{}\",\"calibration_digest\":\"{}\",\"bearing_basis_digest\":\"{}\",\"candidate_basis_digest\":\"{}\",\"policy_digest\":\"{}\",\"left_sample_index\":{},\"right_sample_index\":{},\"generation\":{},\"status\":\"{}\",\"selected_candidate_id\":{},\"ambiguous_candidate_ids\":[",
            self.basis.correspondence_generation_digest,
            self.basis.calibration_digest,
            self.basis.bearing_basis_digest,
            self.basis.candidate_basis_digest,
            self.basis.policy_digest,
            self.basis.left_sample_index,
            self.basis.right_sample_index,
            self.basis.generation,
            self.status.as_str(),
            optional_u64_json(self.selected_candidate_id),
        );
        push_u64_json_array(&mut output, &self.ambiguous_candidate_ids)?;
        write!(
            output,
            "],\"match_count\":{},\"candidate_count\":{},\"evaluation_count\":{},\"policy\":{{\"max_epipolar_residual_nano\":{},\"min_epipolar_normal_nano\":{},\"min_parallax_nano\":{},\"min_inlier_matches\":{},\"min_inlier_ratio_ppm\":{},\"min_positive_depth_ratio_ppm\":{},\"max_median_residual_nano\":{},\"require_cheirality\":{},\"max_evaluations\":{}}},\"evaluations\":[",
            self.matches.len(),
            self.candidates.len(),
            self.evaluation_count,
            self.policy.max_epipolar_residual_nano,
            self.policy.min_epipolar_normal_nano,
            self.policy.min_parallax_nano,
            self.policy.min_inlier_matches,
            self.policy.min_inlier_ratio_ppm,
            self.policy.min_positive_depth_ratio_ppm,
            self.policy.max_median_residual_nano,
            self.policy.require_cheirality,
            self.policy.max_evaluations,
        )
        .map_err(json_rendering)?;
        for (position, evaluation) in self.evaluations.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"candidate_id\":{},\"evidence_digest\":\"{}\",\"source\":\"{}\",\"match_count\":{},\"inlier_count\":{},\"inlier_ratio_ppm\":{},\"positive_depth_count\":{},\"positive_depth_ratio_ppm\":{},\"median_inlier_residual_nano\":{},\"maximum_inlier_residual_nano\":{},\"median_inlier_parallax_nano\":{},\"accepted\":{},\"assessments\":[",
                evaluation.candidate_id,
                evaluation.evidence_digest,
                evaluation.source.as_str(),
                evaluation.match_count,
                evaluation.inlier_count,
                evaluation.inlier_ratio_ppm,
                evaluation.positive_depth_count,
                evaluation.positive_depth_ratio_ppm,
                evaluation.median_inlier_residual_nano,
                evaluation.maximum_inlier_residual_nano,
                evaluation.median_inlier_parallax_nano,
                evaluation.accepted,
            )
            .map_err(json_rendering)?;
            for (assessment_position, assessment) in evaluation.assessments.iter().enumerate() {
                if assessment_position > 0 {
                    output.push(',');
                }
                write!(
                    output,
                    "{{\"match_id\":{},\"epipolar_residual_nano\":{},\"parallax_nano\":{},\"left_depth_positive\":{},\"right_depth_positive\":{},\"inlier\":{},\"rejection_reason\":{}}}",
                    assessment.match_id,
                    assessment.epipolar_residual_nano,
                    assessment.parallax_nano,
                    assessment.left_depth_positive,
                    assessment.right_depth_positive,
                    assessment.inlier,
                    optional_rejection_json(assessment.rejection_reason),
                )
                .map_err(json_rendering)?;
            }
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_basis(encoder: &mut Encoder, basis: &RelativePoseBasis) {
    encoder.put_digest(&basis.correspondence_generation_digest);
    encoder.put_digest(&basis.calibration_digest);
    encoder.put_digest(&basis.bearing_basis_digest);
    encoder.put_digest(&basis.candidate_basis_digest);
    encoder.put_digest(&basis.policy_digest);
    encoder.put_u64(basis.left_sample_index);
    encoder.put_u64(basis.right_sample_index);
    encoder.put_u64(basis.generation);
}

fn encode_semantic_policy(encoder: &mut Encoder, policy: RelativePosePolicy) {
    encoder.put_u64(policy.max_epipolar_residual_nano);
    encoder.put_u64(policy.min_epipolar_normal_nano);
    encoder.put_u64(policy.min_parallax_nano);
    encoder.put_u32(policy.min_inlier_matches);
    encoder.put_u32(policy.min_inlier_ratio_ppm);
    encoder.put_u32(policy.min_positive_depth_ratio_ppm);
    encoder.put_u64(policy.max_median_residual_nano);
    encoder.put_bool(policy.require_cheirality);
}

fn encode_match(encoder: &mut Encoder, value: &BearingMatch) {
    encoder.put_u64(value.match_id);
    encoder.put_u64(value.left_observation_id);
    encoder.put_u64(value.right_observation_id);
    for component in value.left_bearing_nano {
        encoder.put_i64(component);
    }
    for component in value.right_bearing_nano {
        encoder.put_i64(component);
    }
    encoder.put_u64(value.uncertainty_nano);
}

fn encode_candidate(encoder: &mut Encoder, value: &RelativePoseCandidate) {
    encoder.put_u64(value.candidate_id);
    encoder.put_digest(&value.evidence_digest);
    encoder.put_u8(value.source.code());
    for component in value.rotation_left_to_right_nano {
        encoder.put_i64(component);
    }
    for component in value.translation_left_origin_in_right_nano {
        encoder.put_i64(component);
    }
}

fn encode_evaluation(
    encoder: &mut Encoder,
    value: &PoseCandidateEvaluation,
) -> Result<(), RelativePoseError> {
    encoder.put_u64(value.candidate_id);
    encoder.put_digest(&value.evidence_digest);
    encoder.put_u8(value.source.code());
    encoder.put_u32(value.match_count);
    encoder.put_u32(value.inlier_count);
    encoder.put_u32(value.inlier_ratio_ppm);
    encoder.put_u32(value.positive_depth_count);
    encoder.put_u32(value.positive_depth_ratio_ppm);
    encoder.put_u64(value.median_inlier_residual_nano);
    encoder.put_u64(value.maximum_inlier_residual_nano);
    encoder.put_u64(value.median_inlier_parallax_nano);
    encoder.put_bool(value.accepted);
    encoder.put_u64(usize_to_u64(value.assessments.len())?);
    for assessment in &value.assessments {
        encoder.put_u64(assessment.match_id);
        encoder.put_u64(assessment.epipolar_residual_nano);
        encoder.put_u64(assessment.parallax_nano);
        encoder.put_bool(assessment.left_depth_positive);
        encoder.put_bool(assessment.right_depth_positive);
        encoder.put_bool(assessment.inlier);
        match assessment.rejection_reason {
            Some(reason) => {
                encoder.put_bool(true);
                encoder.put_u8(reason.code());
            }
            None => encoder.put_bool(false),
        }
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

fn encode_u64_sequence(
    encoder: &mut Encoder,
    values: &[u64],
) -> Result<(), RelativePoseError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn usize_to_u64(value: usize) -> Result<u64, RelativePoseError> {
    u64::try_from(value).map_err(|_| RelativePoseError::ArithmeticOverflow {
        field: "collection_length",
    })
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn optional_rejection_json(value: Option<PoseMatchRejectionReason>) -> String {
    value.map_or_else(
        || "null".to_owned(),
        |reason| format!("\"{}\"", reason.as_str()),
    )
}

fn push_u64_json_array(
    output: &mut String,
    values: &[u64],
) -> Result<(), RelativePoseError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}

fn json_rendering(error: std::fmt::Error) -> RelativePoseError {
    RelativePoseError::JsonRendering(error.to_string())
}
