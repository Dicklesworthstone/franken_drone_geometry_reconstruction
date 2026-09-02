impl KeyframeSelection {
    /// Replays the complete deterministic selection and compares every retained field.
    ///
    /// # Errors
    ///
    /// Returns a selection failure or derived-field mismatch.
    pub fn validate(&self) -> Result<(), KeyframeError> {
        let rebuilt = select_keyframes(
            self.basis.clone(),
            self.policy,
            self.candidates.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(KeyframeError::DerivedMismatch {
                field: "keyframe_selection",
            })
        }
    }

    /// Returns the deterministic canonical binary representation.
    ///
    /// # Errors
    ///
    /// Returns a validation or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, KeyframeError> {
        self.validate()?;
        let capacity = 1_024_usize
            .saturating_add(self.candidates.len().saturating_mul(192))
            .saturating_add(
                self.candidates
                    .iter()
                    .map(|candidate| candidate.coverage_cells.len().saturating_mul(8))
                    .sum::<usize>(),
            );
        let mut encoder = Encoder::with_capacity(capacity);
        encoder.put_str(KEYFRAME_SELECTION_SCHEMA)?;
        encode_basis(&mut encoder, &self.basis);
        encode_policy(&mut encoder, self.policy);
        encoder.put_u64(usize_to_u64(self.candidates.len())?);
        for candidate in &self.candidates {
            encode_candidate(&mut encoder, candidate)?;
        }
        encoder.put_u64(usize_to_u64(self.selected.len())?);
        for selected in &self.selected {
            encode_selected(&mut encoder, selected)?;
        }
        encoder.put_u64(usize_to_u64(self.rejected.len())?);
        for rejected in &self.rejected {
            encode_rejected(&mut encoder, rejected);
        }
        encode_u64_sequence(&mut encoder, &self.covered_cells)?;
        encode_u32_sequence(&mut encoder, &self.selected_view_sectors)?;
        encode_u32_sequence(&mut encoder, &self.selected_baseline_bins)?;
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated selection identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, KeyframeError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(KEYFRAME_SELECTION_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Renders deterministic field-ordered JSON for agents and public tooling.
    ///
    /// Presentation ticks and total scores are decimal strings so generic JSON clients cannot lose
    /// integer precision. The exact candidate table is named by `candidate_basis_digest`; the
    /// result carries selected and rejected decisions rather than duplicating all metric input.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, KeyframeError> {
        let digest = self.digest()?;
        let hard_rejections = self
            .rejected
            .iter()
            .filter(|rejected| {
                matches!(
                    rejected.reason,
                    KeyframeRejectionReason::SharpnessBelowMinimum
                        | KeyframeRejectionReason::TextureBelowMinimum
                        | KeyframeRejectionReason::DarkClippingExceeded
                        | KeyframeRejectionReason::BrightClippingExceeded
                        | KeyframeRejectionReason::DynamicContentExceeded
                        | KeyframeRejectionReason::OverlapBelowMinimum
                        | KeyframeRejectionReason::EmptyCoverage
                )
            })
            .count();
        let eligible_candidates = self.candidates.len().saturating_sub(hard_rejections);
        let mut output = format!(
            "{{\"schema\":\"{KEYFRAME_SELECTION_SCHEMA}\",\"selection_digest\":\"{digest}\",\"timeline_digest\":\"{}\",\"decoded_frame_generation_digest\":\"{}\",\"calibration_digest\":\"{}\",\"candidate_basis_digest\":\"{}\",\"policy_digest\":\"{}\",\"selection_generation\":{},\"candidate_count\":{},\"eligible_candidate_count\":{},\"selected_count\":{},\"rejected_count\":{},\"covered_cell_count\":{},\"selected_view_sector_count\":{},\"selected_baseline_bin_count\":{},\"policy\":{{\"max_selected\":{},\"min_sharpness_ppm\":{},\"min_texture_ppm\":{},\"max_dark_clipped_ppm\":{},\"max_bright_clipped_ppm\":{},\"max_dynamic_content_ppm\":{},\"min_overlap_ppm\":{},\"min_presentation_spacing_ticks\":{},\"quality_weight\":{},\"coverage_weight\":{},\"view_diversity_weight\":{},\"baseline_diversity_weight\":{}}},\"covered_cells\":[",
            self.basis.timeline_digest,
            self.basis.decoded_frame_generation_digest,
            self.basis.calibration_digest,
            self.basis.candidate_basis_digest,
            self.basis.policy_digest,
            self.basis.selection_generation,
            self.candidates.len(),
            eligible_candidates,
            self.selected.len(),
            self.rejected.len(),
            self.covered_cells.len(),
            self.selected_view_sectors.len(),
            self.selected_baseline_bins.len(),
            self.policy.max_selected,
            self.policy.min_sharpness_ppm,
            self.policy.min_texture_ppm,
            self.policy.max_dark_clipped_ppm,
            self.policy.max_bright_clipped_ppm,
            self.policy.max_dynamic_content_ppm,
            self.policy.min_overlap_ppm,
            self.policy.min_presentation_spacing_ticks,
            self.policy.quality_weight,
            self.policy.coverage_weight,
            self.policy.view_diversity_weight,
            self.policy.baseline_diversity_weight,
        );
        push_u64_json_array(&mut output, &self.covered_cells)?;
        output.push_str("],\"selected_view_sectors\":[");
        push_u32_json_array(&mut output, &self.selected_view_sectors)?;
        output.push_str("],\"selected_baseline_bins\":[");
        push_u32_json_array(&mut output, &self.selected_baseline_bins)?;
        output.push_str("],\"selected\":[");
        for (position, selected) in self.selected.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"rank\":{},\"sample_index\":{},\"frame_digest\":\"{}\",\"presentation_tick\":\"{}\",\"quality_score_ppm\":{},\"marginal_coverage_cells\":{},\"new_view_sector\":{},\"new_baseline_bin\":{},\"total_score\":\"{}\"}}",
                selected.rank,
                selected.sample_index,
                selected.frame_digest,
                selected.presentation_tick,
                selected.quality_score_ppm,
                selected.marginal_coverage_cells,
                selected.new_view_sector,
                selected.new_baseline_bin,
                selected.total_score,
            )
            .map_err(json_rendering)?;
        }
        output.push_str("],\"rejected\":[");
        for (position, rejected) in self.rejected.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"sample_index\":{},\"frame_digest\":\"{}\",\"reason\":\"{}\",\"quality_score_ppm\":{},\"marginal_coverage_cells\":{},\"conflicts_with_sample_index\":{}}}",
                rejected.sample_index,
                rejected.frame_digest,
                rejected.reason.as_str(),
                rejected.quality_score_ppm,
                rejected.marginal_coverage_cells,
                optional_u64_json(rejected.conflicts_with_sample_index),
            )
            .map_err(json_rendering)?;
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_basis(encoder: &mut Encoder, basis: &KeyframeBasis) {
    encoder.put_digest(&basis.timeline_digest);
    encoder.put_digest(&basis.decoded_frame_generation_digest);
    encoder.put_digest(&basis.calibration_digest);
    encoder.put_digest(&basis.candidate_basis_digest);
    encoder.put_digest(&basis.policy_digest);
    encoder.put_u64(basis.selection_generation);
}

fn encode_policy(encoder: &mut Encoder, policy: KeyframePolicy) {
    encoder.put_u32(policy.max_selected);
    encoder.put_u32(policy.min_sharpness_ppm);
    encoder.put_u32(policy.min_texture_ppm);
    encoder.put_u32(policy.max_dark_clipped_ppm);
    encoder.put_u32(policy.max_bright_clipped_ppm);
    encoder.put_u32(policy.max_dynamic_content_ppm);
    encoder.put_u32(policy.min_overlap_ppm);
    encoder.put_u64(policy.min_presentation_spacing_ticks);
    encoder.put_u32(policy.quality_weight);
    encoder.put_u32(policy.coverage_weight);
    encoder.put_u32(policy.view_diversity_weight);
    encoder.put_u32(policy.baseline_diversity_weight);
}

fn encode_candidate(
    encoder: &mut Encoder,
    candidate: &KeyframeCandidate,
) -> Result<(), KeyframeError> {
    encoder.put_u64(candidate.sample_index);
    encoder.put_digest(&candidate.frame_digest);
    encode_i128(encoder, candidate.presentation_tick)?;
    encoder.put_u32(candidate.sharpness_ppm);
    encoder.put_u32(candidate.texture_ppm);
    encoder.put_u32(candidate.dark_clipped_ppm);
    encoder.put_u32(candidate.bright_clipped_ppm);
    encoder.put_u32(candidate.dynamic_content_ppm);
    encoder.put_u32(candidate.overlap_ppm);
    encoder.put_u32(candidate.view_sector);
    encoder.put_u32(candidate.baseline_bin);
    encode_u64_sequence(encoder, &candidate.coverage_cells)
}

fn encode_selected(
    encoder: &mut Encoder,
    selected: &SelectedKeyframe,
) -> Result<(), KeyframeError> {
    encoder.put_u32(selected.rank);
    encoder.put_u64(selected.sample_index);
    encoder.put_digest(&selected.frame_digest);
    encode_i128(encoder, selected.presentation_tick)?;
    encoder.put_u32(selected.quality_score_ppm);
    encoder.put_u32(selected.marginal_coverage_cells);
    encoder.put_bool(selected.new_view_sector);
    encoder.put_bool(selected.new_baseline_bin);
    encode_u128(encoder, selected.total_score)
}

fn encode_rejected(encoder: &mut Encoder, rejected: &RejectedKeyframe) {
    encoder.put_u64(rejected.sample_index);
    encoder.put_digest(&rejected.frame_digest);
    encoder.put_u8(rejected.reason.code());
    encoder.put_u32(rejected.quality_score_ppm);
    encoder.put_u32(rejected.marginal_coverage_cells);
    match rejected.conflicts_with_sample_index {
        Some(sample_index) => {
            encoder.put_bool(true);
            encoder.put_u64(sample_index);
        }
        None => encoder.put_bool(false),
    }
}

fn encode_u64_sequence(
    encoder: &mut Encoder,
    values: &[u64],
) -> Result<(), KeyframeError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn encode_u32_sequence(
    encoder: &mut Encoder,
    values: &[u32],
) -> Result<(), KeyframeError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u32(*value);
    }
    Ok(())
}

fn push_u64_json_array(output: &mut String, values: &[u64]) -> Result<(), KeyframeError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}

fn push_u32_json_array(output: &mut String, values: &[u32]) -> Result<(), KeyframeError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}
