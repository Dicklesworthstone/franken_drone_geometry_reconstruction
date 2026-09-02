#[cfg(test)]
mod tests {
    use super::{
        EDGE_SCALE_GENERATION_SCHEMA, EdgeRatioSource, EdgeRatioWitness, EdgeScaleBasis,
        EdgeScaleComponentStatus, EdgeScalePolicy, EdgeScaleSubject, RelationStatus,
        WitnessDisposition, edge_subject_table_digest, edge_witness_table_digest,
        reconcile_edge_scales,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis(
        byte: u8,
        subjects: &[EdgeScaleSubject],
        witnesses: &[EdgeRatioWitness],
    ) -> Result<EdgeScaleBasis, super::EdgeScaleError> {
        Ok(EdgeScaleBasis {
            pose_graph_generation_digest: digest(byte),
            edge_subject_basis_digest: edge_subject_table_digest(subjects)?,
            witness_basis_digest: edge_witness_table_digest(subjects, witnesses)?,
            policy_digest: digest(22),
            generation: 1,
        })
    }

    fn subject(edge_id: u64, pose_component_root_node_id: u64) -> EdgeScaleSubject {
        EdgeScaleSubject {
            edge_id,
            pose_verification_digest: digest(u8::try_from(edge_id).map_or(255, |value| value)),
            pose_component_root_node_id,
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn witness(
        witness_id: u64,
        group: u64,
        lower: u64,
        higher: u64,
        numerator: u64,
        denominator: u64,
        source: EdgeRatioSource,
    ) -> EdgeRatioWitness {
        EdgeRatioWitness {
            witness_id,
            evidence_digest: digest(u8::try_from(witness_id.saturating_add(30)).map_or(254, |value| value)),
            correlation_group_id: group,
            lower_edge_id: lower,
            higher_edge_id: higher,
            ratio_numerator: numerator,
            ratio_denominator: denominator,
            uncertainty_ppm: 1_000,
            support_count: 10,
            source,
        }
    }

    fn policy() -> EdgeScalePolicy {
        EdgeScalePolicy {
            min_cross_validation_groups: 1,
            ..EdgeScalePolicy::default()
        }
    }

    #[test]
    fn chain_propagates_one_arbitrary_component_gauge() -> Result<(), super::EdgeScaleError> {
        let subjects = vec![subject(10, 1), subject(20, 1), subject(30, 1)];
        let witnesses = vec![
            witness(1, 101, 10, 20, 1, 2, EdgeRatioSource::SharedTrackGeometry),
            witness(2, 102, 20, 30, 2, 1, EdgeRatioSource::MultiViewGeometry),
        ];
        let result = reconcile_edge_scales(
            basis(1, &subjects, &witnesses)?,
            policy(),
            subjects,
            witnesses,
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.scales.iter().find(|scale| scale.edge_id == 10).is_some_and(|scale| scale.relative_scale_nano == 1_000_000_000)
                    && value.scales.iter().find(|scale| scale.edge_id == 20).is_some_and(|scale| scale.relative_scale_nano == 2_000_000_000)
                    && value.scales.iter().find(|scale| scale.edge_id == 30).is_some_and(|scale| scale.relative_scale_nano == 1_000_000_000)
                    && value.components.len() == 1
                    && value.components[0].status == EdgeScaleComponentStatus::TreeOnly
                    && value.components[0].cross_validated
                    && value.validate().is_ok()
                    && matches!(value.to_json(), Ok(ref json) if json.contains(EDGE_SCALE_GENERATION_SCHEMA) && json.contains("component_edge_scale_unit"))
        ));
        Ok(())
    }

    #[test]
    fn internally_conflicting_correlation_group_is_excluded_as_a_whole() -> Result<(), super::EdgeScaleError> {
        let subjects = vec![subject(10, 1), subject(20, 1)];
        let witnesses = vec![
            witness(1, 100, 10, 20, 1, 2, EdgeRatioSource::SharedTrackGeometry),
            witness(2, 100, 10, 20, 2, 1, EdgeRatioSource::SharedTrackGeometry),
        ];
        let result = reconcile_edge_scales(
            basis(2, &subjects, &witnesses)?,
            policy(),
            subjects,
            witnesses,
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.relations.is_empty()
                    && value.components.len() == 2
                    && value.components.iter().all(|component| component.status == EdgeScaleComponentStatus::Isolated)
                    && value.witness_assessments.iter().all(|assessment| assessment.disposition == WitnessDisposition::CorrelatedConflict)
        ));
        Ok(())
    }

    #[test]
    fn independent_consensus_retains_outlier_evidence() -> Result<(), super::EdgeScaleError> {
        let subjects = vec![subject(10, 1), subject(20, 1)];
        let witnesses = vec![
            witness(1, 101, 10, 20, 1, 2, EdgeRatioSource::SharedTrackGeometry),
            witness(2, 102, 10, 20, 1, 2, EdgeRatioSource::TelemetryBaseline),
            witness(3, 103, 10, 20, 2, 1, EdgeRatioSource::ModelPrior),
        ];
        let result = reconcile_edge_scales(
            basis(3, &subjects, &witnesses)?,
            policy(),
            subjects,
            witnesses,
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.relations.len() == 1
                    && value.relations[0].ratio_lower_over_higher_nano == 500_000_000
                    && value.relations[0].supporting_correlation_group_ids == vec![101, 102]
                    && value.relations[0].rejected_witness_ids == vec![3]
                    && value.witness_assessments.iter().any(|assessment| assessment.witness_id == 3 && assessment.disposition == WitnessDisposition::ConsensusOutlier)
        ));
        Ok(())
    }

    #[test]
    fn contradictory_redundant_ratio_cycle_demotes_component() -> Result<(), super::EdgeScaleError> {
        let subjects = vec![subject(10, 1), subject(20, 1), subject(30, 1)];
        let witnesses = vec![
            witness(1, 101, 10, 20, 1, 2, EdgeRatioSource::SharedTrackGeometry),
            witness(2, 102, 20, 30, 1, 2, EdgeRatioSource::SharedTrackGeometry),
            witness(3, 103, 10, 30, 1, 2, EdgeRatioSource::SharedTrackGeometry),
        ];
        let result = reconcile_edge_scales(
            basis(4, &subjects, &witnesses)?,
            policy(),
            subjects,
            witnesses,
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.components.len() == 1
                    && value.components[0].status == EdgeScaleComponentStatus::Conflicted
                    && !value.components[0].cross_validated
                    && value.relation_assessments.iter().any(|assessment| assessment.status == RelationStatus::Conflicting)
        ));
        Ok(())
    }

    #[test]
    fn cross_pose_component_witness_does_not_join_incomparable_gauges() -> Result<(), super::EdgeScaleError> {
        let subjects = vec![subject(10, 1), subject(20, 2)];
        let witnesses = vec![witness(
            1,
            101,
            10,
            20,
            1,
            1,
            EdgeRatioSource::ExternalOracle,
        )];
        let result = reconcile_edge_scales(
            basis(5, &subjects, &witnesses)?,
            policy(),
            subjects,
            witnesses,
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.relations.is_empty()
                    && value.components.len() == 2
                    && value.witness_assessments[0].disposition == WitnessDisposition::CrossPoseComponent
        ));
        Ok(())
    }

    #[test]
    fn input_order_and_larger_successful_budget_preserve_semantic_identity() -> Result<(), super::EdgeScaleError> {
        let subjects = vec![subject(10, 1), subject(20, 1), subject(30, 1)];
        let witnesses = vec![
            witness(1, 101, 10, 20, 1, 2, EdgeRatioSource::SharedTrackGeometry),
            witness(2, 102, 20, 30, 2, 1, EdgeRatioSource::MultiViewGeometry),
        ];
        let mut reversed_subjects = subjects.clone();
        reversed_subjects.reverse();
        let mut reversed_witnesses = witnesses.clone();
        reversed_witnesses.reverse();
        let mut larger_budget = policy();
        larger_budget.max_path_expansions = 100_000_000;
        let exact_basis = basis(6, &subjects, &witnesses)?;
        let left = reconcile_edge_scales(
            exact_basis.clone(),
            policy(),
            subjects,
            witnesses,
        );
        let right = reconcile_edge_scales(
            exact_basis,
            larger_budget,
            reversed_subjects,
            reversed_witnesses,
        );
        assert!(matches!(
            (left, right),
            (Ok(left), Ok(right))
                if left.relations == right.relations
                    && left.scales == right.scales
                    && left.components == right.components
                    && left.digest().is_ok()
                    && left.digest() == right.digest()
        ));
        Ok(())
    }
}
