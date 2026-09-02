#[cfg(test)]
mod tests {
    use super::{
        KeyframeBasis, KeyframeCandidate, KeyframeError, KeyframePolicy,
        KeyframeRejectionReason, PPM_SCALE, select_keyframes,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis(byte: u8) -> KeyframeBasis {
        KeyframeBasis {
            timeline_digest: digest(byte),
            decoded_frame_generation_digest: digest(20),
            calibration_digest: digest(21),
            candidate_basis_digest: digest(22),
            policy_digest: digest(23),
            selection_generation: 1,
        }
    }

    fn candidate(
        sample_index: u64,
        presentation_tick: i128,
        cells: Vec<u64>,
        view_sector: u32,
        baseline_bin: u32,
    ) -> KeyframeCandidate {
        KeyframeCandidate {
            sample_index,
            frame_digest: digest(u8::try_from(sample_index.saturating_add(1)).map_or(255, |v| v)),
            presentation_tick,
            sharpness_ppm: 800_000,
            texture_ppm: 700_000,
            dark_clipped_ppm: 10_000,
            bright_clipped_ppm: 20_000,
            dynamic_content_ppm: 30_000,
            overlap_ppm: 600_000,
            view_sector,
            baseline_bin,
            coverage_cells: cells,
        }
    }

    fn policy(max_selected: u32) -> KeyframePolicy {
        KeyframePolicy {
            max_selected,
            min_sharpness_ppm: 200_000,
            min_texture_ppm: 100_000,
            max_dark_clipped_ppm: 200_000,
            max_bright_clipped_ppm: 200_000,
            max_dynamic_content_ppm: 300_000,
            min_overlap_ppm: 100_000,
            min_presentation_spacing_ticks: 0,
            quality_weight: 1,
            coverage_weight: 1_000,
            view_diversity_weight: 250_000,
            baseline_diversity_weight: 250_000,
        }
    }

    #[test]
    fn marginal_coverage_and_diversity_drive_selection() {
        let mut low_quality = candidate(3, 3_000, vec![4], 3, 3);
        low_quality.sharpness_ppm = 10_000;
        let result = select_keyframes(
            basis(1),
            policy(2),
            vec![
                candidate(2, 2_000, vec![1, 2], 1, 1),
                candidate(1, 1_000, vec![2, 3], 2, 2),
                candidate(0, 0, vec![1, 2], 1, 1),
                low_quality,
            ],
        );
        assert!(matches!(
            result,
            Ok(ref selection)
                if selection.selected.len() == 2
                    && selection.selected[0].sample_index == 0
                    && selection.selected[1].sample_index == 1
                    && selection.covered_cells == vec![1, 2, 3]
                    && selection.rejected.iter().any(|value| value.sample_index == 2 && value.reason == KeyframeRejectionReason::RedundantEvidence)
                    && selection.rejected.iter().any(|value| value.sample_index == 3 && value.reason == KeyframeRejectionReason::SharpnessBelowMinimum)
                    && selection.validate().is_ok()
        ));
    }

    #[test]
    fn input_order_does_not_change_identity() {
        let candidates = vec![
            candidate(0, 0, vec![1, 2], 1, 1),
            candidate(1, 1_000, vec![2, 3], 2, 2),
            candidate(2, 2_000, vec![3, 4], 3, 3),
        ];
        let mut reversed = candidates.clone();
        reversed.reverse();
        let left = select_keyframes(basis(2), policy(2), candidates);
        let right = select_keyframes(basis(2), policy(2), reversed);
        assert!(matches!(
            (left, right),
            (Ok(left), Ok(right))
                if left == right && left.digest().is_ok() && left.digest() == right.digest()
        ));
    }

    #[test]
    fn temporal_spacing_is_explicit() {
        let mut spacing_policy = policy(3);
        spacing_policy.min_presentation_spacing_ticks = 100;
        let result = select_keyframes(
            basis(3),
            spacing_policy,
            vec![
                candidate(0, 0, vec![1, 2], 1, 1),
                candidate(1, 50, vec![3, 4], 2, 2),
                candidate(2, 200, vec![5, 6], 3, 3),
            ],
        );
        assert!(matches!(
            result,
            Ok(ref selection)
                if selection.selected.len() == 2
                    && selection.rejected.iter().any(|value| value.sample_index == 1
                        && value.reason == KeyframeRejectionReason::TemporalSpacing
                        && value.conflicts_with_sample_index == Some(0))
        ));
    }

    #[test]
    fn capacity_is_distinct_from_redundancy() {
        let result = select_keyframes(
            basis(4),
            policy(1),
            vec![
                candidate(0, 0, vec![1], 1, 1),
                candidate(1, 1_000, vec![2], 2, 2),
            ],
        );
        assert!(matches!(
            result,
            Ok(ref selection)
                if selection.selected.len() == 1
                    && selection.rejected.len() == 1
                    && selection.rejected[0].reason == KeyframeRejectionReason::Capacity
                    && selection.rejected[0].marginal_coverage_cells == 1
        ));
    }

    #[test]
    fn malformed_coverage_and_duplicate_identity_fail_closed() {
        let malformed = select_keyframes(
            basis(5),
            policy(1),
            vec![candidate(0, 0, vec![2, 1], 1, 1)],
        );
        assert!(matches!(
            malformed,
            Err(KeyframeError::NonCanonicalCoverageCells { sample_index: 0 })
        ));
        let first = candidate(0, 0, vec![1], 1, 1);
        let mut duplicate = candidate(1, 1_000, vec![2], 2, 2);
        duplicate.frame_digest = first.frame_digest.clone();
        assert!(matches!(
            select_keyframes(basis(6), policy(1), vec![first, duplicate]),
            Err(KeyframeError::DuplicateFrameDigest { .. })
        ));
    }

    #[test]
    fn negative_ticks_and_large_scores_are_lossless_json_strings() {
        let result = select_keyframes(
            basis(7),
            policy(1),
            vec![candidate(0, -9_007_199_254_740_993, vec![1], 1, 1)],
        );
        assert!(matches!(
            result,
            Ok(ref selection)
                if matches!(selection.to_json(), Ok(ref json)
                    if json.contains("\"presentation_tick\":\"-9007199254740993\"")
                        && json.contains("\"total_score\":\""))
        ));
    }

    #[test]
    fn basis_and_policy_control_identity() {
        let candidates = vec![candidate(0, 0, vec![1], 1, 1)];
        let left = select_keyframes(basis(8), policy(1), candidates.clone());
        let right = select_keyframes(basis(9), policy(1), candidates.clone());
        let mut changed_policy = policy(1);
        changed_policy.coverage_weight = 2_000;
        let policy_changed = select_keyframes(basis(8), changed_policy, candidates);
        assert!(matches!(
            (left, right, policy_changed),
            (Ok(left), Ok(right), Ok(policy_changed))
                if left.digest().is_ok()
                    && right.digest().is_ok()
                    && policy_changed.digest().is_ok()
                    && left.digest() != right.digest()
                    && left.digest() != policy_changed.digest()
        ));
    }

    #[test]
    fn ppm_bounds_are_enforced() {
        let mut value = candidate(0, 0, vec![1], 1, 1);
        value.overlap_ppm = PPM_SCALE.saturating_add(1);
        assert!(matches!(
            select_keyframes(basis(10), policy(1), vec![value]),
            Err(KeyframeError::BoundExceeded {
                field: "overlap_ppm",
                ..
            })
        ));
    }
}
