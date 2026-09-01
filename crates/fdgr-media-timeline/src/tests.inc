#[cfg(test)]
mod tests {
    use super::{TimelineBasis, TimelineError, build_sample_timeline};
    use fdgr_media::{SampleRecord, TrackSampleWindow};
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis() -> TimelineBasis {
        TimelineBasis {
            recorded_media_root_manifest_digest: digest(1),
            source_manifest_digest: digest(2),
            source_object_digest: digest(3),
            source_object_length: 10_000,
            track_id: 7,
            timescale: 1_000,
        }
    }

    fn sample(index: u64, decode_time: u64, presentation_time: i128) -> SampleRecord {
        SampleRecord {
            sample_index: index,
            decode_time,
            composition_time: presentation_time,
            duration: 10,
            byte_offset: 100 + index.saturating_mul(50),
            byte_length: 50,
            is_sync: index == 0,
            sample_description_index: 1,
        }
    }

    fn window(samples: Vec<SampleRecord>) -> TrackSampleWindow {
        TrackSampleWindow {
            track_id: 7,
            timescale: 1_000,
            total_samples: 3,
            start_sample: 0,
            requested_max_samples: 3,
            complete: true,
            index_entries_scanned: 12,
            samples,
        }
    }

    #[test]
    fn timeline_records_gaps_reordering_and_exact_coverage() {
        let source = window(vec![sample(0, 0, 20), sample(1, 10, 0), sample(2, 30, 30)]);
        let timeline = build_sample_timeline(basis(), &source);
        assert!(matches!(
            timeline,
            Ok(ref value)
                if value.gaps.len() == 1
                    && value.gaps.first().is_some_and(|gap| gap.duration == 10)
                    && value.total_gap_duration == 10
                    && value.presentation_reordered
                    && value.covers_entire_track
                    && value.prefix_unrepresented_samples == 0
                    && value.suffix_unrepresented_samples == 0
                    && value.decode_start == Some(0)
                    && value.decode_end == Some(40)
                    && value.presentation_start == Some(0)
                    && value.presentation_end == Some(40)
                    && value.validate().is_ok()
        ));
    }

    #[test]
    fn partial_window_cannot_masquerade_as_complete_track_coverage() {
        let mut source = TrackSampleWindow {
            track_id: 7,
            timescale: 1_000,
            total_samples: 5,
            start_sample: 2,
            requested_max_samples: 2,
            complete: false,
            index_entries_scanned: 8,
            samples: vec![sample(2, 20, 20), sample(3, 30, 30)],
        };
        let timeline = build_sample_timeline(basis(), &source);
        assert!(matches!(
            timeline,
            Ok(ref value)
                if value.end_sample == 4
                    && !value.reaches_track_end
                    && !value.covers_entire_track
                    && value.prefix_unrepresented_samples == 2
                    && value.suffix_unrepresented_samples == 1
        ));
        source.complete = true;
        assert!(matches!(
            build_sample_timeline(basis(), &source),
            Err(TimelineError::CompleteFlagMismatch {
                expected: false,
                observed: true,
            })
        ));
    }

    #[test]
    fn semantic_identity_ignores_request_and_scan_cost_diagnostics() {
        let mut first_window = window(vec![sample(0, 0, 0), sample(1, 10, 10), sample(2, 20, 20)]);
        let mut second_window = first_window.clone();
        first_window.requested_max_samples = 3;
        first_window.index_entries_scanned = 12;
        second_window.requested_max_samples = 99;
        second_window.index_entries_scanned = 9_999;
        let first = build_sample_timeline(basis(), &first_window);
        let second = build_sample_timeline(basis(), &second_window);
        assert!(matches!(
            (&first, &second),
            (Ok(left), Ok(right))
                if left.requested_max_samples != right.requested_max_samples
                    && left.index_entries_scanned != right.index_entries_scanned
                    && matches!((left.digest(), right.digest()), (Ok(a), Ok(b)) if a == b)
        ));
    }

    #[test]
    fn timeline_identity_is_deterministic_and_basis_sensitive() {
        let source = window(vec![sample(0, 0, 0), sample(1, 10, 10), sample(2, 20, 20)]);
        let first = build_sample_timeline(basis(), &source);
        let second = build_sample_timeline(basis(), &source);
        assert!(matches!(
            (&first, &second),
            (Ok(left), Ok(right))
                if matches!((left.digest(), right.digest()), (Ok(a), Ok(b)) if a == b)
        ));
        let mut changed_basis = basis();
        changed_basis.source_manifest_digest = digest(9);
        let changed = build_sample_timeline(changed_basis, &source);
        assert!(matches!(
            (&first, &changed),
            (Ok(left), Ok(right))
                if matches!((left.digest(), right.digest()), (Ok(a), Ok(b)) if a != b)
        ));
    }

    #[test]
    fn overlapping_decode_intervals_are_refused() {
        let source = window(vec![sample(0, 0, 0), sample(1, 5, 5), sample(2, 20, 20)]);
        assert!(matches!(
            build_sample_timeline(basis(), &source),
            Err(TimelineError::OverlappingDecodeIntervals {
                previous_sample_index: 0,
                sample_index: 1,
                ..
            })
        ));
    }

    #[test]
    fn overlapping_byte_intervals_are_refused_even_when_decode_order_is_valid() {
        let first = sample(0, 0, 0);
        let mut second = sample(1, 10, 10);
        second.byte_offset = 125;
        let source = window(vec![first, second, sample(2, 20, 20)]);
        assert!(matches!(
            build_sample_timeline(basis(), &source),
            Err(TimelineError::OverlappingByteIntervals {
                previous_sample_index: 0,
                sample_index: 1,
                ..
            })
        ));
    }

    #[test]
    fn source_byte_reordering_is_reported_without_inventing_an_error() {
        let first = sample(0, 0, 0);
        let mut second = sample(1, 10, 10);
        let mut third = sample(2, 20, 20);
        second.byte_offset = 500;
        third.byte_offset = 250;
        let source = window(vec![first, second, third]);
        let timeline = build_sample_timeline(basis(), &source);
        assert!(matches!(timeline, Ok(ref value) if value.source_byte_order_reordered));
    }

    #[test]
    fn mixed_track_basis_is_refused() {
        let source = window(vec![sample(0, 0, 0), sample(1, 10, 10), sample(2, 20, 20)]);
        let mut mixed = basis();
        mixed.track_id = 8;
        assert!(matches!(
            build_sample_timeline(mixed, &source),
            Err(TimelineError::TrackMismatch {
                expected: 8,
                observed: 7,
            })
        ));
    }

    #[test]
    fn noncontiguous_sample_indices_are_refused() {
        let source = window(vec![sample(0, 0, 0), sample(2, 10, 10), sample(3, 20, 20)]);
        assert!(matches!(
            build_sample_timeline(basis(), &source),
            Err(TimelineError::NonContiguousSampleIndex {
                expected: 1,
                observed: 2,
            })
        ));
    }

    #[test]
    fn sample_beyond_authenticated_source_is_refused() {
        let mut source = window(vec![sample(0, 0, 0), sample(1, 10, 10), sample(2, 20, 20)]);
        if let Some(last) = source.samples.last_mut() {
            last.byte_offset = 9_990;
            last.byte_length = 50;
        }
        assert!(matches!(
            build_sample_timeline(basis(), &source),
            Err(TimelineError::SampleOutsideSource {
                sample_index: 2,
                ..
            })
        ));
    }

    #[test]
    fn zero_identities_and_zero_byte_samples_are_refused() {
        let source = window(vec![sample(0, 0, 0), sample(1, 10, 10), sample(2, 20, 20)]);
        let mut zero_basis = basis();
        zero_basis.source_manifest_digest = EvidenceDigest::from_bytes([0_u8; 32]);
        assert!(matches!(
            build_sample_timeline(zero_basis, &source),
            Err(TimelineError::ZeroIdentity {
                field: "source_manifest_digest",
            })
        ));
        let mut zero_sample = sample(0, 0, 0);
        zero_sample.byte_length = 0;
        let zero_window = window(vec![zero_sample, sample(1, 10, 10), sample(2, 20, 20)]);
        assert!(matches!(
            build_sample_timeline(basis(), &zero_window),
            Err(TimelineError::ZeroByteLength { sample_index: 0 })
        ));
    }

    #[test]
    fn json_uses_lossless_decimal_strings_for_presentation_ticks() {
        let source = window(vec![sample(0, 0, -5), sample(1, 10, 10), sample(2, 20, 20)]);
        let timeline = build_sample_timeline(basis(), &source);
        assert!(matches!(
            timeline.and_then(|value| value.to_json()),
            Ok(ref json)
                if json.contains("\"presentation_time_ticks\":\"-5\"")
                    && json.contains("\"has_negative_presentation_time\":true")
        ));
    }
}
