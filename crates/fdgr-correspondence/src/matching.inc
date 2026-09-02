#[derive(Clone, Debug, Eq, PartialEq)]
struct ProvisionalMatch {
    pair_id: u64,
    left_observation_id: u64,
    right_observation_id: u64,
    hamming_distance: u16,
    second_best_distance: Option<u16>,
    ratio_ppm: Option<u32>,
    mutual: bool,
}

/// Builds one deterministic correspondence generation and collision-safe track graph.
///
/// # Errors
///
/// Returns a stable basis, policy, observation, pair, budget, matching, tracking, or arithmetic
/// error.
pub fn build_correspondences(
    basis: CorrespondenceBasis,
    policy: MatchPolicy,
    observations: Vec<FeatureObservation>,
    pairs: Vec<FramePair>,
) -> Result<CorrespondenceGeneration, CorrespondenceError> {
    validate_basis(&basis)?;
    validate_policy(policy)?;
    let observations = canonicalize_observations(observations)?;
    let pairs = canonicalize_pairs(pairs, &observations)?;
    let ineligible_observation_ids = observations
        .iter()
        .filter(|observation| eligibility_rejection(observation, policy).is_some())
        .map(|observation| observation.observation_id)
        .collect::<Vec<_>>();
    let mut by_sample: BTreeMap<u64, Vec<&FeatureObservation>> = BTreeMap::new();
    for observation in &observations {
        by_sample
            .entry(observation.sample_index)
            .or_default()
            .push(observation);
    }
    let mut budget = DistanceBudget {
        used: 0,
        maximum: policy.max_distance_evaluations,
    };
    let mut provisional = Vec::new();
    let mut rejected = Vec::new();
    for pair in &pairs {
        let left = by_sample.get(&pair.left_sample_index).ok_or(
            CorrespondenceError::UnknownFrameSample {
                pair_id: pair.pair_id,
                sample_index: pair.left_sample_index,
            },
        )?;
        let right = by_sample.get(&pair.right_sample_index).ok_or(
            CorrespondenceError::UnknownFrameSample {
                pair_id: pair.pair_id,
                sample_index: pair.right_sample_index,
            },
        )?;
        match_pair(
            *pair,
            policy,
            left,
            right,
            &mut budget,
            &mut provisional,
            &mut rejected,
        )?;
        if provisional.len() > MAX_MATCH_EDGES {
            return Err(CorrespondenceError::MatchLimitExceeded {
                actual: provisional.len(),
                maximum: MAX_MATCH_EDGES,
            });
        }
    }
    provisional.sort_by(|left, right| {
        left.hamming_distance
            .cmp(&right.hamming_distance)
            .then(left.pair_id.cmp(&right.pair_id))
            .then(left.left_observation_id.cmp(&right.left_observation_id))
            .then(left.right_observation_id.cmp(&right.right_observation_id))
    });
    let (accepted_matches, tracks, unmatched_observation_ids, collision_rejections) =
        assemble_tracks(&observations, provisional)?;
    rejected.extend(collision_rejections);
    rejected.sort_by(|left, right| {
        left.pair_id
            .cmp(&right.pair_id)
            .then(left.left_observation_id.cmp(&right.left_observation_id))
            .then(left.reason.code().cmp(&right.reason.code()))
            .then(
                left.candidate_right_observation_id
                    .cmp(&right.candidate_right_observation_id),
            )
    });
    Ok(CorrespondenceGeneration {
        basis,
        policy,
        observations,
        pairs,
        accepted_matches,
        rejected_matches: rejected,
        tracks,
        ineligible_observation_ids,
        unmatched_observation_ids,
        distance_evaluations: budget.used,
    })
}

fn match_pair(
    pair: FramePair,
    policy: MatchPolicy,
    left: &[&FeatureObservation],
    right: &[&FeatureObservation],
    budget: &mut DistanceBudget,
    provisional: &mut Vec<ProvisionalMatch>,
    rejected: &mut Vec<MatchRejection>,
) -> Result<(), CorrespondenceError> {
    let eligible_left = left
        .iter()
        .copied()
        .filter(|observation| eligibility_rejection(observation, policy).is_none())
        .collect::<Vec<_>>();
    let eligible_right = right
        .iter()
        .copied()
        .filter(|observation| eligibility_rejection(observation, policy).is_none())
        .collect::<Vec<_>>();
    let mut reverse_cache: BTreeMap<u64, Nearest> = BTreeMap::new();
    for left_observation in left {
        if let Some(reason) = eligibility_rejection(left_observation, policy) {
            rejected.push(MatchRejection {
                pair_id: pair.pair_id,
                left_observation_id: left_observation.observation_id,
                candidate_right_observation_id: None,
                reason,
                best_distance: None,
                second_best_distance: None,
            });
            continue;
        }
        let Some(nearest) = nearest_descriptor(
            left_observation.descriptor,
            &eligible_right,
            budget,
        )? else {
            rejected.push(MatchRejection {
                pair_id: pair.pair_id,
                left_observation_id: left_observation.observation_id,
                candidate_right_observation_id: None,
                reason: MatchRejectionReason::NoEligibleRight,
                best_distance: None,
                second_best_distance: None,
            });
            continue;
        };
        if nearest.best_tied {
            rejected.push(rejection_from_nearest(
                pair.pair_id,
                left_observation.observation_id,
                nearest,
                MatchRejectionReason::NearestTie,
            ));
            continue;
        }
        if policy.require_second_best && nearest.second_best_distance.is_none() {
            rejected.push(rejection_from_nearest(
                pair.pair_id,
                left_observation.observation_id,
                nearest,
                MatchRejectionReason::SecondBestUnavailable,
            ));
            continue;
        }
        if nearest.best_distance > policy.max_hamming_distance {
            rejected.push(rejection_from_nearest(
                pair.pair_id,
                left_observation.observation_id,
                nearest,
                MatchRejectionReason::DistanceExceeded,
            ));
            continue;
        }
        let ratio = nearest
            .second_best_distance
            .map(|second| ratio_ppm(nearest.best_distance, second))
            .transpose()?;
        if ratio.is_some_and(|value| value > policy.ratio_threshold_ppm) {
            rejected.push(rejection_from_nearest(
                pair.pair_id,
                left_observation.observation_id,
                nearest,
                MatchRejectionReason::RatioFailed,
            ));
            continue;
        }
        let mutual = if policy.require_mutual {
            let reverse = match reverse_cache.get(&nearest.best_observation_id).copied() {
                Some(value) => value,
                None => {
                    let right_observation = observation_by_id(
                        &eligible_right,
                        nearest.best_observation_id,
                    )?;
                    let value = nearest_descriptor(
                        right_observation.descriptor,
                        &eligible_left,
                        budget,
                    )?
                    .ok_or(CorrespondenceError::UnknownObservation {
                        observation_id: left_observation.observation_id,
                    })?;
                    reverse_cache.insert(nearest.best_observation_id, value);
                    value
                }
            };
            if reverse.best_tied
                || reverse.best_observation_id != left_observation.observation_id
            {
                rejected.push(rejection_from_nearest(
                    pair.pair_id,
                    left_observation.observation_id,
                    nearest,
                    MatchRejectionReason::MutualFailed,
                ));
                continue;
            }
            true
        } else {
            false
        };
        provisional.push(ProvisionalMatch {
            pair_id: pair.pair_id,
            left_observation_id: left_observation.observation_id,
            right_observation_id: nearest.best_observation_id,
            hamming_distance: nearest.best_distance,
            second_best_distance: nearest.second_best_distance,
            ratio_ppm: ratio,
            mutual,
        });
    }
    Ok(())
}

fn observation_by_id<'a>(
    observations: &[&'a FeatureObservation],
    observation_id: u64,
) -> Result<&'a FeatureObservation, CorrespondenceError> {
    observations
        .iter()
        .copied()
        .find(|observation| observation.observation_id == observation_id)
        .ok_or(CorrespondenceError::UnknownObservation { observation_id })
}

fn rejection_from_nearest(
    pair_id: u64,
    left_observation_id: u64,
    nearest: Nearest,
    reason: MatchRejectionReason,
) -> MatchRejection {
    MatchRejection {
        pair_id,
        left_observation_id,
        candidate_right_observation_id: Some(nearest.best_observation_id),
        reason,
        best_distance: Some(nearest.best_distance),
        second_best_distance: nearest.second_best_distance,
    }
}
