#[cfg(test)]
mod tests {
    use super::{
        CandidateSource, EpipolarBasis, EpipolarError, EpipolarPolicy, EssentialCandidate,
        NormalizedCorrespondence, VerificationDecision, verify_epipolar_candidates,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis(byte: u8) -> EpipolarBasis {
        EpipolarBasis {
            correspondence_generation_digest: digest(byte),
            calibration_digest: digest(20),
            observation_basis_digest: digest(21),
            candidate_basis_digest: digest(22),
            policy_digest: digest(23),
            pair_id: 7,
            left_sample_index: 10,
            right_sample_index: 20,
            generation: 1,
        }
    }

    fn observations(parallax_x_ppm: i64) -> Vec<NormalizedCorrespondence> {
        (0_u64..12)
            .map(|index| {
                let x = i64::try_from(index).map_or(0, |value| value * 50_000 - 250_000);
                let y = i64::try_from(index % 4).map_or(0, |value| value * 80_000 - 120_000);
                NormalizedCorrespondence {
                    match_id: index + 1,
                    pair_id: 7,
                    left_observation_id: index + 101,
                    right_observation_id: index + 201,
                    left_x_ppm: x,
                    left_y_ppm: y,
                    right_x_ppm: x + parallax_x_ppm,
                    right_y_ppm: y,
                    uncertainty_ppm: 100,
                    left_spatial_bin: u16::try_from(index % 4 + 1).map_or(1, |value| value),
                    right_spatial_bin: u16::try_from(index % 4 + 1).map_or(1, |value| value),
                }
            })
            .collect()
    }

    const fn translation_x_candidate(candidate_id: u64) -> EssentialCandidate {
        EssentialCandidate {
            candidate_id,
            source: CandidateSource::NativeMinimalSolver,
            matrix: [0, 0, 0, 0, 0, -1, 0, 1, 0],
        }
    }

    const fn translation_y_candidate(candidate_id: u64) -> EssentialCandidate {
        EssentialCandidate {
            candidate_id,
            source: CandidateSource::TelemetryPrior,
            matrix: [0, 0, 1, 0, 0, 0, -1, 0, 0],
        }
    }

    fn policy() -> EpipolarPolicy {
        EpipolarPolicy {
            max_residual_ppm: 1_000,
            min_inliers: 8,
            min_inlier_ratio_ppm: 700_000,
            min_spatial_bins_per_image: 4,
            max_determinant_residual_ppm: 1_000,
            min_inlier_margin: 2,
            max_evaluations: 10_000,
        }
    }

    #[test]
    fn clear_epipolar_hypothesis_is_admitted_without_pose_authority() {
        let result = verify_epipolar_candidates(
            basis(1),
            policy(),
            observations(100_000),
            vec![translation_x_candidate(1), translation_y_candidate(2)],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.decision == VerificationDecision::EpipolarSupportedHypothesis
                    && value.admitted_candidate_id == Some(1)
                    && value.best_candidate_id == Some(1)
                    && value.evaluations.iter().any(|evaluation| evaluation.candidate_id == 1 && evaluation.inlier_count == 12 && evaluation.passes_admission_gates)
                    && value.evaluations.iter().any(|evaluation| evaluation.candidate_id == 2 && !evaluation.passes_admission_gates)
                    && value.validate().is_ok()
                    && matches!(value.to_json(), Ok(ref json) if json.contains("epipolar_supported_hypothesis") && !json.contains("pose_authority"))
        ));
    }

    #[test]
    fn equally_supported_distinct_candidates_are_ambiguous() {
        let result = verify_epipolar_candidates(
            basis(2),
            policy(),
            observations(0),
            vec![translation_x_candidate(1), translation_y_candidate(2)],
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.decision == VerificationDecision::Ambiguous
                    && value.best_candidate_id == Some(1)
                    && value.runner_up_candidate_id == Some(2)
                    && value.admitted_candidate_id.is_none()
                    && value.inlier_margin == 0
        ));
    }

    #[test]
    fn determinant_gate_blocks_full_rank_matrix() {
        let candidate = EssentialCandidate {
            candidate_id: 3,
            source: CandidateSource::ExternalOracle,
            matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1],
        };
        let result = verify_epipolar_candidates(basis(3), policy(), observations(0), vec![candidate]);
        assert!(matches!(
            result,
            Ok(ref value)
                if value.decision == VerificationDecision::Rejected
                    && value.admitted_candidate_id.is_none()
                    && value.evaluations[0].determinant_residual_ppm == 1_000_000
                    && !value.evaluations[0].determinant_gate_passed
        ));
    }

    #[test]
    fn homogeneous_scale_and_sign_are_canonicalized() {
        let positive = EssentialCandidate {
            candidate_id: 1,
            source: CandidateSource::NativeLinearSolver,
            matrix: [0, 0, 0, 0, 0, -5, 0, 5, 0],
        };
        let negative = EssentialCandidate {
            candidate_id: 2,
            source: CandidateSource::ModelWorker,
            matrix: [0, 0, 0, 0, 0, 9, 0, -9, 0],
        };
        assert!(matches!(
            verify_epipolar_candidates(
                basis(4),
                policy(),
                observations(100_000),
                vec![positive, negative],
            ),
            Err(EpipolarError::DuplicateCandidateMatrix {
                first_candidate_id: 1,
                duplicate_candidate_id: 2
            })
        ));
    }

    #[test]
    fn evaluation_budget_fails_before_candidate_scoring() {
        let mut bounded = policy();
        bounded.max_evaluations = 10;
        assert!(matches!(
            verify_epipolar_candidates(
                basis(5),
                bounded,
                observations(100_000),
                vec![translation_x_candidate(1)],
            ),
            Err(EpipolarError::EvaluationBudgetExceeded {
                required: 12,
                maximum: 10
            })
        ));
    }

    #[test]
    fn input_order_and_successful_budget_do_not_change_semantic_identity() {
        let observations = observations(100_000);
        let candidates = vec![translation_x_candidate(1), translation_y_candidate(2)];
        let mut reversed_observations = observations.clone();
        reversed_observations.reverse();
        let mut reversed_candidates = candidates.clone();
        reversed_candidates.reverse();
        let mut larger_budget = policy();
        larger_budget.max_evaluations = 100_000;
        let left = verify_epipolar_candidates(basis(6), policy(), observations, candidates);
        let right = verify_epipolar_candidates(
            basis(6),
            larger_budget,
            reversed_observations,
            reversed_candidates,
        );
        assert!(matches!(
            (left, right),
            (Ok(left), Ok(right))
                if left.candidates == right.candidates
                    && left.evaluations == right.evaluations
                    && left.decision == right.decision
                    && left.digest().is_ok()
                    && left.digest() == right.digest()
        ));
    }

    #[test]
    fn pair_and_duplicate_endpoint_errors_fail_closed() {
        let mut wrong_pair = observations(100_000);
        wrong_pair[0].pair_id = 9;
        assert!(matches!(
            verify_epipolar_candidates(
                basis(7),
                policy(),
                wrong_pair,
                vec![translation_x_candidate(1)],
            ),
            Err(EpipolarError::PairIdentityMismatch {
                match_id: 1,
                expected: 7,
                observed: 9
            })
        ));
        let mut duplicate = observations(100_000);
        duplicate[1].left_observation_id = duplicate[0].left_observation_id;
        duplicate[1].right_observation_id = duplicate[0].right_observation_id;
        assert!(matches!(
            verify_epipolar_candidates(
                basis(8),
                policy(),
                duplicate,
                vec![translation_x_candidate(1)],
            ),
            Err(EpipolarError::DuplicateObservationPair { .. })
        ));
    }
}
