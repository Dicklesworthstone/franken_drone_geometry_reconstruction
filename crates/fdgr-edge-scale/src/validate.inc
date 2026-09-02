fn canonicalize_witnesses(
    mut witnesses: Vec<EdgeRatioWitness>,
    subjects: &[EdgeScaleSubject],
) -> Result<Vec<EdgeRatioWitness>, EdgeScaleError> {
    if witnesses.len() > MAX_SCALE_WITNESSES {
        return Err(EdgeScaleError::BoundExceeded {
            field: "witness_count",
            actual: usize_to_u128(witnesses.len()),
            maximum: usize_to_u128(MAX_SCALE_WITNESSES),
        });
    }
    let edge_ids = subjects.iter().map(|subject| subject.edge_id).collect::<BTreeSet<_>>();
    witnesses.sort_by_key(|witness| witness.witness_id);
    let mut witness_ids = BTreeSet::new();
    for witness in &mut witnesses {
        validate_safe_nonzero("witness_id", witness.witness_id)?;
        validate_safe_nonzero("correlation_group_id", witness.correlation_group_id)?;
        reject_zero_digest("witness_evidence_digest", &witness.evidence_digest)?;
        if !witness_ids.insert(witness.witness_id) {
            return Err(EdgeScaleError::DuplicateWitness { witness_id: witness.witness_id });
        }
        if witness.lower_edge_id >= witness.higher_edge_id {
            return Err(EdgeScaleError::NonCanonicalWitness { witness_id: witness.witness_id });
        }
        for edge_id in [witness.lower_edge_id, witness.higher_edge_id] {
            if !edge_ids.contains(&edge_id) {
                return Err(EdgeScaleError::UnknownEdge {
                    witness_id: witness.witness_id,
                    edge_id,
                });
            }
        }
        let (numerator, denominator) =
            normalized_fraction(witness.ratio_numerator, witness.ratio_denominator)?;
        if numerator > MAX_RATIO_TERM || denominator > MAX_RATIO_TERM {
            return Err(EdgeScaleError::BoundExceeded {
                field: "normalized_ratio_term",
                actual: u128::from(numerator.max(denominator)),
                maximum: u128::from(MAX_RATIO_TERM),
            });
        }
        let ratio_nano = fraction_to_nano(numerator, denominator)
            .map_err(|_| EdgeScaleError::UnrepresentableRatio {
                witness_id: witness.witness_id,
            })?;
        let inverse_ratio_nano = fraction_to_nano(denominator, numerator)
            .map_err(|_| EdgeScaleError::UnrepresentableRatio {
                witness_id: witness.witness_id,
            })?;
        if ratio_nano == 0
            || inverse_ratio_nano == 0
            || ratio_nano > MAX_RELATIVE_SCALE_NANO
            || inverse_ratio_nano > MAX_RELATIVE_SCALE_NANO
        {
            return Err(EdgeScaleError::UnrepresentableRatio {
                witness_id: witness.witness_id,
            });
        }
        witness.ratio_numerator = numerator;
        witness.ratio_denominator = denominator;
        if u64::from(witness.uncertainty_ppm) > MAX_SCALE_RESIDUAL_PPM {
            return Err(EdgeScaleError::BoundExceeded {
                field: "uncertainty_ppm",
                actual: u128::from(witness.uncertainty_ppm),
                maximum: u128::from(MAX_SCALE_RESIDUAL_PPM),
            });
        }
        if witness.support_count == 0 {
            return Err(EdgeScaleError::ZeroValue { field: "support_count" });
        }
    }
    Ok(witnesses)
}
