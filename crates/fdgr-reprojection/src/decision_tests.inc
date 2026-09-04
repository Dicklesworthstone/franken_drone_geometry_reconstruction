#[cfg(test)]
mod decision_tests {
    use super::{
        ReprojectionComponentStatus, ReprojectionDecision, ReprojectionPolicy,
        ReprojectionRecommendation, ReprojectionResidualSummary, component_decision,
    };
    use fdgr_bundle_admission::BundleAdmissionDecision;

    const fn complete_summary(count: u64) -> ReprojectionResidualSummary {
        ReprojectionResidualSummary {
            eligible_observation_count: count,
            projected_observation_count: count,
            inlier_observation_count: count,
            rms_residual_nano_pixels: Some(0),
            median_residual_nano_pixels: Some(0),
            maximum_residual_nano_pixels: Some(0),
        }
    }

    #[test]
    fn upstream_diagnostic_never_becomes_positive_authority() {
        let policy = ReprojectionPolicy::default();
        let optimize = complete_summary(u64::from(
            policy.min_optimize_observations_per_component,
        ));
        let held_out = complete_summary(u64::from(
            policy.min_held_out_observations_per_component,
        ));
        let (status, decision, recommendation) = component_decision(
            BundleAdmissionDecision::AdmitDiagnostic,
            &optimize,
            &held_out,
            &[],
            &[],
            policy,
        );
        assert_eq!(status, ReprojectionComponentStatus::UpstreamDiagnostic);
        assert_eq!(decision, ReprojectionDecision::AdmitDiagnostic);
        assert_eq!(
            recommendation,
            ReprojectionRecommendation::ResolveBundleAdmission
        );
    }

    #[test]
    fn upstream_block_dominates_apparently_clean_residuals() {
        let policy = ReprojectionPolicy::default();
        let optimize = complete_summary(u64::from(
            policy.min_optimize_observations_per_component,
        ));
        let held_out = complete_summary(u64::from(
            policy.min_held_out_observations_per_component,
        ));
        let (status, decision, recommendation) = component_decision(
            BundleAdmissionDecision::Block,
            &optimize,
            &held_out,
            &[],
            &[],
            policy,
        );
        assert_eq!(status, ReprojectionComponentStatus::BlockedAdmission);
        assert_eq!(decision, ReprojectionDecision::Block);
        assert_eq!(
            recommendation,
            ReprojectionRecommendation::ResolveBundleAdmission
        );
    }
}
