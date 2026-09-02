#[cfg(test)]
mod tests {
    use super::{
        BearingMatch, MAX_SAFE_JSON_INTEGER, PoseCandidateSource, PoseMatchRejectionReason,
        RelativePoseBasis, RelativePoseCandidate, RelativePoseError, RelativePosePolicy,
        RelativePoseStatus, UNIT_NANO, verify_relative_pose,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis(byte: u8) -> RelativePoseBasis {
        RelativePoseBasis {
            correspondence_generation_digest: digest(byte),
            calibration_digest: digest(20),
            bearing_basis_digest: digest(21),
            candidate_basis_digest: digest(22),
            policy_digest: digest(23),
            left_sample_index: 10,
            right_sample_index: 20,
            generation: 1,
        }
    }

    fn bearing_match(match_id: u64) -> BearingMatch {
        BearingMatch {
            match_id,
            left_observation_id: match_id,
            right_observation_id: match_id.saturating_add(100),
            left_bearing_nano: [0, 0, UNIT_NANO],
            right_bearing_nano: [-600_000_000, 0, 800_000_000],
            uncertainty_nano: 0,
        }
    }

    fn matches(count: u64) -> Vec<BearingMatch> {
        (1..=count).map(bearing_match).collect()
    }

    fn candidate(
        candidate_id: u64,
        translation: [i64; 3],
    ) -> RelativePoseCandidate {
        RelativePoseCandidate {
            candidate_id,
            evidence_digest: digest(u8::try_from(candidate_id).map_or(250, |value| value)),
            source: PoseCandidateSource::DiagnosticHypothesis,
            rotation_left_to_right_nano: [
                UNIT_NANO, 0, 0, 0, UNIT_NANO, 0, 0, 0, UNIT_NANO,
            ],
            translation_left_origin_in_right_nano: translation,
        }
    }

    fn policy() -> RelativePosePolicy {
        RelativePosePolicy {
            max_epipolar_residual_nano: 1_000_000,
            min_epipolar_normal_nano: 10_000_000,
            min_parallax_nano: 1_000_000,
            min_inlier_matches: 3,
            min_inlier_ratio_ppm: 800_000,
            min_positive_depth_ratio_ppm: 800_000,
            max_median_residual_nano: 1_000_000,
            require_cheirality: true,
            max_evaluations: 1_000,
        }
    }

    #[test]
    fn unique_motion_is_geometrically_verified() {
        let result = verify_relative_pose(
            basis(1),
            policy(),
            matches(4),
            vec![
                candidate(1, [-UNIT_NANO, 0, 0]),
                candidate(2, [UNIT_NANO, 0, 0]),
                candidate(3, [0, -UNIT_NANO, 0]),
            ],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.status == RelativePoseStatus::GeometricallyVerified
                    && value.selected_candidate_id == Some(1)
                    && value.ambiguous_candidate_ids.is_empty()
                    && value.evaluations[0].accepted
                    && value.evaluations[0].inlier_count == 4
                    && !value.evaluations[1].accepted
                    && value.evaluations[1].assessments.iter().all(|assessment|
                        assessment.rejection_reason == Some(PoseMatchRejectionReason::CheiralityFailed))
                    && !value.evaluations[2].accepted
                    && value.evaluations[2].assessments.iter().all(|assessment|
                        assessment.rejection_reason == Some(PoseMatchRejectionReason::EpipolarResidualExceeded))
                    && value.validate().is_ok()
                    && value.selected_candidate().is_some_and(|candidate| candidate.candidate_id == 1)
                    && matches!(value.to_json(), Ok(ref json) if json.contains("fdgr.relative_pose_verification/1"))
        ));
    }

    #[test]
    fn exact_semantic_tie_remains_ambiguous() {
        let result = verify_relative_pose(
            basis(2),
            policy(),
            matches(3),
            vec![
                candidate(7, [-UNIT_NANO, 0, 0]),
                candidate(8, [-UNIT_NANO, 0, 0]),
            ],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.status == RelativePoseStatus::Ambiguous
                    && value.selected_candidate_id.is_none()
                    && value.ambiguous_candidate_ids == vec![7, 8]
        ));
    }

    #[test]
    fn no_passing_candidate_is_explicit() {
        let result = verify_relative_pose(
            basis(3),
            policy(),
            matches(3),
            vec![candidate(1, [0, -UNIT_NANO, 0])],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.status == RelativePoseStatus::NoAcceptedCandidate
                    && value.selected_candidate_id.is_none()
                    && value.ambiguous_candidate_ids.is_empty()
        ));
    }

    #[test]
    fn input_order_does_not_change_identity() {
        let mut reversed_matches = matches(4);
        reversed_matches.reverse();
        let mut reversed_candidates = vec![
            candidate(1, [-UNIT_NANO, 0, 0]),
            candidate(2, [UNIT_NANO, 0, 0]),
        ];
        reversed_candidates.reverse();
        let left = verify_relative_pose(
            basis(4),
            policy(),
            matches(4),
            vec![
                candidate(1, [-UNIT_NANO, 0, 0]),
                candidate(2, [UNIT_NANO, 0, 0]),
            ],
        );
        let right = verify_relative_pose(
            basis(4),
            policy(),
            reversed_matches,
            reversed_candidates,
        );
        assert!(matches!(
            (left, right),
            (Ok(left), Ok(right))
                if left == right && left.digest().is_ok() && left.digest() == right.digest()
        ));
    }

    #[test]
    fn successful_operation_ceiling_does_not_change_semantic_identity() {
        let mut low = policy();
        low.max_evaluations = 3;
        let mut high = low;
        high.max_evaluations = 1_000;
        let left = verify_relative_pose(
            basis(5),
            low,
            matches(3),
            vec![candidate(1, [-UNIT_NANO, 0, 0])],
        );
        let right = verify_relative_pose(
            basis(5),
            high,
            matches(3),
            vec![candidate(1, [-UNIT_NANO, 0, 0])],
        );
        assert!(matches!(
            (left, right),
            (Ok(left), Ok(right))
                if left.digest().is_ok() && left.digest() == right.digest()
        ));
    }

    #[test]
    fn insufficient_operation_budget_fails_before_scoring() {
        let mut bounded = policy();
        bounded.max_evaluations = 5;
        let result = verify_relative_pose(
            basis(6),
            bounded,
            matches(3),
            vec![
                candidate(1, [-UNIT_NANO, 0, 0]),
                candidate(2, [UNIT_NANO, 0, 0]),
            ],
        );
        assert!(matches!(
            result,
            Err(RelativePoseError::EvaluationBudgetExceeded {
                required: 6,
                maximum: 5
            })
        ));
    }

    #[test]
    fn malformed_rotation_and_bearing_fail_closed() {
        let mut invalid_candidate = candidate(1, [-UNIT_NANO, 0, 0]);
        invalid_candidate.rotation_left_to_right_nano[0] = 0;
        assert!(matches!(
            verify_relative_pose(
                basis(7),
                policy(),
                matches(3),
                vec![invalid_candidate]
            ),
            Err(RelativePoseError::RotationNormInvalid { .. })
        ));
        let mut invalid_match = bearing_match(1);
        invalid_match.left_bearing_nano = [1, 0, 0];
        assert!(matches!(
            verify_relative_pose(
                basis(8),
                policy(),
                vec![invalid_match, bearing_match(2), bearing_match(3)],
                vec![candidate(1, [-UNIT_NANO, 0, 0])]
            ),
            Err(RelativePoseError::UnitVectorInvalid {
                field: "left_bearing_nano",
                ..
            })
        ));
    }

    #[test]
    fn duplicate_matches_and_noncanonical_basis_fail_closed() {
        let duplicate = verify_relative_pose(
            basis(9),
            policy(),
            vec![bearing_match(1), bearing_match(1), bearing_match(2)],
            vec![candidate(1, [-UNIT_NANO, 0, 0])],
        );
        assert!(matches!(
            duplicate,
            Err(RelativePoseError::DuplicateMatchId { match_id: 1 })
        ));
        let mut invalid_basis = basis(10);
        invalid_basis.left_sample_index = invalid_basis.right_sample_index;
        assert!(matches!(
            verify_relative_pose(
                invalid_basis,
                policy(),
                matches(3),
                vec![candidate(1, [-UNIT_NANO, 0, 0])]
            ),
            Err(RelativePoseError::NonCanonicalFramePair)
        ));
    }

    #[test]
    fn public_identifiers_remain_json_safe() {
        let mut invalid = bearing_match(1);
        invalid.match_id = MAX_SAFE_JSON_INTEGER.saturating_add(1);
        assert!(matches!(
            verify_relative_pose(
                basis(11),
                policy(),
                vec![invalid, bearing_match(2), bearing_match(3)],
                vec![candidate(1, [-UNIT_NANO, 0, 0])]
            ),
            Err(RelativePoseError::BoundExceeded {
                field: "match_id",
                ..
            })
        ));
    }
}
