/// Builds one canonical timeline from an exact validated classic-sample window.
///
/// # Errors
///
/// Returns a stable error for mixed basis, invalid identity, malformed sample order, overlapping
/// decode or byte intervals, arithmetic overflow, or inconsistent window coverage.
pub fn build_sample_timeline(
    basis: TimelineBasis,
    window: &TrackSampleWindow,
) -> Result<CanonicalSampleTimeline, TimelineError> {
    validate_basis(&basis)?;
    if basis.track_id != window.track_id {
        return Err(TimelineError::TrackMismatch {
            expected: basis.track_id,
            observed: window.track_id,
        });
    }
    if basis.timescale != window.timescale {
        return Err(TimelineError::TimescaleMismatch {
            expected: basis.timescale,
            observed: window.timescale,
        });
    }
    validate_count_limits(window.requested_max_samples, window.samples.len())?;
    if window.samples.len() > window.requested_max_samples {
        return Err(TimelineError::ReturnedMoreThanRequested {
            returned: window.samples.len(),
            requested: window.requested_max_samples,
        });
    }
    let coverage = derive_coverage(
        window.start_sample,
        window.samples.len(),
        window.total_samples,
    )?;
    if window.complete != coverage.reaches_track_end {
        return Err(TimelineError::CompleteFlagMismatch {
            expected: coverage.reaches_track_end,
            observed: window.complete,
        });
    }
    let parts = build_parts(
        window.start_sample,
        basis.source_object_length,
        window.samples.iter().cloned(),
    )?;
    Ok(CanonicalSampleTimeline {
        basis,
        total_samples: window.total_samples,
        start_sample: window.start_sample,
        end_sample: coverage.end_sample,
        requested_max_samples: window.requested_max_samples,
        reaches_track_end: coverage.reaches_track_end,
        covers_entire_track: coverage.covers_entire_track,
        prefix_unrepresented_samples: coverage.prefix_unrepresented_samples,
        suffix_unrepresented_samples: coverage.suffix_unrepresented_samples,
        index_entries_scanned: window.index_entries_scanned,
        samples: parts.samples,
        gaps: parts.gaps,
        total_gap_duration: parts.total_gap_duration,
        sync_sample_count: parts.sync_sample_count,
        sample_description_indices: parts.sample_description_indices,
        source_byte_order_reordered: parts.source_byte_order_reordered,
        presentation_reordered: parts.presentation_reordered,
        has_negative_presentation_time: parts.has_negative_presentation_time,
        decode_start: parts.decode_start,
        decode_end: parts.decode_end,
        presentation_start: parts.presentation_start,
        presentation_end: parts.presentation_end,
    })
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct CoverageSummary {
    end_sample: u64,
    reaches_track_end: bool,
    covers_entire_track: bool,
    prefix_unrepresented_samples: u64,
    suffix_unrepresented_samples: u64,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct TimelineParts {
    samples: Vec<TimelineSample>,
    gaps: Vec<TimelineGap>,
    total_gap_duration: u64,
    sync_sample_count: u64,
    sample_description_indices: Vec<u32>,
    source_byte_order_reordered: bool,
    presentation_reordered: bool,
    has_negative_presentation_time: bool,
    decode_start: Option<u64>,
    decode_end: Option<u64>,
    presentation_start: Option<i128>,
    presentation_end: Option<i128>,
}

fn build_parts(
    expected_start_sample: u64,
    source_object_length: u64,
    samples: impl IntoIterator<Item = SampleRecord>,
) -> Result<TimelineParts, TimelineError> {
    let mut output: Vec<TimelineSample> = Vec::new();
    let mut gaps = Vec::new();
    let mut byte_ranges = Vec::new();
    let mut description_indices = BTreeSet::new();
    let mut previous_decode_end = None;
    let mut previous_presentation_time = None;
    let mut previous_byte_offset = None;
    let mut total_gap_duration = 0_u64;
    let mut sync_sample_count = 0_u64;
    let mut source_byte_order_reordered = false;
    let mut presentation_reordered = false;
    let mut has_negative_presentation_time = false;
    let mut presentation_start = None;
    let mut presentation_end = None;

    for sample in samples {
        let expected_index = match output.last() {
            Some(previous) => previous.sample_index.checked_add(1),
            None => Some(expected_start_sample),
        }
        .ok_or(TimelineError::ArithmeticOverflow {
            field: "sample_index",
        })?;
        if sample.sample_index != expected_index {
            return Err(TimelineError::NonContiguousSampleIndex {
                expected: expected_index,
                observed: sample.sample_index,
            });
        }
        if sample.duration == 0 {
            return Err(TimelineError::ZeroDuration {
                sample_index: sample.sample_index,
            });
        }
        if sample.byte_length == 0 {
            return Err(TimelineError::ZeroByteLength {
                sample_index: sample.sample_index,
            });
        }
        if sample.sample_description_index == 0 {
            return Err(TimelineError::ZeroSampleDescriptionIndex {
                sample_index: sample.sample_index,
            });
        }

        if let Some(previous) = output.last() {
            let previous_end = previous_decode_end.ok_or(TimelineError::DerivedSummaryMismatch {
                field: "previous_decode_end",
            })?;
            if sample.decode_time < previous_end {
                return Err(TimelineError::OverlappingDecodeIntervals {
                    previous_sample_index: previous.sample_index,
                    previous_decode_end: previous_end,
                    sample_index: sample.sample_index,
                    decode_time: sample.decode_time,
                });
            }
            if sample.decode_time > previous_end {
                let duration = sample.decode_time.saturating_sub(previous_end);
                total_gap_duration = total_gap_duration.checked_add(duration).ok_or(
                    TimelineError::ArithmeticOverflow {
                        field: "total_gap_duration",
                    },
                )?;
                gaps.push(TimelineGap {
                    after_sample_index: previous.sample_index,
                    before_sample_index: sample.sample_index,
                    start_decode_time: previous_end,
                    end_decode_time: sample.decode_time,
                    duration,
                });
            }
        }

        let byte_end = sample
            .byte_offset
            .checked_add(u64::from(sample.byte_length))
            .ok_or(TimelineError::ArithmeticOverflow {
                field: "sample_byte_end",
            })?;
        if byte_end > source_object_length {
            return Err(TimelineError::SampleOutsideSource {
                sample_index: sample.sample_index,
                byte_end,
                source_object_length,
            });
        }
        if previous_byte_offset.is_some_and(|previous| sample.byte_offset < previous) {
            source_byte_order_reordered = true;
        }
        byte_ranges.push((sample.byte_offset, byte_end, sample.sample_index));

        let decode_end = sample
            .decode_time
            .checked_add(u64::from(sample.duration))
            .ok_or(TimelineError::ArithmeticOverflow {
                field: "decode_end",
            })?;
        let decode_time_i128 = i128::from(sample.decode_time);
        let composition_offset = sample
            .composition_time
            .checked_sub(decode_time_i128)
            .ok_or(TimelineError::ArithmeticOverflow {
                field: "composition_offset",
            })?;
        let presentation_sample_end = sample
            .composition_time
            .checked_add(i128::from(sample.duration))
            .ok_or(TimelineError::ArithmeticOverflow {
                field: "presentation_end",
            })?;
        if previous_presentation_time.is_some_and(|previous| sample.composition_time < previous) {
            presentation_reordered = true;
        }
        if sample.composition_time < 0 {
            has_negative_presentation_time = true;
        }
        presentation_start = Some(presentation_start.map_or(
            sample.composition_time,
            |current: i128| current.min(sample.composition_time),
        ));
        presentation_end = Some(presentation_end.map_or(
            presentation_sample_end,
            |current: i128| current.max(presentation_sample_end),
        ));
        if sample.is_sync {
            sync_sample_count = sync_sample_count.checked_add(1).ok_or(
                TimelineError::ArithmeticOverflow {
                    field: "sync_sample_count",
                },
            )?;
        }
        description_indices.insert(sample.sample_description_index);
        output.push(TimelineSample {
            sample_index: sample.sample_index,
            decode_time: sample.decode_time,
            presentation_time: sample.composition_time,
            composition_offset,
            duration: sample.duration,
            decode_end,
            presentation_end: presentation_sample_end,
            byte_offset: sample.byte_offset,
            byte_end,
            byte_length: sample.byte_length,
            is_sync: sample.is_sync,
            sample_description_index: sample.sample_description_index,
        });
        previous_decode_end = Some(decode_end);
        previous_presentation_time = Some(sample.composition_time);
        previous_byte_offset = Some(sample.byte_offset);
    }

    validate_nonoverlapping_byte_ranges(&byte_ranges)?;
    let decode_start = output.first().map(|sample| sample.decode_time);
    let decode_end = output.last().map(|sample| sample.decode_end);
    Ok(TimelineParts {
        samples: output,
        gaps,
        total_gap_duration,
        sync_sample_count,
        sample_description_indices: description_indices.into_iter().collect(),
        source_byte_order_reordered,
        presentation_reordered,
        has_negative_presentation_time,
        decode_start,
        decode_end,
        presentation_start,
        presentation_end,
    })
}
