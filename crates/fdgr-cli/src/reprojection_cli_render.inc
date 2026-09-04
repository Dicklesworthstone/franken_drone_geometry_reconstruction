fn print_text(evaluation: &ReprojectionEvaluation) -> Result<(), String> {
    let digest = evaluation
        .digest()
        .map_err(|error| format!("reprojection identity failed: {error}"))?;
    println!("schema: {REPROJECTION_EVALUATION_SCHEMA}");
    println!("reprojection_digest: {digest}");
    println!("authority: {}", evaluation.authority());
    println!(
        "bundle_admission_digest: {}",
        evaluation.basis.bundle_admission_digest
    );
    println!(
        "calibration_table_digest: {}",
        evaluation.basis.calibration_table_digest
    );
    println!("algorithm_digest: {}", evaluation.basis.algorithm_digest);
    println!("policy_digest: {}", evaluation.basis.policy_digest);
    println!("generation: {}", evaluation.basis.generation);
    println!("calibrations: {}", evaluation.calibrations.len());
    println!("observations: {}", evaluation.observations.len());
    println!("components: {}", evaluation.components.len());
    println!("operations: {}", evaluation.operation_count);
    for component in &evaluation.components {
        println!(
            "component root={} scale_root={} status={} decision={} recommendation={} optimize_eligible={} optimize_projected={} optimize_inliers={} optimize_rms_nano_pixels={} optimize_median_nano_pixels={} optimize_max_nano_pixels={} held_out_eligible={} held_out_projected={} held_out_inliers={} held_out_rms_nano_pixels={} held_out_median_nano_pixels={} held_out_max_nano_pixels={} unprojectable={:?} conflicting={:?}",
            component.component_root_node_id,
            display_optional_u64(component.scale_component_root_edge_id),
            component.status.as_str(),
            component.decision.as_str(),
            component.recommendation.as_str(),
            component.optimize.eligible_observation_count,
            component.optimize.projected_observation_count,
            component.optimize.inlier_observation_count,
            display_optional_u64(component.optimize.rms_residual_nano_pixels),
            display_optional_u64(component.optimize.median_residual_nano_pixels),
            display_optional_u64(component.optimize.maximum_residual_nano_pixels),
            component.held_out.eligible_observation_count,
            component.held_out.projected_observation_count,
            component.held_out.inlier_observation_count,
            display_optional_u64(component.held_out.rms_residual_nano_pixels),
            display_optional_u64(component.held_out.median_residual_nano_pixels),
            display_optional_u64(component.held_out.maximum_residual_nano_pixels),
            component.unprojectable_observation_ids,
            component.conflicting_observation_ids,
        );
    }
    for observation in &evaluation.observations {
        if observation.disposition.as_str() == "projected_inlier"
            || observation.disposition.as_str() == "upstream_excluded"
        {
            continue;
        }
        println!(
            "observation id={} landmark={} camera={} component_root={} role={} disposition={} projected=({}, {}) depth={} residual=({}, {}) magnitude={} normalized_ppm={} within_domain={}",
            observation.observation_id,
            observation.landmark_id,
            observation.camera_node_id,
            observation.component_root_node_id,
            observation.role.as_str(),
            observation.disposition.as_str(),
            display_optional_i64(observation.projected_x_nano_pixels),
            display_optional_i64(observation.projected_y_nano_pixels),
            display_optional_i64(observation.depth_nano),
            display_optional_i64(observation.residual_x_nano_pixels),
            display_optional_i64(observation.residual_y_nano_pixels),
            display_optional_u64(observation.residual_nano_pixels),
            display_optional_u64(observation.normalized_residual_ppm),
            display_optional_bool(observation.within_image_domain),
        );
    }
    Ok(())
}

fn display_optional_u64(value: Option<u64>) -> String {
    value.map_or_else(|| "none".to_owned(), |value| value.to_string())
}

fn display_optional_i64(value: Option<i64>) -> String {
    value.map_or_else(|| "none".to_owned(), |value| value.to_string())
}

fn display_optional_bool(value: Option<bool>) -> &'static str {
    match value {
        Some(true) => "true",
        Some(false) => "false",
        None => "none",
    }
}
