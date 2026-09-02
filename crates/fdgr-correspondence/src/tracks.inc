fn assemble_tracks(
    observations: &[FeatureObservation],
    provisional: Vec<ProvisionalMatch>,
) -> Result<
    (
        Vec<PairwiseMatch>,
        Vec<FeatureTrack>,
        Vec<u64>,
        Vec<MatchRejection>,
    ),
    CorrespondenceError,
> {
    let mut observation_indices = BTreeMap::new();
    let mut parent = Vec::with_capacity(observations.len());
    let mut component_samples = Vec::with_capacity(observations.len());
    for (index, observation) in observations.iter().enumerate() {
        observation_indices.insert(observation.observation_id, index);
        parent.push(index);
        component_samples.push(BTreeSet::from([observation.sample_index]));
    }
    let mut accepted = Vec::new();
    let mut collision_rejections = Vec::new();
    for edge in provisional {
        let left_index = *observation_indices
            .get(&edge.left_observation_id)
            .ok_or(CorrespondenceError::UnknownObservation {
                observation_id: edge.left_observation_id,
            })?;
        let right_index = *observation_indices
            .get(&edge.right_observation_id)
            .ok_or(CorrespondenceError::UnknownObservation {
                observation_id: edge.right_observation_id,
            })?;
        let left_root = find_root(&mut parent, left_index)?;
        let right_root = find_root(&mut parent, right_index)?;
        if left_root == right_root {
            accepted.push(pairwise_match(edge, true));
            continue;
        }
        if components_overlap(&component_samples, left_root, right_root)? {
            collision_rejections.push(MatchRejection {
                pair_id: edge.pair_id,
                left_observation_id: edge.left_observation_id,
                candidate_right_observation_id: Some(edge.right_observation_id),
                reason: MatchRejectionReason::FrameCollision,
                best_distance: Some(edge.hamming_distance),
                second_best_distance: edge.second_best_distance,
            });
            continue;
        }
        union_components(
            &mut parent,
            &mut component_samples,
            left_root,
            right_root,
        )?;
        accepted.push(pairwise_match(edge, false));
        if accepted.len() > MAX_MATCH_EDGES {
            return Err(CorrespondenceError::MatchLimitExceeded {
                actual: accepted.len(),
                maximum: MAX_MATCH_EDGES,
            });
        }
    }

    let mut root_by_observation = BTreeMap::new();
    let mut components: BTreeMap<usize, Vec<u64>> = BTreeMap::new();
    for (index, observation) in observations.iter().enumerate() {
        let root = find_root(&mut parent, index)?;
        root_by_observation.insert(observation.observation_id, root);
        components
            .entry(root)
            .or_default()
            .push(observation.observation_id);
    }
    let sample_by_observation = observations
        .iter()
        .map(|observation| (observation.observation_id, observation.sample_index))
        .collect::<BTreeMap<_, _>>();
    let mut pair_ids_by_root: BTreeMap<usize, BTreeSet<u64>> = BTreeMap::new();
    let mut edge_count_by_root: BTreeMap<usize, u32> = BTreeMap::new();
    for edge in &accepted {
        let root = *root_by_observation
            .get(&edge.left_observation_id)
            .ok_or(CorrespondenceError::UnknownObservation {
                observation_id: edge.left_observation_id,
            })?;
        pair_ids_by_root
            .entry(root)
            .or_default()
            .insert(edge.pair_id);
        let count = edge_count_by_root.entry(root).or_default();
        *count = count
            .checked_add(1)
            .ok_or(CorrespondenceError::ArithmeticOverflow {
                field: "track_edge_count",
            })?;
    }
    let mut tracks = Vec::new();
    let mut unmatched = Vec::new();
    for (root, observation_ids) in components {
        if observation_ids.len() < 2 {
            unmatched.extend(observation_ids);
            continue;
        }
        let mut sample_indices = Vec::with_capacity(observation_ids.len());
        for observation_id in &observation_ids {
            sample_indices.push(*sample_by_observation.get(observation_id).ok_or(
                CorrespondenceError::UnknownObservation {
                    observation_id: *observation_id,
                },
            )?);
        }
        sample_indices.sort_unstable();
        if sample_indices.windows(2).any(|pair| {
            matches!(pair, [left, right] if left == right)
        }) {
            return Err(CorrespondenceError::DerivedMismatch {
                field: "track_sample_uniqueness",
            });
        }
        let track_id = u64::try_from(tracks.len().saturating_add(1)).map_err(|_| {
            CorrespondenceError::ArithmeticOverflow { field: "track_id" }
        })?;
        tracks.push(FeatureTrack {
            track_id,
            observation_ids,
            sample_indices,
            supporting_pair_ids: pair_ids_by_root
                .remove(&root)
                .map_or_else(Vec::new, |values| values.into_iter().collect()),
            edge_count: edge_count_by_root.remove(&root).map_or(0, |value| value),
        });
    }
    unmatched.sort_unstable();
    Ok((accepted, tracks, unmatched, collision_rejections))
}

fn pairwise_match(edge: ProvisionalMatch, creates_cycle: bool) -> PairwiseMatch {
    PairwiseMatch {
        pair_id: edge.pair_id,
        left_observation_id: edge.left_observation_id,
        right_observation_id: edge.right_observation_id,
        hamming_distance: edge.hamming_distance,
        second_best_distance: edge.second_best_distance,
        ratio_ppm: edge.ratio_ppm,
        mutual: edge.mutual,
        creates_cycle,
    }
}

fn find_root(
    parent: &mut [usize],
    mut index: usize,
) -> Result<usize, CorrespondenceError> {
    loop {
        let current = *parent
            .get(index)
            .ok_or(CorrespondenceError::ArithmeticOverflow {
                field: "union_find_index",
            })?;
        if current == index {
            return Ok(index);
        }
        let grandparent = *parent
            .get(current)
            .ok_or(CorrespondenceError::ArithmeticOverflow {
                field: "union_find_parent",
            })?;
        let slot = parent
            .get_mut(index)
            .ok_or(CorrespondenceError::ArithmeticOverflow {
                field: "union_find_index",
            })?;
        *slot = grandparent;
        index = grandparent;
    }
}

fn components_overlap(
    component_samples: &[BTreeSet<u64>],
    left_root: usize,
    right_root: usize,
) -> Result<bool, CorrespondenceError> {
    let left = component_samples
        .get(left_root)
        .ok_or(CorrespondenceError::ArithmeticOverflow {
            field: "left_component",
        })?;
    let right = component_samples
        .get(right_root)
        .ok_or(CorrespondenceError::ArithmeticOverflow {
            field: "right_component",
        })?;
    Ok(if left.len() <= right.len() {
        left.iter().any(|sample| right.contains(sample))
    } else {
        right.iter().any(|sample| left.contains(sample))
    })
}

fn union_components(
    parent: &mut [usize],
    component_samples: &mut [BTreeSet<u64>],
    left_root: usize,
    right_root: usize,
) -> Result<(), CorrespondenceError> {
    let (keep, drop) = if left_root <= right_root {
        (left_root, right_root)
    } else {
        (right_root, left_root)
    };
    let parent_slot = parent
        .get_mut(drop)
        .ok_or(CorrespondenceError::ArithmeticOverflow {
            field: "union_parent",
        })?;
    *parent_slot = keep;
    let dropped = {
        let dropped_slot = component_samples
            .get_mut(drop)
            .ok_or(CorrespondenceError::ArithmeticOverflow {
                field: "union_drop_component",
            })?;
        std::mem::take(dropped_slot)
    };
    let kept = component_samples
        .get_mut(keep)
        .ok_or(CorrespondenceError::ArithmeticOverflow {
            field: "union_keep_component",
        })?;
    kept.extend(dropped);
    Ok(())
}
