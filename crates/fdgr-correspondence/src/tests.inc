#[cfg(test)]
mod tests {
    use super::{
        CorrespondenceBasis, CorrespondenceError, Descriptor256, FeatureObservation, FramePair,
        MatchPolicy, MatchRejectionReason, PPM_SCALE, build_correspondences,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis(byte: u8) -> CorrespondenceBasis {
        CorrespondenceBasis {
            keyframe_selection_digest: digest(byte),
            calibration_digest: digest(20),
            feature_basis_digest: digest(21),
            pair_basis_digest: digest(22),
            policy_digest: digest(23),
            generation: 1,
        }
    }

    fn descriptor(first: u64) -> Descriptor256 {
        Descriptor256([first, 0, 0, 0])
    }

    fn observation(
        observation_id: u64,
        sample_index: u64,
        feature_id: u32,
        descriptor: Descriptor256,
    ) -> FeatureObservation {
        FeatureObservation {
            observation_id,
            sample_index,
            frame_digest: digest(u8::try_from(sample_index.saturating_add(40)).map_or(255, |v| v)),
            feature_id,
            x_nano_pixels: i64::from(feature_id) * 1_000_000_000,
            y_nano_pixels: i64::from(feature_id) * 2_000_000_000,
            response_ppm: 800_000,
            uncertainty_nano_pixels: 100_000_000,
            descriptor,
            dynamic_masked: false,
        }
    }

    const fn pair(pair_id: u64, left: u64, right: u64) -> FramePair {
        FramePair {
            pair_id,
            left_sample_index: left,
            right_sample_index: right,
        }
    }

    const fn policy() -> MatchPolicy {
        MatchPolicy {
            max_hamming_distance: 64,
            ratio_threshold_ppm: 800_000,
            require_second_best: false,
            require_mutual: true,
            min_response_ppm: 100_000,
            max_uncertainty_nano_pixels: 1_000_000_000,
            reject_dynamic_masked: true,
            max_distance_evaluations: 10_000,
        }
    }

    #[test]
    fn exact_chain_forms_one_deterministic_track() {
        let result = build_correspondences(
            basis(1),
            policy(),
            vec![
                observation(1, 0, 1, descriptor(1)),
                observation(2, 1, 1, descriptor(1)),
                observation(3, 2, 1, descriptor(1)),
            ],
            vec![pair(1, 0, 1), pair(2, 1, 2)],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.accepted_matches.len() == 2
                    && value.rejected_matches.is_empty()
                    && value.tracks.len() == 1
                    && value.tracks[0].observation_ids == vec![1, 2, 3]
                    && value.tracks[0].sample_indices == vec![0, 1, 2]
                    && value.unmatched_observation_ids.is_empty()
                    && value.validate().is_ok()
                    && matches!(value.to_json(), Ok(ref json) if json.contains("fdgr.correspondence_generation/1"))
        ));
    }

    #[test]
    fn ambiguity_and_quality_failures_remain_explicit() {
        let mut low_response = observation(1, 0, 1, descriptor(1));
        low_response.response_ppm = 1;
        let result = build_correspondences(
            basis(2),
            policy(),
            vec![
                low_response,
                observation(2, 0, 2, descriptor(0)),
                observation(3, 1, 1, descriptor(1)),
                observation(4, 1, 2, descriptor(2)),
            ],
            vec![pair(1, 0, 1)],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.ineligible_observation_ids == vec![1]
                    && value.rejected_matches.iter().any(|entry| entry.left_observation_id == 1 && entry.reason == MatchRejectionReason::ResponseBelowMinimum)
                    && value.rejected_matches.iter().any(|entry| entry.left_observation_id == 2 && entry.reason == MatchRejectionReason::NearestTie)
        ));
    }

    #[test]
    fn track_union_rejects_same_frame_collisions() {
        let mut broad = policy();
        broad.max_hamming_distance = 256;
        let result = build_correspondences(
            basis(3),
            broad,
            vec![
                observation(1, 0, 1, descriptor(0)),
                observation(2, 0, 2, descriptor(u64::MAX)),
                observation(3, 1, 1, descriptor(0)),
                observation(4, 2, 1, descriptor(u64::MAX)),
            ],
            vec![pair(1, 0, 1), pair(2, 0, 2), pair(3, 1, 2)],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.accepted_matches.len() == 2
                    && value.tracks.len() == 2
                    && value.rejected_matches.iter().any(|entry| entry.pair_id == 3 && entry.reason == MatchRejectionReason::FrameCollision)
                    && value.tracks.iter().all(|track| track.sample_indices.windows(2).all(|samples| matches!(samples, [left, right] if left < right)))
        ));
    }

    #[test]
    fn operation_budget_fails_before_unbounded_matching() {
        let mut bounded = policy();
        bounded.max_distance_evaluations = 1;
        let result = build_correspondences(
            basis(4),
            bounded,
            vec![
                observation(1, 0, 1, descriptor(0)),
                observation(2, 0, 2, descriptor(1)),
                observation(3, 1, 1, descriptor(0)),
                observation(4, 1, 2, descriptor(1)),
            ],
            vec![pair(1, 0, 1)],
        );
        assert!(matches!(
            result,
            Err(CorrespondenceError::DistanceBudgetExceeded {
                attempted: 2,
                maximum: 1
            })
        ));
    }

    #[test]
    fn input_order_and_successful_budget_do_not_change_identity() {
        let observations = vec![
            observation(1, 0, 1, descriptor(1)),
            observation(2, 1, 1, descriptor(1)),
            observation(3, 2, 1, descriptor(1)),
        ];
        let pairs = vec![pair(1, 0, 1), pair(2, 1, 2)];
        let mut reversed_observations = observations.clone();
        reversed_observations.reverse();
        let mut reversed_pairs = pairs.clone();
        reversed_pairs.reverse();
        let mut high_budget = policy();
        high_budget.max_distance_evaluations = 1_000_000;
        let left = build_correspondences(basis(5), policy(), observations, pairs);
        let right = build_correspondences(
            basis(5),
            high_budget,
            reversed_observations,
            reversed_pairs,
        );
        assert!(matches!(
            (left, right),
            (Ok(left), Ok(right))
                if left.accepted_matches == right.accepted_matches
                    && left.tracks == right.tracks
                    && left.digest().is_ok()
                    && left.digest() == right.digest()
        ));
    }

    #[test]
    fn duplicate_features_and_noncanonical_pairs_fail_closed() {
        let duplicate_feature = build_correspondences(
            basis(6),
            policy(),
            vec![
                observation(1, 0, 1, descriptor(0)),
                observation(2, 0, 1, descriptor(1)),
                observation(3, 1, 1, descriptor(0)),
            ],
            vec![pair(1, 0, 1)],
        );
        assert!(matches!(
            duplicate_feature,
            Err(CorrespondenceError::DuplicateFeatureInFrame {
                sample_index: 0,
                feature_id: 1
            })
        ));
        let noncanonical = build_correspondences(
            basis(7),
            policy(),
            vec![
                observation(1, 0, 1, descriptor(0)),
                observation(2, 1, 1, descriptor(0)),
            ],
            vec![pair(1, 1, 0)],
        );
        assert!(matches!(
            noncanonical,
            Err(CorrespondenceError::NonCanonicalFramePair { pair_id: 1 })
        ));
    }

    #[test]
    fn second_best_requirement_refuses_trivial_singleton_matches() {
        let mut strict = policy();
        strict.require_second_best = true;
        let result = build_correspondences(
            basis(8),
            strict,
            vec![
                observation(1, 0, 1, descriptor(1)),
                observation(2, 1, 1, descriptor(1)),
            ],
            vec![pair(1, 0, 1)],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.accepted_matches.is_empty()
                    && value.rejected_matches.iter().any(|entry|
                        entry.left_observation_id == 1
                            && entry.reason == MatchRejectionReason::SecondBestUnavailable)
        ));
    }

    #[test]
    fn ppm_policy_bounds_are_enforced() {
        let mut invalid = policy();
        invalid.ratio_threshold_ppm = PPM_SCALE.saturating_add(1);
        assert!(matches!(
            build_correspondences(
                basis(9),
                invalid,
                vec![
                    observation(1, 0, 1, descriptor(0)),
                    observation(2, 1, 1, descriptor(0)),
                ],
                vec![pair(1, 0, 1)],
            ),
            Err(CorrespondenceError::BoundExceeded {
                field: "ratio_threshold_ppm",
                ..
            })
        ));
    }
}
