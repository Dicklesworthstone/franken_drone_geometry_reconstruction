impl PoseRefinementGeneration {
    /// Replays every derived field from the exact retained initialization.
    ///
    /// # Errors
    ///
    /// Returns a stable upstream, construction, budget, or deterministic mismatch.
    pub fn validate(&self) -> Result<(), PoseRefinementError> {
        let rebuilt = refine_pose_translation(
            self.basis.clone(),
            self.policy,
            self.initialization.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(PoseRefinementError::DerivedMismatch {
                field: "pose_refinement_generation",
            })
        }
    }

    /// Returns deterministic canonical semantic bytes.
    ///
    /// `max_operations` and `operation_count` remain visible operation evidence but are excluded
    /// from successful semantic identity.
    ///
    /// # Errors
    ///
    /// Returns a validation or canonical codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, PoseRefinementError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(
            4_096_usize
                .saturating_add(self.poses.len().saturating_mul(256))
                .saturating_add(self.factors.len().saturating_mul(192))
                .saturating_add(self.components.len().saturating_mul(192)),
        );
        encoder.put_str(POSE_REFINEMENT_SCHEMA)?;
        encoder.put_digest(&self.basis.initialization_digest);
        encoder.put_digest(&self.basis.policy_digest);
        encoder.put_u64(self.basis.generation);
        encoder.put_u32(self.policy.max_iterations);
        encoder.put_u64(self.policy.convergence_delta_nano);
        encoder.put_u64(self.policy.huber_delta_nano);
        encoder.put_u32(self.policy.damping_weight);
        encoder.put_u32(self.policy.max_factor_weight);
        encoder.put_u64(self.policy.max_camera_center_abs_nano);

        encoder.put_u64(usize_to_u64(self.poses.len())?);
        for pose in &self.poses {
            encoder.put_u64(pose.node_id);
            encoder.put_u64(pose.sample_index);
            encoder.put_u64(pose.component_root_node_id);
            encode_optional_u64(&mut encoder, pose.scale_component_root_edge_id);
            for value in pose.rotation_node_from_root_nano {
                encoder.put_i64(value);
            }
            for value in pose.initial_camera_center_from_root_nano {
                encoder.put_i64(value);
            }
            for value in pose.refined_camera_center_from_root_nano {
                encoder.put_i64(value);
            }
            for value in pose.adjustment_nano {
                encoder.put_i64(value);
            }
            encoder.put_bool(pose.root_pinned);
        }

        encoder.put_u64(usize_to_u64(self.factors.len())?);
        for factor in &self.factors {
            encoder.put_u64(factor.edge_id);
            encoder.put_u64(factor.component_root_node_id);
            encoder.put_u64(factor.left_node_id);
            encoder.put_u64(factor.right_node_id);
            encoder.put_u32(factor.base_weight);
            encoder.put_u32(factor.robust_weight);
            encode_optional_vector(&mut encoder, factor.measured_displacement_nano);
            for value in factor.initial_implied_displacement_nano {
                encoder.put_i64(value);
            }
            for value in factor.refined_implied_displacement_nano {
                encoder.put_i64(value);
            }
            encode_optional_u64(&mut encoder, factor.initial_residual_nano);
            encode_optional_u64(&mut encoder, factor.final_residual_nano);
            encoder.put_u8(factor.disposition.code());
        }

        encoder.put_u64(usize_to_u64(self.components.len())?);
        for component in &self.components {
            encoder.put_u64(component.component_root_node_id);
            encode_optional_u64(&mut encoder, component.scale_component_root_edge_id);
            encoder.put_u8(component.status.code());
            encoder.put_u8(component.decision.code());
            encoder.put_u8(component.reason.code());
            encoder.put_u32(component.iterations);
            encode_optional_u64(&mut encoder, component.initial_rms_residual_nano);
            encode_optional_u64(&mut encoder, component.final_rms_residual_nano);
            encode_optional_u64(&mut encoder, component.rms_improvement_nano);
            encoder.put_u64(component.max_adjustment_nano);
            encoder.put_bool(component.do_nothing_dominates);
            encode_u64s(&mut encoder, &component.node_ids)?;
            encode_u64s(&mut encoder, &component.factor_edge_ids)?;
            encode_u64s(&mut encoder, &component.active_factor_edge_ids)?;
            encode_u64s(&mut encoder, &component.downweighted_edge_ids)?;
        }
        encoder.put_u64(self.iteration_count);
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated semantic refinement identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, PoseRefinementError> {
        let domain = DigestDomain::parse(POSE_REFINEMENT_SCHEMA)?;
        Ok(hash_domain(&domain, &self.to_canonical_bytes()?)?)
    }

    /// Renders deterministic field-ordered JSON.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, PoseRefinementError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{POSE_REFINEMENT_SCHEMA}\",\"refinement_digest\":\"{digest}\",\"authority\":\"{POSE_REFINEMENT_AUTHORITY}\",\"unit\":\"component_edge_scale_unit_nano\",\"initialization_digest\":\"{}\",\"policy_digest\":\"{}\",\"generation\":{},\"pose_count\":{},\"factor_count\":{},\"component_count\":{},\"iteration_count\":{},\"operation_count\":{},\"policy\":{{\"max_iterations\":{},\"convergence_delta_nano\":{},\"huber_delta_nano\":{},\"damping_weight\":{},\"max_factor_weight\":{},\"max_camera_center_abs_nano\":{},\"max_operations\":{}}},\"poses\":[",
            self.basis.initialization_digest,
            self.basis.policy_digest,
            self.basis.generation,
            self.poses.len(),
            self.factors.len(),
            self.components.len(),
            self.iteration_count,
            self.operation_count,
            self.policy.max_iterations,
            self.policy.convergence_delta_nano,
            self.policy.huber_delta_nano,
            self.policy.damping_weight,
            self.policy.max_factor_weight,
            self.policy.max_camera_center_abs_nano,
            self.policy.max_operations,
        );
        for (index, pose) in self.poses.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"node_id\":{},\"sample_index\":{},\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"rotation_node_from_root_nano\":[",
                pose.node_id,
                pose.sample_index,
                pose.component_root_node_id,
                optional_u64_json(pose.scale_component_root_edge_id),
            )
            .map_err(json_error)?;
            push_i64s(&mut output, &pose.rotation_node_from_root_nano)?;
            output.push_str("],\"initial_camera_center_from_root_nano\":[");
            push_i64s(
                &mut output,
                &pose.initial_camera_center_from_root_nano,
            )?;
            output.push_str("],\"refined_camera_center_from_root_nano\":[");
            push_i64s(
                &mut output,
                &pose.refined_camera_center_from_root_nano,
            )?;
            output.push_str("],\"adjustment_nano\":[");
            push_i64s(&mut output, &pose.adjustment_nano)?;
            write!(
                output,
                "],\"root_pinned\":{}}}",
                pose.root_pinned,
            )
            .map_err(json_error)?;
        }

        output.push_str("],\"factors\":[");
        for (index, factor) in self.factors.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"edge_id\":{},\"component_root_node_id\":{},\"left_node_id\":{},\"right_node_id\":{},\"base_weight\":{},\"robust_weight\":{},\"measured_displacement_nano\":",
                factor.edge_id,
                factor.component_root_node_id,
                factor.left_node_id,
                factor.right_node_id,
                factor.base_weight,
                factor.robust_weight,
            )
            .map_err(json_error)?;
            push_optional_vector_json(&mut output, factor.measured_displacement_nano)?;
            output.push_str(",\"initial_implied_displacement_nano\":[");
            push_i64s(
                &mut output,
                &factor.initial_implied_displacement_nano,
            )?;
            output.push_str("],\"refined_implied_displacement_nano\":[");
            push_i64s(
                &mut output,
                &factor.refined_implied_displacement_nano,
            )?;
            write!(
                output,
                "],\"initial_residual_nano\":{},\"final_residual_nano\":{},\"disposition\":\"{}\"}}",
                optional_u64_json(factor.initial_residual_nano),
                optional_u64_json(factor.final_residual_nano),
                factor.disposition.as_str(),
            )
            .map_err(json_error)?;
        }

        output.push_str("],\"components\":[");
        for (index, component) in self.components.iter().enumerate() {
            if index > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"component_root_node_id\":{},\"scale_component_root_edge_id\":{},\"status\":\"{}\",\"decision\":\"{}\",\"reason\":\"{}\",\"iterations\":{},\"initial_rms_residual_nano\":{},\"final_rms_residual_nano\":{},\"rms_improvement_nano\":{},\"max_adjustment_nano\":{},\"do_nothing_dominates\":{},\"node_ids\":[",
                component.component_root_node_id,
                optional_u64_json(component.scale_component_root_edge_id),
                component.status.as_str(),
                component.decision.as_str(),
                component.reason.as_str(),
                component.iterations,
                optional_u64_json(component.initial_rms_residual_nano),
                optional_u64_json(component.final_rms_residual_nano),
                optional_u64_json(component.rms_improvement_nano),
                component.max_adjustment_nano,
                component.do_nothing_dominates,
            )
            .map_err(json_error)?;
            push_u64s(&mut output, &component.node_ids)?;
            output.push_str("],\"factor_edge_ids\":[");
            push_u64s(&mut output, &component.factor_edge_ids)?;
            output.push_str("],\"active_factor_edge_ids\":[");
            push_u64s(&mut output, &component.active_factor_edge_ids)?;
            output.push_str("],\"downweighted_edge_ids\":[");
            push_u64s(&mut output, &component.downweighted_edge_ids)?;
            output.push_str("]}");
        }
        output.push_str("]}");
        Ok(output)
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

fn encode_optional_vector(encoder: &mut Encoder, value: Option<[i64; 3]>) {
    match value {
        Some(vector) => {
            encoder.put_bool(true);
            for coordinate in vector {
                encoder.put_i64(coordinate);
            }
        }
        None => encoder.put_bool(false),
    }
}

fn encode_u64s(
    encoder: &mut Encoder,
    values: &[u64],
) -> Result<(), PoseRefinementError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn push_i64s<const N: usize>(
    output: &mut String,
    values: &[i64; N],
) -> Result<(), PoseRefinementError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn push_u64s(
    output: &mut String,
    values: &[u64],
) -> Result<(), PoseRefinementError> {
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_error)?;
    }
    Ok(())
}

fn push_optional_vector_json(
    output: &mut String,
    value: Option<[i64; 3]>,
) -> Result<(), PoseRefinementError> {
    match value {
        Some(vector) => {
            output.push('[');
            push_i64s(output, &vector)?;
            output.push(']');
        }
        None => output.push_str("null"),
    }
    Ok(())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |value| value.to_string())
}

fn json_error(error: fmt::Error) -> PoseRefinementError {
    PoseRefinementError::JsonRendering(error.to_string())
}

fn usize_to_u64(value: usize) -> Result<u64, PoseRefinementError> {
    u64::try_from(value).map_err(|_| PoseRefinementError::ArithmeticOverflow {
        field: "collection_length",
    })
}
