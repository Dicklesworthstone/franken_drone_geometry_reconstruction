fn validate_nonoverlapping_byte_ranges(
    ranges: &[(u64, u64, u64)],
) -> Result<(), TimelineError> {
    let mut sorted = ranges.to_vec();
    sorted.sort_unstable_by_key(|(start, end, sample_index)| (*start, *end, *sample_index));
    for pair in sorted.windows(2) {
        let [previous, current] = pair else {
            continue;
        };
        if current.0 < previous.1 {
            return Err(TimelineError::OverlappingByteIntervals {
                previous_sample_index: previous.2,
                previous_byte_end: previous.1,
                sample_index: current.2,
                byte_offset: current.0,
            });
        }
    }
    Ok(())
}

fn sample_as_record(sample: &TimelineSample) -> SampleRecord {
    SampleRecord {
        sample_index: sample.sample_index,
        decode_time: sample.decode_time,
        composition_time: sample.presentation_time,
        duration: sample.duration,
        byte_offset: sample.byte_offset,
        byte_length: sample.byte_length,
        is_sync: sample.is_sync,
        sample_description_index: sample.sample_description_index,
    }
}

fn validate_basis(basis: &TimelineBasis) -> Result<(), TimelineError> {
    reject_zero_digest(
        "recorded_media_root_manifest_digest",
        &basis.recorded_media_root_manifest_digest,
    )?;
    reject_zero_digest("source_manifest_digest", &basis.source_manifest_digest)?;
    reject_zero_digest("source_object_digest", &basis.source_object_digest)?;
    if basis.source_object_length == 0 {
        return Err(TimelineError::ZeroValue {
            field: "source_object_length",
        });
    }
    if basis.track_id == 0 {
        return Err(TimelineError::ZeroValue { field: "track_id" });
    }
    if basis.timescale == 0 {
        return Err(TimelineError::ZeroValue { field: "timescale" });
    }
    Ok(())
}

fn validate_count_limits(requested: usize, returned: usize) -> Result<(), TimelineError> {
    if requested == 0 {
        return Err(TimelineError::ZeroValue {
            field: "requested_max_samples",
        });
    }
    if requested > MAX_TIMELINE_SAMPLES {
        return Err(TimelineError::RequestLimitExceeded {
            actual: requested,
            maximum: MAX_TIMELINE_SAMPLES,
        });
    }
    if returned > MAX_TIMELINE_SAMPLES {
        return Err(TimelineError::SampleLimitExceeded {
            actual: returned,
            maximum: MAX_TIMELINE_SAMPLES,
        });
    }
    Ok(())
}

fn reject_zero_digest(field: &'static str, digest: &EvidenceDigest) -> Result<(), TimelineError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(TimelineError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn derive_coverage(
    start_sample: u64,
    returned: usize,
    total_samples: u64,
) -> Result<CoverageSummary, TimelineError> {
    if start_sample > total_samples {
        return Err(TimelineError::StartBeyondTrack {
            start_sample,
            total_samples,
        });
    }
    let returned = usize_to_u64(returned)?;
    let end_sample = start_sample
        .checked_add(returned)
        .ok_or(TimelineError::ArithmeticOverflow {
            field: "sample_window_end",
        })?;
    if end_sample > total_samples {
        return Err(TimelineError::WindowBeyondTrack {
            end_sample,
            total_samples,
        });
    }
    let suffix = total_samples.saturating_sub(end_sample);
    Ok(CoverageSummary {
        end_sample,
        reaches_track_end: suffix == 0,
        covers_entire_track: start_sample == 0 && suffix == 0,
        prefix_unrepresented_samples: start_sample,
        suffix_unrepresented_samples: suffix,
    })
}

fn compare_derived<T: PartialEq>(
    field: &'static str,
    observed: T,
    expected: T,
) -> Result<(), TimelineError> {
    if observed == expected {
        Ok(())
    } else {
        Err(TimelineError::DerivedSummaryMismatch { field })
    }
}

fn encode_basis(encoder: &mut Encoder, basis: &TimelineBasis) {
    encoder.put_digest(&basis.recorded_media_root_manifest_digest);
    encoder.put_digest(&basis.source_manifest_digest);
    encoder.put_digest(&basis.source_object_digest);
    encoder.put_u64(basis.source_object_length);
    encoder.put_u32(basis.track_id);
    encoder.put_u32(basis.timescale);
}

fn encode_sample(encoder: &mut Encoder, sample: &TimelineSample) -> Result<(), CodecError> {
    encoder.put_u64(sample.sample_index);
    encoder.put_u64(sample.decode_time);
    encoder.put_bytes(&sample.presentation_time.to_be_bytes())?;
    encoder.put_bytes(&sample.composition_offset.to_be_bytes())?;
    encoder.put_u32(sample.duration);
    encoder.put_u64(sample.decode_end);
    encoder.put_bytes(&sample.presentation_end.to_be_bytes())?;
    encoder.put_u64(sample.byte_offset);
    encoder.put_u64(sample.byte_end);
    encoder.put_u32(sample.byte_length);
    encoder.put_bool(sample.is_sync);
    encoder.put_u32(sample.sample_description_index);
    Ok(())
}

fn encode_gap(encoder: &mut Encoder, gap: &TimelineGap) {
    encoder.put_u64(gap.after_sample_index);
    encoder.put_u64(gap.before_sample_index);
    encoder.put_u64(gap.start_decode_time);
    encoder.put_u64(gap.end_decode_time);
    encoder.put_u64(gap.duration);
}

fn encode_optional_u64(encoder: &mut Encoder, value: Option<u64>) {
    encoder.put_bool(value.is_some());
    encoder.put_u64(value.unwrap_or_default());
}

fn encode_optional_i128(encoder: &mut Encoder, value: Option<i128>) -> Result<(), CodecError> {
    encoder.put_bool(value.is_some());
    encoder.put_bytes(&value.unwrap_or_default().to_be_bytes())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn optional_i128_json(value: Option<i128>) -> String {
    value.map_or_else(
        || "null".to_owned(),
        |number| format!("\"{number}\""),
    )
}

fn json_rendering(error: fmt::Error) -> TimelineError {
    TimelineError::JsonRendering(error.to_string())
}

fn usize_to_u64(value: usize) -> Result<u64, TimelineError> {
    u64::try_from(value).map_err(|_| TimelineError::LengthOverflow)
}
