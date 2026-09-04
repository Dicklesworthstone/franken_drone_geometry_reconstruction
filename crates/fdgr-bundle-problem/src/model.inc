impl BundleProblemGeneration {
    /// Replays every derived field from the retained exact inputs.
    ///
    /// # Errors
    ///
    /// Returns a stable basis, upstream, structural, graph, budget, or deterministic mismatch.
    pub fn validate(&self) -> Result<(), BundleProblemError> {
        let rebuilt = compile_bundle_problem(
            self.basis.clone(),
            self.policy,
            self.pose_refinement.clone(),
            self.cameras
                .iter()
                .map(|camera| camera.binding.clone())
                .collect(),
            self.landmarks
                .iter()
                .map(|landmark| landmark.seed.clone())
                .collect(),
            self.observations
                .iter()
                .map(|observation| observation.observation.clone())
                .collect(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(BundleProblemError::DerivedMismatch {
                field: "bundle_problem_generation",
            })
        }
    }

    /// Returns deterministic canonical semantic bytes.
    ///
    /// Successful execution ceilings, observed operation count, pruning rounds, and graph path
    /// expansions remain visible operation evidence but are excluded from semantic identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, graph, or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, BundleProblemError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(
            4_096_usize
                .saturating_add(self.cameras.len().saturating_mul(256))
                .saturating_add(self.landmarks.len().saturating_mul(192))
                .saturating_add(self.observations.len().saturating_mul(192))
                .saturating_add(self.components.len().saturating_mul(512)),
        );
        encoder.put_str(BUNDLE_PROBLEM_SCHEMA)?;
        encoder.put_digest(&self.basis.pose_refinement_digest);
        encoder.put_digest(&self.basis.camera_binding_basis_digest);
        encoder.put_digest(&self.basis.landmark_seed_basis_digest);
        encoder.put_digest(&self.basis.observation_basis_digest);
        encoder.put_digest(&self.basis.policy_digest);
        encoder.put_u64(self.basis.generation);
        encode_semantic_policy(&mut encoder, self.policy);

        encoder.put_u64(usize_to_u64(self.cameras.len())?);
        for camera in &self.cameras {
            encode_camera_binding(&mut encoder, &camera.binding);
            encoder.put_u64(camera.component_root_node_id);
            encode_optional_u64(&mut encoder, camera.scale_component_root_edge_id);
            for value in camera.rotation_node_from_root_nano {
                encoder.put_i64(value);
            }
            for value in camera.selected_camera_center_from_root_nano {
                encoder.put_i64(value);
            }
            encoder.put_u8(camera.pose_selection.code());
            encoder.put_u8(camera.disposition.code());
            encoder.put_u32(camera.active_landmark_count);
        }

        encoder.put_u64(usize_to_u64(self.landmarks.len())?);
        for landmark in &self.landmarks {
            encode_landmark_seed(&mut encoder, &landmark.seed);
            encoder.put_u8(landmark.disposition.code());
            encode_u64s(&mut encoder, &landmark.active_camera_node_ids)?;
            encode_u64s(&mut encoder, &landmark.held_out_camera_node_ids)?;
        }

        encoder.put_u64(usize_to_u64(self.observations.len())?);
        for observation in &self.observations {
            encode_observation(&mut encoder, &observation.observation);
            encoder.put_u8(observation.disposition.code());
        }

        encoder.put_u64(usize_to_u64(self.graph_nodes.len())?);
        for mapping in &self.graph_nodes {
            encoder.put_u64(mapping.graph_node_id);
            encoder.put_u8(mapping.kind.code());
            encoder.put_u64(mapping.domain_id);
            encoder.put_u64(mapping.component_root_node_id);
            encoder.put_digest(&mapping.evidence_digest);
        }
        encoder.put_digest(&self.graph.digest()?);

        encoder.put_u64(usize_to_u64(self.components.len())?);
        for component in &self.components {
            encoder.put_u64(component.component_root_node_id);
            encode_optional_u64(&mut encoder, component.scale_component_root_edge_id);
            encoder.put_u8(component.status.code());
            encoder.put_u8(component.decision.code());
            encoder.put_u8(component.recommendation.code());
            encode_u64s(&mut encoder, &component.active_camera_node_ids)?;
            encode_u64s(&mut encoder, &component.pruned_camera_node_ids)?;
            encode_u64s(&mut encoder, &component.active_landmark_ids)?;
            encode_u64s(&mut encoder, &component.pruned_landmark_ids)?;
            encode_u64s(
                &mut encoder,
                &component.active_optimize_observation_ids,
            )?;
            encode_u64s(
                &mut encoder,
                &component.eligible_held_out_observation_ids,
            )?;
            encode_u64s(
                &mut encoder,
                &component.eligible_held_out_camera_node_ids,
            )?;
            encode_u64s(&mut encoder, &component.bridge_observation_ids)?;
            encode_u64s(&mut encoder, &component.forest_observation_ids)?;
            encode_u64s(&mut encoder, &component.non_forest_observation_ids)?;
            encode_u64s(&mut encoder, &component.graph_component_ids)?;
            encoder.put_bool(component.root_connected);
            encoder.put_u32(component.root_active_landmark_count);
            encoder.put_u64(component.cycle_rank);
            encoder.put_u64(component.nominal_equation_count);
            encoder.put_u64(component.nominal_unknown_count);
            encoder.put_i64(component.nominal_equation_surplus);
        }
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated semantic bundle-problem identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, graph, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, BundleProblemError> {
        let domain = DigestDomain::parse(BUNDLE_PROBLEM_SCHEMA)?;
        Ok(hash_domain(&domain, &self.to_canonical_bytes()?)?)
    }

    /// Renders deterministic field-ordered JSON for agents and public tooling.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, graph, or formatting error.
    pub fn to_json(&self) -> Result<String, BundleProblemError> {
        let digest = self.digest()?;
        let graph_digest = self.graph.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{BUNDLE_PROBLEM_SCHEMA}\",\"bundle_problem_digest\":\"{digest}\",\"authority\":\"{BUNDLE_PROBLEM_AUTHORITY}\",\"pose_refinement_digest\":\"{}\",\"camera_binding_basis_digest\":\"{}\",\"landmark_seed_basis_digest\":\"{}\",\"observation_basis_digest\":\"{}\",\"policy_digest\":\"{}\",\"generation\":{},\"camera_count\":{},\"landmark_count\":{},\"observation_count\":{},\"graph_digest\":\"{graph_digest}\",\"graph_node_count\":{},\"graph_edge_count\":{},\"graph_cycle_rank\":{},\"graph_path_expansions\":{},\"component_count\":{},\"pruning_rounds\":{},\"operation_count\":{},\"policy\":{{\"min_optimize_cameras_per_landmark\":{},\"min_active_landmarks_per_camera\":{},\"min_root_active_landmarks\":{},\"max_observation_uncertainty_nano_pixels\":{},\"min_held_out_observations_per_component\":{},\"min_held_out_cameras_per_component\":{},\"max_graph_path_expansions\":{},\"max_operations\":{}}},\"cameras\":[",
            self.basis.pose_refinement_digest,
            self.basis.camera_binding_basis_digest,
            self.basis.landmark_seed_basis_digest,
            self.basis.observation_basis_digest,
            self.basis.policy_digest,
            self.basis.generation,
            self.cameras.len(),
            self.landmarks.len(),
            self.observations.len(),
            self.graph_nodes.len(),
            self.graph.edges.len(),
            self.graph.cycle_rank,
            self.graph.path_expansions,
            self.components.len(),
            self.pruning_rounds,
            self.operation_count,
            self.policy.min_optimize_cameras_per_landmark,
            self.policy.min_active_landmarks_per_camera,
            self.policy.min_root_active_landmarks,
            self.policy.max_observation_uncertainty_nano_pixels,
            self.policy.min_held_out_observations_per_component,
            self.policy.min_held_out_cameras_per_component,
            self.policy.max_graph_path_expansions,
            self.policy.max_operations,
        );
        for (index, camera) in self.cameras.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"camera_node_id\":{},\"sample_index\":{},\"frame_digest\":\"{}\",\"effective_calibration_digest\":\"{}\",\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"rotation_node_from_root_nano\":[",
                camera.binding.camera_node_id,
                camera.binding.sample_index,
                camera.binding.frame_digest,
                camera.binding.effective_calibration_digest,
                camera.component_root_node_id,
                optional_u64_json(camera.scale_component_root_edge_id),
            )
            .map_err(json_error)?;
            push_i64s(&mut output, &camera.rotation_node_from_root_nano)?;
            output.push_str("],\"selected_camera_center_from_root_nano\":[");
            push_i64s(
                &mut output,
                &camera.selected_camera_center_from_root_nano,
            )?;
            write!(
                output,
                "],\"pose_selection\":\"{}\",\"disposition\":\"{}\",\"active_landmark_count\":{}}}",
                camera.pose_selection.as_str(),
                camera.disposition.as_str(),
                camera.active_landmark_count,
            )
            .map_err(json_error)?;
        }

        output.push_str("],\"landmarks\":[");
        for (index, landmark) in self.landmarks.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"landmark_id\":{},\"source_track_id\":{},\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"seed_evidence_digest\":\"{}\",\"seed_position_from_root_nano\":[",
                landmark.seed.landmark_id,
                landmark.seed.source_track_id,
                landmark.seed.component_root_node_id,
                optional_u64_json(landmark.seed.scale_component_root_edge_id),
                landmark.seed.seed_evidence_digest,
            )
            .map_err(json_error)?;
            push_i64s(&mut output, &landmark.seed.seed_position_from_root_nano)?;
            write!(
                output,
                "],\"seed_uncertainty_nano\":{},\"disposition\":\"{}\",\"active_camera_node_ids\":[",
                landmark.seed.seed_uncertainty_nano,
                landmark.disposition.as_str(),
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &landmark.active_camera_node_ids)?;
            output.push_str("],\"held_out_camera_node_ids\":[");
            push_u64s(&mut output, &landmark.held_out_camera_node_ids)?;
            output.push_str("]}");
        }

        output.push_str("],\"observations\":[");
        for (index, compiled) in self.observations.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            let observation = &compiled.observation;
            write!(
                output,
                "{{\"observation_id\":{},\"landmark_id\":{},\"camera_node_id\":{},\"sample_index\":{},\"frame_digest\":\"{}\",\"source_feature_observation_id\":{},\"evidence_digest\":\"{}\",\"x_nano_pixels\":{},\"y_nano_pixels\":{},\"localization_uncertainty_nano_pixels\":{},\"dynamic_masked\":{},\"role\":\"{}\",\"disposition\":\"{}\"}}",
                observation.observation_id,
                observation.landmark_id,
                observation.camera_node_id,
                observation.sample_index,
                observation.frame_digest,
                observation.source_feature_observation_id,
                observation.evidence_digest,
                observation.x_nano_pixels,
                observation.y_nano_pixels,
                observation.localization_uncertainty_nano_pixels,
                observation.dynamic_masked,
                observation.role.as_str(),
                compiled.disposition.as_str(),
            )
            .map_err(json_error)?;
        }

        output.push_str("],\"graph_nodes\":[");
        for (index, mapping) in self.graph_nodes.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"graph_node_id\":{},\"kind\":\"{}\",\"domain_id\":{},\"component_root_node_id\":{},\"evidence_digest\":\"{}\"}}",
                mapping.graph_node_id,
                mapping.kind.as_str(),
                mapping.domain_id,
                mapping.component_root_node_id,
                mapping.evidence_digest,
            )
            .map_err(json_error)?;
        }

        output.push_str("],\"graph\":");
        output.push_str(&self.graph.to_json()?);
        output.push_str(",\"components\":[");
        for (index, component) in self.components.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"status\":\"{}\",\"decision\":\"{}\",\"recommendation\":\"{}\",\"root_connected\":{},\"root_active_landmark_count\":{},\"cycle_rank\":{},\"nominal_equation_count\":{},\"nominal_unknown_count\":{},\"nominal_equation_surplus\":{},\"rank_authority\":\"planning_count_only\",\"active_camera_node_ids\":[",
                component.component_root_node_id,
                optional_u64_json(component.scale_component_root_edge_id),
                component.status.as_str(),
                component.decision.as_str(),
                component.recommendation.as_str(),
                component.root_connected,
                component.root_active_landmark_count,
                component.cycle_rank,
                component.nominal_equation_count,
                component.nominal_unknown_count,
                component.nominal_equation_surplus,
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &component.active_camera_node_ids)?;
            output.push_str("],\"pruned_camera_node_ids\":[");
            push_u64s(&mut output, &component.pruned_camera_node_ids)?;
            output.push_str("],\"active_landmark_ids\":[");
            push_u64s(&mut output, &component.active_landmark_ids)?;
            output.push_str("],\"pruned_landmark_ids\":[");
            push_u64s(&mut output, &component.pruned_landmark_ids)?;
            output.push_str("],\"active_optimize_observation_ids\":[");
            push_u64s(
                &mut output,
                &component.active_optimize_observation_ids,
            )?;
            output.push_str("],\"eligible_held_out_observation_ids\":[");
            push_u64s(
                &mut output,
                &component.eligible_held_out_observation_ids,
            )?;
            output.push_str("],\"eligible_held_out_camera_node_ids\":[");
            push_u64s(
                &mut output,
                &component.eligible_held_out_camera_node_ids,
            )?;
            output.push_str("],\"bridge_observation_ids\":[");
            push_u64s(&mut output, &component.bridge_observation_ids)?;
            output.push_str("],\"forest_observation_ids\":[");
            push_u64s(&mut output, &component.forest_observation_ids)?;
            output.push_str("],\"non_forest_observation_ids\":[");
            push_u64s(&mut output, &component.non_forest_observation_ids)?;
            output.push_str("],\"graph_component_ids\":[");
            push_u64s(&mut output, &component.graph_component_ids)?;
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn encode_semantic_policy(encoder: &mut Encoder, policy: BundleProblemPolicy) {
    encoder.put_u32(policy.min_optimize_cameras_per_landmark);
    encoder.put_u32(policy.min_active_landmarks_per_camera);
    encoder.put_u32(policy.min_root_active_landmarks);
    encoder.put_u64(policy.max_observation_uncertainty_nano_pixels);
    encoder.put_u32(policy.min_held_out_observations_per_component);
    encoder.put_u32(policy.min_held_out_cameras_per_component);
}

fn encode_u64s(
    encoder: &mut Encoder,
    values: &[u64],
) -> Result<(), BundleProblemError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn push_u64s(output: &mut String, values: &[u64]) -> Result<(), BundleProblemError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn push_i64s(output: &mut String, values: &[i64]) -> Result<(), BundleProblemError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |value| value.to_string())
}

fn json_error(error: fmt::Error) -> BundleProblemError {
    BundleProblemError::JsonRendering(error.to_string())
}
