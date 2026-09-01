impl CanonicalSampleTimeline {
    /// Revalidates all timeline invariants and deterministic derived summaries.
    ///
    /// # Errors
    ///
    /// Returns a stable error for identity, basis, order, range, gap, byte-span, summary, or
    /// arithmetic drift.
    pub fn validate(&self) -> Result<(), TimelineError> {
        validate_basis(&self.basis)?;
        validate_count_limits(self.requested_max_samples, self.samples.len())?;
        if self.samples.len() > self.requested_max_samples {
            return Err(TimelineError::ReturnedMoreThanRequested {
                returned: self.samples.len(),
                requested: self.requested_max_samples,
            });
        }
        let coverage = derive_coverage(
            self.start_sample,
            self.samples.len(),
            self.total_samples,
        )?;
        compare_derived("end_sample", self.end_sample, coverage.end_sample)?;
        compare_derived(
            "reaches_track_end",
            self.reaches_track_end,
            coverage.reaches_track_end,
        )?;
        compare_derived(
            "covers_entire_track",
            self.covers_entire_track,
            coverage.covers_entire_track,
        )?;
        compare_derived(
            "prefix_unrepresented_samples",
            self.prefix_unrepresented_samples,
            coverage.prefix_unrepresented_samples,
        )?;
        compare_derived(
            "suffix_unrepresented_samples",
            self.suffix_unrepresented_samples,
            coverage.suffix_unrepresented_samples,
        )?;
        let rebuilt = build_parts(
            self.start_sample,
            self.basis.source_object_length,
            self.samples.iter().map(sample_as_record),
        )?;
        compare_derived("samples", &self.samples, &rebuilt.samples)?;
        compare_derived("gaps", &self.gaps, &rebuilt.gaps)?;
        compare_derived(
            "total_gap_duration",
            self.total_gap_duration,
            rebuilt.total_gap_duration,
        )?;
        compare_derived(
            "sync_sample_count",
            self.sync_sample_count,
            rebuilt.sync_sample_count,
        )?;
        compare_derived(
            "sample_description_indices",
            &self.sample_description_indices,
            &rebuilt.sample_description_indices,
        )?;
        compare_derived(
            "source_byte_order_reordered",
            self.source_byte_order_reordered,
            rebuilt.source_byte_order_reordered,
        )?;
        compare_derived(
            "presentation_reordered",
            self.presentation_reordered,
            rebuilt.presentation_reordered,
        )?;
        compare_derived(
            "has_negative_presentation_time",
            self.has_negative_presentation_time,
            rebuilt.has_negative_presentation_time,
        )?;
        compare_derived("decode_start", self.decode_start, rebuilt.decode_start)?;
        compare_derived("decode_end", self.decode_end, rebuilt.decode_end)?;
        compare_derived(
            "presentation_start",
            self.presentation_start,
            rebuilt.presentation_start,
        )?;
        compare_derived(
            "presentation_end",
            self.presentation_end,
            rebuilt.presentation_end,
        )?;
        Ok(())
    }

    /// Returns the deterministic canonical binary representation.
    ///
    /// # Errors
    ///
    /// Returns a timeline validation or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, TimelineError> {
        self.validate()?;
        let capacity = 768_usize.saturating_add(self.samples.len().saturating_mul(128));
        let mut encoder = Encoder::with_capacity(capacity);
        encoder.put_str(MEDIA_TIMELINE_SCHEMA)?;
        encode_basis(&mut encoder, &self.basis);
        encoder.put_u64(self.total_samples);
        encoder.put_u64(self.start_sample);
        encoder.put_u64(self.end_sample);
        encoder.put_bool(self.reaches_track_end);
        encoder.put_bool(self.covers_entire_track);
        encoder.put_u64(self.prefix_unrepresented_samples);
        encoder.put_u64(self.suffix_unrepresented_samples);
        encoder.put_u64(self.total_gap_duration);
        encoder.put_u64(self.sync_sample_count);
        encoder.put_bool(self.source_byte_order_reordered);
        encoder.put_bool(self.presentation_reordered);
        encoder.put_bool(self.has_negative_presentation_time);
        encode_optional_u64(&mut encoder, self.decode_start);
        encode_optional_u64(&mut encoder, self.decode_end);
        encode_optional_i128(&mut encoder, self.presentation_start)?;
        encode_optional_i128(&mut encoder, self.presentation_end)?;
        encoder.put_u64(usize_to_u64(self.sample_description_indices.len())?);
        for index in &self.sample_description_indices {
            encoder.put_u32(*index);
        }
        encoder.put_u64(usize_to_u64(self.samples.len())?);
        for sample in &self.samples {
            encode_sample(&mut encoder, sample)?;
        }
        encoder.put_u64(usize_to_u64(self.gaps.len())?);
        for gap in &self.gaps {
            encode_gap(&mut encoder, gap);
        }
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated timeline identity.
    ///
    /// # Errors
    ///
    /// Returns a timeline validation, domain, canonical encoding, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, TimelineError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(MEDIA_TIMELINE_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Renders deterministic field-ordered JSON for agent and CLI surfaces.
    ///
    /// Presentation-domain `i128` values are encoded as decimal strings so consumers never lose
    /// integer precision through an IEEE-754 JSON implementation.
    ///
    /// # Errors
    ///
    /// Returns the same validation and identity errors as [`Self::digest`], or a formatting error.
    pub fn to_json(&self) -> Result<String, TimelineError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{MEDIA_TIMELINE_SCHEMA}\",\"timeline_digest\":\"{digest}\",\"recorded_media_root_manifest_digest\":\"{}\",\"source_manifest_digest\":\"{}\",\"source_object_digest\":\"{}\",\"source_object_length\":{},\"track_id\":{},\"timescale\":{},\"total_samples\":{},\"start_sample\":{},\"end_sample\":{},\"requested_max_samples\":{},\"returned_samples\":{},\"reaches_track_end\":{},\"covers_entire_track\":{},\"prefix_unrepresented_samples\":{},\"suffix_unrepresented_samples\":{},\"index_entries_scanned\":{},\"total_gap_duration\":{},\"sync_sample_count\":{},\"source_byte_order_reordered\":{},\"presentation_reordered\":{},\"has_negative_presentation_time\":{},\"decode_start\":{},\"decode_end\":{},\"presentation_start_ticks\":{},\"presentation_end_ticks\":{},\"sample_description_indices\":[",
            self.basis.recorded_media_root_manifest_digest,
            self.basis.source_manifest_digest,
            self.basis.source_object_digest,
            self.basis.source_object_length,
            self.basis.track_id,
            self.basis.timescale,
            self.total_samples,
            self.start_sample,
            self.end_sample,
            self.requested_max_samples,
            self.samples.len(),
            self.reaches_track_end,
            self.covers_entire_track,
            self.prefix_unrepresented_samples,
            self.suffix_unrepresented_samples,
            self.index_entries_scanned,
            self.total_gap_duration,
            self.sync_sample_count,
            self.source_byte_order_reordered,
            self.presentation_reordered,
            self.has_negative_presentation_time,
            optional_u64_json(self.decode_start),
            optional_u64_json(self.decode_end),
            optional_i128_json(self.presentation_start),
            optional_i128_json(self.presentation_end),
        );
        for (position, index) in self.sample_description_indices.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(output, "{index}").map_err(json_rendering)?;
        }
        output.push_str("],\"gaps\":[");
        for (position, gap) in self.gaps.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"after_sample_index\":{},\"before_sample_index\":{},\"start_decode_time\":{},\"end_decode_time\":{},\"duration\":{}}}",
                gap.after_sample_index,
                gap.before_sample_index,
                gap.start_decode_time,
                gap.end_decode_time,
                gap.duration,
            )
            .map_err(json_rendering)?;
        }
        output.push_str("],\"samples\":[");
        for (position, sample) in self.samples.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"sample_index\":{},\"decode_time\":{},\"presentation_time_ticks\":\"{}\",\"composition_offset_ticks\":\"{}\",\"duration\":{},\"decode_end\":{},\"presentation_end_ticks\":\"{}\",\"byte_offset\":{},\"byte_end\":{},\"byte_length\":{},\"is_sync\":{},\"sample_description_index\":{}}}",
                sample.sample_index,
                sample.decode_time,
                sample.presentation_time,
                sample.composition_offset,
                sample.duration,
                sample.decode_end,
                sample.presentation_end,
                sample.byte_offset,
                sample.byte_end,
                sample.byte_length,
                sample.is_sync,
                sample.sample_description_index,
            )
            .map_err(json_rendering)?;
        }
        output.push_str("]}");
        Ok(output)
    }
}
