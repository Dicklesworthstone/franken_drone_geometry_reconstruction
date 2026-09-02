impl CorrespondenceGeneration {
    /// Replays matching and track assembly and compares every retained field.
    ///
    /// # Errors
    ///
    /// Returns a matching/track failure or derived-field mismatch.
    pub fn validate(&self) -> Result<(), CorrespondenceError> {
        let rebuilt = build_correspondences(
            self.basis.clone(),
            self.policy,
            self.observations.clone(),
            self.pairs.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(CorrespondenceError::DerivedMismatch {
                field: "correspondence_generation",
            })
        }
    }

    /// Returns deterministic canonical binary bytes.
    ///
    /// # Errors
    ///
    /// Returns a validation or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, CorrespondenceError> {
        self.validate()?;
        let capacity = 1_024_usize
            .saturating_add(self.observations.len().saturating_mul(160))
            .saturating_add(self.accepted_matches.len().saturating_mul(64))
            .saturating_add(self.rejected_matches.len().saturating_mul(48));
        let mut encoder = Encoder::with_capacity(capacity);
        encoder.put_str(CORRESPONDENCE_GENERATION_SCHEMA)?;
        encode_basis(&mut encoder, &self.basis);
        encode_policy(&mut encoder, self.policy);
        encoder.put_u64(usize_to_u64(self.observations.len())?);
        for observation in &self.observations {
            encode_observation(&mut encoder, observation);
        }
        encoder.put_u64(usize_to_u64(self.pairs.len())?);
        for pair in &self.pairs {
            encode_pair(&mut encoder, *pair);
        }
        encoder.put_u64(usize_to_u64(self.accepted_matches.len())?);
        for edge in &self.accepted_matches {
            encode_match(&mut encoder, edge);
        }
        encoder.put_u64(usize_to_u64(self.rejected_matches.len())?);
        for rejection in &self.rejected_matches {
            encode_rejection(&mut encoder, rejection);
        }
        encoder.put_u64(usize_to_u64(self.tracks.len())?);
        for track in &self.tracks {
            encode_track(&mut encoder, track)?;
        }
        encode_u64_sequence(&mut encoder, &self.ineligible_observation_ids)?;
        encode_u64_sequence(&mut encoder, &self.unmatched_observation_ids)?;
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated generation identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, CorrespondenceError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(CORRESPONDENCE_GENERATION_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Renders deterministic field-ordered JSON for agents and public tooling.
    ///
    /// The exact source feature and pair tables are named by content digest rather than duplicated.
    /// All numeric identifiers are constrained to the lossless JSON integer range.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, CorrespondenceError> {
        let digest = self.digest()?;
        let eligible_count = self
            .observations
            .len()
            .saturating_sub(self.ineligible_observation_ids.len());
        let mut output = format!(
            "{{\"schema\":\"{CORRESPONDENCE_GENERATION_SCHEMA}\",\"generation_digest\":\"{digest}\",\"keyframe_selection_digest\":\"{}\",\"calibration_digest\":\"{}\",\"feature_basis_digest\":\"{}\",\"pair_basis_digest\":\"{}\",\"policy_digest\":\"{}\",\"generation\":{},\"observation_count\":{},\"eligible_observation_count\":{},\"pair_count\":{},\"accepted_match_count\":{},\"rejected_match_count\":{},\"track_count\":{},\"unmatched_observation_count\":{},\"distance_evaluations\":{},\"policy\":{{\"max_hamming_distance\":{},\"ratio_threshold_ppm\":{},\"require_second_best\":{},\"require_mutual\":{},\"min_response_ppm\":{},\"max_uncertainty_nano_pixels\":{},\"reject_dynamic_masked\":{},\"max_distance_evaluations\":{}}},\"ineligible_observation_ids\":[",
            self.basis.keyframe_selection_digest,
            self.basis.calibration_digest,
            self.basis.feature_basis_digest,
            self.basis.pair_basis_digest,
            self.basis.policy_digest,
            self.basis.generation,
            self.observations.len(),
            eligible_count,
            self.pairs.len(),
            self.accepted_matches.len(),
            self.rejected_matches.len(),
            self.tracks.len(),
            self.unmatched_observation_ids.len(),
            self.distance_evaluations,
            self.policy.max_hamming_distance,
            self.policy.ratio_threshold_ppm,
            self.policy.require_second_best,
            self.policy.require_mutual,
            self.policy.min_response_ppm,
            self.policy.max_uncertainty_nano_pixels,
            self.policy.reject_dynamic_masked,
            self.policy.max_distance_evaluations,
        );
        push_u64_json_array(&mut output, &self.ineligible_observation_ids)?;
        output.push_str("],\"unmatched_observation_ids\":[");
        push_u64_json_array(&mut output, &self.unmatched_observation_ids)?;
        output.push_str("],\"accepted_matches\":[");
        for (position, edge) in self.accepted_matches.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"pair_id\":{},\"left_observation_id\":{},\"right_observation_id\":{},\"hamming_distance\":{},\"second_best_distance\":{},\"ratio_ppm\":{},\"mutual\":{},\"creates_cycle\":{}}}",
                edge.pair_id,
                edge.left_observation_id,
                edge.right_observation_id,
                edge.hamming_distance,
                optional_u16_json(edge.second_best_distance),
                optional_u32_json(edge.ratio_ppm),
                edge.mutual,
                edge.creates_cycle,
            )
            .map_err(json_rendering)?;
        }
        output.push_str("],\"rejected_matches\":[");
        for (position, rejection) in self.rejected_matches.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"pair_id\":{},\"left_observation_id\":{},\"candidate_right_observation_id\":{},\"reason\":\"{}\",\"best_distance\":{},\"second_best_distance\":{}}}",
                rejection.pair_id,
                rejection.left_observation_id,
                optional_u64_json(rejection.candidate_right_observation_id),
                rejection.reason.as_str(),
                optional_u16_json(rejection.best_distance),
                optional_u16_json(rejection.second_best_distance),
            )
            .map_err(json_rendering)?;
        }
        output.push_str("],\"tracks\":[");
        for (position, track) in self.tracks.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"track_id\":{},\"edge_count\":{},\"observation_ids\":[",
                track.track_id,
                track.edge_count,
            )
            .map_err(json_rendering)?;
            push_u64_json_array(&mut output, &track.observation_ids)?;
            output.push_str("],\"sample_indices\":[");
            push_u64_json_array(&mut output, &track.sample_indices)?;
            output.push_str("],\"supporting_pair_ids\":[");
            push_u64_json_array(&mut output, &track.supporting_pair_ids)?;
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_basis(encoder: &mut Encoder, basis: &CorrespondenceBasis) {
    encoder.put_digest(&basis.keyframe_selection_digest);
    encoder.put_digest(&basis.calibration_digest);
    encoder.put_digest(&basis.feature_basis_digest);
    encoder.put_digest(&basis.pair_basis_digest);
    encoder.put_digest(&basis.policy_digest);
    encoder.put_u64(basis.generation);
}

fn encode_policy(encoder: &mut Encoder, policy: MatchPolicy) {
    encoder.put_u16(policy.max_hamming_distance);
    encoder.put_u32(policy.ratio_threshold_ppm);
    encoder.put_bool(policy.require_second_best);
    encoder.put_bool(policy.require_mutual);
    encoder.put_u32(policy.min_response_ppm);
    encoder.put_u64(policy.max_uncertainty_nano_pixels);
    encoder.put_bool(policy.reject_dynamic_masked);
}

fn encode_observation(encoder: &mut Encoder, observation: &FeatureObservation) {
    encoder.put_u64(observation.observation_id);
    encoder.put_u64(observation.sample_index);
    encoder.put_digest(&observation.frame_digest);
    encoder.put_u32(observation.feature_id);
    encoder.put_i64(observation.x_nano_pixels);
    encoder.put_i64(observation.y_nano_pixels);
    encoder.put_u32(observation.response_ppm);
    encoder.put_u64(observation.uncertainty_nano_pixels);
    for word in observation.descriptor.0 {
        encoder.put_u64(word);
    }
    encoder.put_bool(observation.dynamic_masked);
}

fn encode_pair(encoder: &mut Encoder, pair: FramePair) {
    encoder.put_u64(pair.pair_id);
    encoder.put_u64(pair.left_sample_index);
    encoder.put_u64(pair.right_sample_index);
}

fn encode_match(encoder: &mut Encoder, edge: &PairwiseMatch) {
    encoder.put_u64(edge.pair_id);
    encoder.put_u64(edge.left_observation_id);
    encoder.put_u64(edge.right_observation_id);
    encoder.put_u16(edge.hamming_distance);
    encode_optional_u16(encoder, edge.second_best_distance);
    encode_optional_u32(encoder, edge.ratio_ppm);
    encoder.put_bool(edge.mutual);
    encoder.put_bool(edge.creates_cycle);
}

fn encode_rejection(encoder: &mut Encoder, rejection: &MatchRejection) {
    encoder.put_u64(rejection.pair_id);
    encoder.put_u64(rejection.left_observation_id);
    encode_optional_u64(encoder, rejection.candidate_right_observation_id);
    encoder.put_u8(rejection.reason.code());
    encode_optional_u16(encoder, rejection.best_distance);
    encode_optional_u16(encoder, rejection.second_best_distance);
}

fn encode_track(
    encoder: &mut Encoder,
    track: &FeatureTrack,
) -> Result<(), CorrespondenceError> {
    encoder.put_u64(track.track_id);
    encoder.put_u32(track.edge_count);
    encode_u64_sequence(encoder, &track.observation_ids)?;
    encode_u64_sequence(encoder, &track.sample_indices)?;
    encode_u64_sequence(encoder, &track.supporting_pair_ids)
}

fn encode_u64_sequence(
    encoder: &mut Encoder,
    values: &[u64],
) -> Result<(), CorrespondenceError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn push_u64_json_array(
    output: &mut String,
    values: &[u64],
) -> Result<(), CorrespondenceError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}
