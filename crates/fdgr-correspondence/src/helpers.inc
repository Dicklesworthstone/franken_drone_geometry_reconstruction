fn validate_basis(basis: &CorrespondenceBasis) -> Result<(), CorrespondenceError> {
    for (field, digest) in [
        ("keyframe_selection_digest", &basis.keyframe_selection_digest),
        ("calibration_digest", &basis.calibration_digest),
        ("feature_basis_digest", &basis.feature_basis_digest),
        ("pair_basis_digest", &basis.pair_basis_digest),
        ("policy_digest", &basis.policy_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    validate_safe_identity("generation", basis.generation)
}

fn validate_policy(policy: MatchPolicy) -> Result<(), CorrespondenceError> {
    if policy.max_hamming_distance > DESCRIPTOR_BITS {
        return Err(CorrespondenceError::BoundExceeded {
            field: "max_hamming_distance",
            actual: u128::from(policy.max_hamming_distance),
            maximum: u128::from(DESCRIPTOR_BITS),
        });
    }
    if policy.ratio_threshold_ppm == 0 {
        return Err(CorrespondenceError::ZeroValue {
            field: "ratio_threshold_ppm",
        });
    }
    validate_ppm("ratio_threshold_ppm", policy.ratio_threshold_ppm)?;
    validate_ppm("min_response_ppm", policy.min_response_ppm)?;
    if policy.max_uncertainty_nano_pixels == 0 {
        return Err(CorrespondenceError::ZeroValue {
            field: "max_uncertainty_nano_pixels",
        });
    }
    if policy.max_uncertainty_nano_pixels > MAX_UNCERTAINTY_NANO_PIXELS {
        return Err(CorrespondenceError::BoundExceeded {
            field: "max_uncertainty_nano_pixels",
            actual: u128::from(policy.max_uncertainty_nano_pixels),
            maximum: u128::from(MAX_UNCERTAINTY_NANO_PIXELS),
        });
    }
    if policy.max_distance_evaluations == 0 {
        return Err(CorrespondenceError::ZeroValue {
            field: "max_distance_evaluations",
        });
    }
    if policy.max_distance_evaluations > MAX_DISTANCE_EVALUATIONS {
        return Err(CorrespondenceError::BoundExceeded {
            field: "max_distance_evaluations",
            actual: u128::from(policy.max_distance_evaluations),
            maximum: u128::from(MAX_DISTANCE_EVALUATIONS),
        });
    }
    Ok(())
}

fn canonicalize_observations(
    mut observations: Vec<FeatureObservation>,
) -> Result<Vec<FeatureObservation>, CorrespondenceError> {
    if observations.is_empty() {
        return Err(CorrespondenceError::EmptyObservations);
    }
    if observations.len() > MAX_FEATURE_OBSERVATIONS {
        return Err(CorrespondenceError::BoundExceeded {
            field: "observation_count",
            actual: usize_to_u128(observations.len()),
            maximum: usize_to_u128(MAX_FEATURE_OBSERVATIONS),
        });
    }
    observations.sort_by(|left, right| {
        left.observation_id
            .cmp(&right.observation_id)
            .then(left.sample_index.cmp(&right.sample_index))
            .then(left.feature_id.cmp(&right.feature_id))
    });
    let mut observation_ids = BTreeSet::new();
    let mut frame_features = BTreeSet::new();
    let mut frame_digests: BTreeMap<u64, EvidenceDigest> = BTreeMap::new();
    let mut counts: BTreeMap<u64, usize> = BTreeMap::new();
    for observation in &observations {
        validate_observation(observation)?;
        if !observation_ids.insert(observation.observation_id) {
            return Err(CorrespondenceError::DuplicateObservationId {
                observation_id: observation.observation_id,
            });
        }
        if !frame_features.insert((observation.sample_index, observation.feature_id)) {
            return Err(CorrespondenceError::DuplicateFeatureInFrame {
                sample_index: observation.sample_index,
                feature_id: observation.feature_id,
            });
        }
        match frame_digests.get(&observation.sample_index) {
            Some(digest) if digest != &observation.frame_digest => {
                return Err(CorrespondenceError::FrameIdentityConflict {
                    sample_index: observation.sample_index,
                });
            }
            None => {
                frame_digests.insert(observation.sample_index, observation.frame_digest.clone());
            }
            Some(_) => {}
        }
        let count = counts.entry(observation.sample_index).or_default();
        *count = count
            .checked_add(1)
            .ok_or(CorrespondenceError::ArithmeticOverflow {
                field: "features_per_frame",
            })?;
        if *count > MAX_FEATURES_PER_FRAME {
            return Err(CorrespondenceError::BoundExceeded {
                field: "features_per_frame",
                actual: usize_to_u128(*count),
                maximum: usize_to_u128(MAX_FEATURES_PER_FRAME),
            });
        }
    }
    Ok(observations)
}

fn validate_observation(
    observation: &FeatureObservation,
) -> Result<(), CorrespondenceError> {
    validate_safe_identity("observation_id", observation.observation_id)?;
    validate_safe_number("sample_index", observation.sample_index)?;
    reject_zero_digest("frame_digest", &observation.frame_digest)?;
    if observation.feature_id == 0 {
        return Err(CorrespondenceError::ZeroValue {
            field: "feature_id",
        });
    }
    for (field, value) in [
        ("x_nano_pixels", observation.x_nano_pixels),
        ("y_nano_pixels", observation.y_nano_pixels),
    ] {
        if value.unsigned_abs() > MAX_ABS_NANO_PIXEL.unsigned_abs() {
            return Err(CorrespondenceError::BoundExceeded {
                field,
                actual: value.unsigned_abs().into(),
                maximum: MAX_ABS_NANO_PIXEL.unsigned_abs().into(),
            });
        }
    }
    validate_ppm("response_ppm", observation.response_ppm)?;
    if observation.uncertainty_nano_pixels > MAX_UNCERTAINTY_NANO_PIXELS {
        return Err(CorrespondenceError::BoundExceeded {
            field: "uncertainty_nano_pixels",
            actual: u128::from(observation.uncertainty_nano_pixels),
            maximum: u128::from(MAX_UNCERTAINTY_NANO_PIXELS),
        });
    }
    Ok(())
}

fn canonicalize_pairs(
    mut pairs: Vec<FramePair>,
    observations: &[FeatureObservation],
) -> Result<Vec<FramePair>, CorrespondenceError> {
    if pairs.is_empty() {
        return Err(CorrespondenceError::EmptyPairs);
    }
    if pairs.len() > MAX_FRAME_PAIRS {
        return Err(CorrespondenceError::BoundExceeded {
            field: "frame_pair_count",
            actual: usize_to_u128(pairs.len()),
            maximum: usize_to_u128(MAX_FRAME_PAIRS),
        });
    }
    pairs.sort_by(|left, right| {
        left.pair_id
            .cmp(&right.pair_id)
            .then(left.left_sample_index.cmp(&right.left_sample_index))
            .then(left.right_sample_index.cmp(&right.right_sample_index))
    });
    let known_samples = observations
        .iter()
        .map(|observation| observation.sample_index)
        .collect::<BTreeSet<_>>();
    let mut pair_ids = BTreeSet::new();
    let mut sample_pairs = BTreeSet::new();
    for pair in &pairs {
        validate_safe_identity("pair_id", pair.pair_id)?;
        validate_safe_number("left_sample_index", pair.left_sample_index)?;
        validate_safe_number("right_sample_index", pair.right_sample_index)?;
        if pair.left_sample_index >= pair.right_sample_index {
            return Err(CorrespondenceError::NonCanonicalFramePair {
                pair_id: pair.pair_id,
            });
        }
        if !pair_ids.insert(pair.pair_id) {
            return Err(CorrespondenceError::DuplicatePairId {
                pair_id: pair.pair_id,
            });
        }
        if !sample_pairs.insert((pair.left_sample_index, pair.right_sample_index)) {
            return Err(CorrespondenceError::DuplicateFramePair {
                left_sample_index: pair.left_sample_index,
                right_sample_index: pair.right_sample_index,
            });
        }
        for sample_index in [pair.left_sample_index, pair.right_sample_index] {
            if !known_samples.contains(&sample_index) {
                return Err(CorrespondenceError::UnknownFrameSample {
                    pair_id: pair.pair_id,
                    sample_index,
                });
            }
        }
    }
    Ok(pairs)
}

fn eligibility_rejection(
    observation: &FeatureObservation,
    policy: MatchPolicy,
) -> Option<MatchRejectionReason> {
    if observation.response_ppm < policy.min_response_ppm {
        Some(MatchRejectionReason::ResponseBelowMinimum)
    } else if observation.uncertainty_nano_pixels > policy.max_uncertainty_nano_pixels {
        Some(MatchRejectionReason::UncertaintyExceeded)
    } else if policy.reject_dynamic_masked && observation.dynamic_masked {
        Some(MatchRejectionReason::DynamicMasked)
    } else {
        None
    }
}

fn hamming_distance(left: Descriptor256, right: Descriptor256) -> u16 {
    left.0
        .iter()
        .zip(right.0.iter())
        .map(|(left_word, right_word)| (left_word ^ right_word).count_ones())
        .fold(0_u16, |total, value| {
            let converted = match u16::try_from(value) {
                Ok(converted) => converted,
                Err(_) => 0,
            };
            total.saturating_add(converted)
        })
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct Nearest {
    best_observation_id: u64,
    best_distance: u16,
    second_best_distance: Option<u16>,
    best_tied: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct DistanceBudget {
    used: u64,
    maximum: u64,
}

impl DistanceBudget {
    fn charge(&mut self) -> Result<(), CorrespondenceError> {
        let attempted = self
            .used
            .checked_add(1)
            .ok_or(CorrespondenceError::ArithmeticOverflow {
                field: "distance_evaluations",
            })?;
        if attempted > self.maximum {
            return Err(CorrespondenceError::DistanceBudgetExceeded {
                attempted,
                maximum: self.maximum,
            });
        }
        self.used = attempted;
        Ok(())
    }
}

fn nearest_descriptor(
    query: Descriptor256,
    candidates: &[&FeatureObservation],
    budget: &mut DistanceBudget,
) -> Result<Option<Nearest>, CorrespondenceError> {
    let mut best: Option<(u16, u64)> = None;
    let mut second: Option<(u16, u64)> = None;
    for candidate in candidates {
        budget.charge()?;
        let value = (hamming_distance(query, candidate.descriptor), candidate.observation_id);
        if best.is_none_or(|current| value < current) {
            second = best;
            best = Some(value);
        } else if second.is_none_or(|current| value < current) {
            second = Some(value);
        }
    }
    let Some((best_distance, best_observation_id)) = best else {
        return Ok(None);
    };
    Ok(Some(Nearest {
        best_observation_id,
        best_distance,
        second_best_distance: second.map(|value| value.0),
        best_tied: second.is_some_and(|value| value.0 == best_distance),
    }))
}

fn ratio_ppm(best: u16, second: u16) -> Result<u32, CorrespondenceError> {
    if second == 0 {
        return Ok(PPM_SCALE);
    }
    let numerator = u128::from(best)
        .checked_mul(u128::from(PPM_SCALE))
        .ok_or(CorrespondenceError::ArithmeticOverflow {
            field: "ratio_ppm",
        })?;
    let denominator = u128::from(second);
    let rounded_up = numerator
        .checked_add(denominator.saturating_sub(1))
        .ok_or(CorrespondenceError::ArithmeticOverflow {
            field: "ratio_ppm",
        })?
        / denominator;
    u32::try_from(rounded_up).map_err(|_| CorrespondenceError::BoundExceeded {
        field: "ratio_ppm",
        actual: rounded_up,
        maximum: u128::from(u32::MAX),
    })
}

fn validate_ppm(field: &'static str, value: u32) -> Result<(), CorrespondenceError> {
    if value > PPM_SCALE {
        return Err(CorrespondenceError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(PPM_SCALE),
        });
    }
    Ok(())
}

fn validate_safe_identity(field: &'static str, value: u64) -> Result<(), CorrespondenceError> {
    if value == 0 {
        return Err(CorrespondenceError::ZeroValue { field });
    }
    validate_safe_number(field, value)
}

fn validate_safe_number(field: &'static str, value: u64) -> Result<(), CorrespondenceError> {
    if value > MAX_SAFE_JSON_INTEGER {
        return Err(CorrespondenceError::BoundExceeded {
            field,
            actual: u128::from(value),
            maximum: u128::from(MAX_SAFE_JSON_INTEGER),
        });
    }
    Ok(())
}

fn reject_zero_digest(
    field: &'static str,
    digest: &EvidenceDigest,
) -> Result<(), CorrespondenceError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(CorrespondenceError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn usize_to_u64(value: usize) -> Result<u64, CorrespondenceError> {
    u64::try_from(value).map_err(|_| CorrespondenceError::ArithmeticOverflow {
        field: "collection_length",
    })
}

const fn usize_to_u128(value: usize) -> u128 {
    value as u128
}

fn encode_optional_u16(encoder: &mut Encoder, value: Option<u16>) {
    match value {
        Some(value) => {
            encoder.put_bool(true);
            encoder.put_u16(value);
        }
        None => encoder.put_bool(false),
    }
}

fn encode_optional_u32(encoder: &mut Encoder, value: Option<u32>) {
    match value {
        Some(value) => {
            encoder.put_bool(true);
            encoder.put_u32(value);
        }
        None => encoder.put_bool(false),
    }
}

fn encode_optional_u64(encoder: &mut Encoder, value: Option<u64>) {
    match value {
        Some(value) => {
            encoder.put_bool(true);
            encoder.put_u64(value);
        }
        None => encoder.put_bool(false),
    }
}

fn optional_u16_json(value: Option<u16>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn optional_u32_json(value: Option<u32>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn json_rendering(error: std::fmt::Error) -> CorrespondenceError {
    CorrespondenceError::JsonRendering(error.to_string())
}
