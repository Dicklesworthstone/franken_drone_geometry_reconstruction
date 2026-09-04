fn parse(arguments: &[String]) -> Result<Options, String> {
    let usage = "usage: fdgr reprojection-evaluate <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> <camera-bindings.tsv> <landmark-seeds.tsv> <bundle-observations.tsv> <camera-domains.tsv> <seed-provenance.tsv> <effective-calibrations.tsv> [exact upstream identities and gates] --camera-domain-file-digest <digest> --seed-provenance-file-digest <digest> --reprojection-calibration-file-digest <digest> --bundle-admission-generation <n> --reprojection-generation <n> [audit and reprojection gates] [--format text|json]";
    let Some(node_path) = arguments.first() else {
        return Err(usage.to_owned());
    };
    let Some(edge_path) = arguments.get(1) else {
        return Err(usage.to_owned());
    };
    let Some(witness_path) = arguments.get(2) else {
        return Err(usage.to_owned());
    };
    let Some(camera_binding_path) = arguments.get(3) else {
        return Err(usage.to_owned());
    };
    let Some(landmark_seed_path) = arguments.get(4) else {
        return Err(usage.to_owned());
    };
    let Some(observation_path) = arguments.get(5) else {
        return Err(usage.to_owned());
    };
    let Some(camera_domain_path) = arguments.get(6) else {
        return Err(usage.to_owned());
    };
    let Some(seed_provenance_path) = arguments.get(7) else {
        return Err(usage.to_owned());
    };
    let Some(calibration_path) = arguments.get(8) else {
        return Err(usage.to_owned());
    };
    let mut problem = BundleProblemPipelineParser::new(
        node_path,
        edge_path,
        witness_path,
        camera_binding_path,
        landmark_seed_path,
        observation_path,
    );
    let mut camera_domain_file_digest = None;
    let mut seed_provenance_file_digest = None;
    let mut calibration_file_digest = None;
    let mut admission_generation = None;
    let mut admission_policy = BundleAdmissionPolicy::default();
    let mut reprojection_generation = None;
    let mut reprojection_policy = ReprojectionPolicy::default();
    let mut format = OutputFormat::Text;
    let mut position = 9_usize;
    while position < arguments.len() {
        let flag = arguments
            .get(position)
            .ok_or_else(|| usage.to_owned())?;
        let value = arguments
            .get(position.saturating_add(1))
            .ok_or_else(|| format!("missing value for {flag}"))?;
        if problem.apply_option(flag, value)? {
            position = position.saturating_add(2);
            continue;
        }
        match flag.as_str() {
            "--camera-domain-file-digest" => {
                camera_domain_file_digest =
                    Some(parse_digest_option(value, "camera-domain file digest")?);
            }
            "--seed-provenance-file-digest" => {
                seed_provenance_file_digest =
                    Some(parse_digest_option(value, "seed-provenance file digest")?);
            }
            "--reprojection-calibration-file-digest" => {
                calibration_file_digest = Some(parse_digest_option(
                    value,
                    "reprojection calibration file digest",
                )?);
            }
            "--bundle-admission-generation" => {
                admission_generation =
                    Some(parse_nonzero_u64_option(value, "bundle-admission generation")?);
            }
            "--max-seed-uncertainty-nano" => {
                admission_policy.max_seed_uncertainty_nano =
                    parse_nonzero_u64_option(value, "maximum seed uncertainty nano")?;
            }
            "--min-seed-support-observations" => {
                admission_policy.min_seed_support_observations = parse_nonzero_u32_option(
                    value,
                    "minimum seed-support observations",
                )?;
            }
            "--min-seed-support-cameras" => {
                admission_policy.min_seed_support_cameras =
                    parse_nonzero_u32_option(value, "minimum seed-support cameras")?;
            }
            "--require-active-held-out-camera" => {
                admission_policy.require_active_held_out_camera =
                    parse_bool_option(value, "require active held-out camera")?;
            }
            "--max-bundle-admission-operations" => {
                admission_policy.max_operations =
                    parse_nonzero_u64_option(value, "maximum bundle-admission operations")?;
            }
            "--reprojection-generation" => {
                reprojection_generation =
                    Some(parse_nonzero_u64_option(value, "reprojection generation")?);
            }
            "--minimum-positive-depth-nano" => {
                reprojection_policy.projection.minimum_positive_depth_nano =
                    parse_nonzero_u64_option(value, "minimum positive depth nano")?;
            }
            "--max-normalized-coordinate-abs-nano" => {
                reprojection_policy.projection.max_normalized_coordinate_abs_nano =
                    parse_nonzero_u64_option(
                        value,
                        "maximum normalized coordinate absolute nano",
                    )?;
            }
            "--max-projected-coordinate-abs-nano-pixels" => {
                reprojection_policy
                    .projection
                    .max_projected_coordinate_abs_nano_pixels = parse_nonzero_u64_option(
                    value,
                    "maximum projected coordinate absolute nano-pixels",
                )?;
            }
            "--max-reprojection-residual-nano-pixels" => {
                reprojection_policy.max_residual_nano_pixels = parse_nonzero_u64_option(
                    value,
                    "maximum reprojection residual nano-pixels",
                )?;
            }
            "--max-normalized-residual-ppm" => {
                reprojection_policy.max_normalized_residual_ppm =
                    parse_nonzero_u64_option(value, "maximum normalized residual ppm")?;
            }
            "--min-reprojection-optimize-observations" => {
                reprojection_policy.min_optimize_observations_per_component =
                    parse_nonzero_u32_option(
                        value,
                        "minimum reprojection optimize observations",
                    )?;
            }
            "--min-reprojection-held-out-observations" => {
                reprojection_policy.min_held_out_observations_per_component =
                    parse_nonzero_u32_option(
                        value,
                        "minimum reprojection held-out observations",
                    )?;
            }
            "--max-reprojection-operations" => {
                reprojection_policy.max_operations =
                    parse_nonzero_u64_option(value, "maximum reprojection operations")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown reprojection-evaluate option {flag:?}")),
        }
        position = position.saturating_add(2);
    }
    Ok(Options {
        problem: problem.finish()?,
        camera_domain_path: PathBuf::from(camera_domain_path),
        camera_domain_file_digest: camera_domain_file_digest
            .ok_or_else(|| "missing --camera-domain-file-digest".to_owned())?,
        seed_provenance_path: PathBuf::from(seed_provenance_path),
        seed_provenance_file_digest: seed_provenance_file_digest
            .ok_or_else(|| "missing --seed-provenance-file-digest".to_owned())?,
        calibration_path: PathBuf::from(calibration_path),
        calibration_file_digest: calibration_file_digest
            .ok_or_else(|| "missing --reprojection-calibration-file-digest".to_owned())?,
        admission_generation: admission_generation
            .ok_or_else(|| "missing --bundle-admission-generation".to_owned())?,
        admission_policy,
        reprojection_generation: reprojection_generation
            .ok_or_else(|| "missing --reprojection-generation".to_owned())?,
        reprojection_policy,
        format,
    })
}

fn read_camera_domains(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<BundleCameraDomain>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_CAMERA_DOMAIN_TABLE_BYTES,
        "bundle camera-domain",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "bundle camera-domain table is empty".to_owned())?;
    if header != CAMERA_DOMAIN_HEADER {
        return Err(format!(
            "bundle camera-domain header mismatch: expected {CAMERA_DOMAIN_HEADER:?}"
        ));
    }
    let mut domains = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        reject_empty_line(line, "bundle camera-domain", line_number)?;
        let fields = line.split('\t').collect::<Vec<_>>();
        let [camera_node_id, frame_digest, effective_calibration_digest, image_width, image_height] =
            fields.as_slice()
        else {
            return Err(format!(
                "bundle camera-domain line {line_number} must contain exactly five tab-separated fields"
            ));
        };
        domains.push(BundleCameraDomain {
            camera_node_id: parse_nonzero_u64_line(
                camera_node_id,
                "camera node id",
                line_number,
            )?,
            frame_digest: parse_digest(frame_digest, "frame digest", line_number)?,
            effective_calibration_digest: parse_digest(
                effective_calibration_digest,
                "effective calibration digest",
                line_number,
            )?,
            image_width: parse_nonzero_u32_line(image_width, "image width", line_number)?,
            image_height: parse_nonzero_u32_line(image_height, "image height", line_number)?,
        });
        if domains.len() > MAX_CAMERA_DOMAINS {
            return Err(format!(
                "bundle camera-domain table contains more than {MAX_CAMERA_DOMAINS} records"
            ));
        }
    }
    Ok(domains)
}

fn read_seed_provenance(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<LandmarkSeedProvenance>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_SEED_PROVENANCE_TABLE_BYTES,
        "bundle seed-provenance",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "bundle seed-provenance table is empty".to_owned())?;
    if header != SEED_PROVENANCE_HEADER {
        return Err(format!(
            "bundle seed-provenance header mismatch: expected {SEED_PROVENANCE_HEADER:?}"
        ));
    }
    let mut provenance = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        reject_empty_line(line, "bundle seed-provenance", line_number)?;
        let fields = line.split('\t').collect::<Vec<_>>();
        let [landmark_id, support_observation_ids] = fields.as_slice() else {
            return Err(format!(
                "bundle seed-provenance line {line_number} must contain exactly two tab-separated fields"
            ));
        };
        provenance.push(LandmarkSeedProvenance {
            landmark_id: parse_nonzero_u64_line(landmark_id, "landmark id", line_number)?,
            support_observation_ids: parse_id_list(
                support_observation_ids,
                "support observation ids",
                line_number,
            )?,
        });
        if provenance.len() > MAX_SEED_PROVENANCES {
            return Err(format!(
                "bundle seed-provenance table contains more than {MAX_SEED_PROVENANCES} records"
            ));
        }
    }
    Ok(provenance)
}

fn read_calibrations(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<ReprojectionCalibration>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_REPROJECTION_CALIBRATION_TABLE_BYTES,
        "reprojection calibration",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "reprojection calibration table is empty".to_owned())?;
    if header != CALIBRATION_HEADER {
        return Err(format!(
            "reprojection calibration header mismatch: expected {CALIBRATION_HEADER:?}"
        ));
    }
    let mut calibrations = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        reject_empty_line(line, "reprojection calibration", line_number)?;
        let fields = line.split('\t').collect::<Vec<_>>();
        let [
            camera_node_id,
            frame_digest,
            effective_calibration_digest,
            source_calibration_digest,
            source_width,
            source_height,
            crop_x,
            crop_y,
            crop_width,
            crop_height,
            output_width,
            output_height,
            fx_nano_pixels,
            fy_nano_pixels,
            cx_nano_pixels,
            cy_nano_pixels,
            distortion_model,
            k1_nano,
            k2_nano,
            p1_nano,
            p2_nano,
            k3_nano,
            rolling,
            readout_direction,
            first_observed_line_offset_ns,
            observed_readout_time_ns,
            reference_phase_nano,
            r00,
            r01,
            r02,
            r10,
            r11,
            r12,
            r20,
            r21,
            r22,
            tx_micrometers,
            ty_micrometers,
            tz_micrometers,
            declared_uncertainty_nano_pixels,
        ] = fields.as_slice()
        else {
            return Err(format!(
                "reprojection calibration line {line_number} must contain exactly forty tab-separated fields"
            ));
        };
        let coefficients = [
            parse_i64_line(k1_nano, "k1 nano", line_number)?,
            parse_i64_line(k2_nano, "k2 nano", line_number)?,
            parse_i64_line(p1_nano, "p1 nano", line_number)?,
            parse_i64_line(p2_nano, "p2 nano", line_number)?,
            parse_i64_line(k3_nano, "k3 nano", line_number)?,
        ];
        let distortion = parse_distortion(distortion_model, coefficients, line_number)?;
        let rolling = parse_bool_option(rolling, "rolling readout")?;
        let first_observed_line_offset_ns = parse_u64_line(
            first_observed_line_offset_ns,
            "first observed line offset ns",
            line_number,
        )?;
        let observed_readout_time_ns = parse_u64_line(
            observed_readout_time_ns,
            "observed readout time ns",
            line_number,
        )?;
        let reference_phase_nano =
            parse_u32_line(reference_phase_nano, "reference phase nano", line_number)?;
        let readout = parse_readout(
            rolling,
            readout_direction,
            first_observed_line_offset_ns,
            observed_readout_time_ns,
            reference_phase_nano,
            line_number,
        )?;
        let calibration = DerivedCalibration {
            source_calibration_digest: parse_digest(
                source_calibration_digest,
                "source calibration digest",
                line_number,
            )?,
            derivation: ImageDerivation {
                source_width: parse_nonzero_u32_line(
                    source_width,
                    "source width",
                    line_number,
                )?,
                source_height: parse_nonzero_u32_line(
                    source_height,
                    "source height",
                    line_number,
                )?,
                crop_x: parse_u32_line(crop_x, "crop x", line_number)?,
                crop_y: parse_u32_line(crop_y, "crop y", line_number)?,
                crop_width: parse_nonzero_u32_line(
                    crop_width,
                    "crop width",
                    line_number,
                )?,
                crop_height: parse_nonzero_u32_line(
                    crop_height,
                    "crop height",
                    line_number,
                )?,
                output_width: parse_nonzero_u32_line(
                    output_width,
                    "output width",
                    line_number,
                )?,
                output_height: parse_nonzero_u32_line(
                    output_height,
                    "output height",
                    line_number,
                )?,
            },
            intrinsics: PinholeIntrinsics {
                fx_nano_pixels: parse_i64_line(
                    fx_nano_pixels,
                    "fx nano-pixels",
                    line_number,
                )?,
                fy_nano_pixels: parse_i64_line(
                    fy_nano_pixels,
                    "fy nano-pixels",
                    line_number,
                )?,
                cx_nano_pixels: parse_i64_line(
                    cx_nano_pixels,
                    "cx nano-pixels",
                    line_number,
                )?,
                cy_nano_pixels: parse_i64_line(
                    cy_nano_pixels,
                    "cy nano-pixels",
                    line_number,
                )?,
                pixel_convention: PixelConvention::TopLeftHalfPixel,
            },
            distortion,
            readout,
            camera_from_body: RigidTransform {
                rotation_nano: [
                    parse_i64_line(r00, "r00", line_number)?,
                    parse_i64_line(r01, "r01", line_number)?,
                    parse_i64_line(r02, "r02", line_number)?,
                    parse_i64_line(r10, "r10", line_number)?,
                    parse_i64_line(r11, "r11", line_number)?,
                    parse_i64_line(r12, "r12", line_number)?,
                    parse_i64_line(r20, "r20", line_number)?,
                    parse_i64_line(r21, "r21", line_number)?,
                    parse_i64_line(r22, "r22", line_number)?,
                ],
                translation_micrometers: [
                    parse_i64_line(
                        tx_micrometers,
                        "translation x micrometers",
                        line_number,
                    )?,
                    parse_i64_line(
                        ty_micrometers,
                        "translation y micrometers",
                        line_number,
                    )?,
                    parse_i64_line(
                        tz_micrometers,
                        "translation z micrometers",
                        line_number,
                    )?,
                ],
            },
            declared_uncertainty_nano_pixels: parse_u64_line(
                declared_uncertainty_nano_pixels,
                "declared uncertainty nano-pixels",
                line_number,
            )?,
        };
        calibration
            .validate()
            .map_err(|error| format!("line {line_number}: invalid derived calibration: {error}"))?;
        calibrations.push(ReprojectionCalibration {
            camera_node_id: parse_nonzero_u64_line(
                camera_node_id,
                "camera node id",
                line_number,
            )?,
            frame_digest: parse_digest(frame_digest, "frame digest", line_number)?,
            effective_calibration_digest: parse_digest(
                effective_calibration_digest,
                "effective calibration digest",
                line_number,
            )?,
            calibration,
        });
        if calibrations.len() > MAX_REPROJECTION_CALIBRATIONS {
            return Err(format!(
                "reprojection calibration table contains more than {MAX_REPROJECTION_CALIBRATIONS} records"
            ));
        }
    }
    if calibrations.is_empty() {
        return Err("reprojection calibration table contains no records".to_owned());
    }
    Ok(calibrations)
}

fn parse_distortion(
    model: &str,
    coefficients: [i64; 5],
    line: usize,
) -> Result<DistortionModel, String> {
    match model {
        "none" if coefficients == [0; 5] => Ok(DistortionModel::None),
        "none" => Err(format!(
            "line {line}: distortion model none requires five zero coefficients"
        )),
        "brown_conrady" => Ok(DistortionModel::BrownConrady {
            k1_nano: coefficients[0],
            k2_nano: coefficients[1],
            p1_nano: coefficients[2],
            p2_nano: coefficients[3],
            k3_nano: coefficients[4],
        }),
        _ => Err(format!(
            "line {line}: unknown distortion model {model:?}; expected none or brown_conrady"
        )),
    }
}

fn parse_readout(
    rolling: bool,
    direction: &str,
    first_observed_line_offset_ns: u64,
    observed_readout_time_ns: u64,
    reference_phase_nano: u32,
    line: usize,
) -> Result<DerivedReadout, String> {
    if !rolling {
        if direction != "none"
            || first_observed_line_offset_ns != 0
            || observed_readout_time_ns != 0
            || reference_phase_nano != 0
        {
            return Err(format!(
                "line {line}: nonrolling calibration requires direction none and zero readout values"
            ));
        }
        return Ok(DerivedReadout {
            rolling: false,
            direction: None,
            first_observed_line_offset_ns: 0,
            observed_readout_time_ns: 0,
            reference_phase_nano: 0,
        });
    }
    if observed_readout_time_ns == 0 {
        return Err(format!(
            "line {line}: rolling calibration requires nonzero observed readout time"
        ));
    }
    let direction = match direction {
        "top_to_bottom" => ReadoutDirection::TopToBottom,
        "bottom_to_top" => ReadoutDirection::BottomToTop,
        "left_to_right" => ReadoutDirection::LeftToRight,
        "right_to_left" => ReadoutDirection::RightToLeft,
        _ => {
            return Err(format!(
                "line {line}: rolling calibration requires a concrete readout direction"
            ));
        }
    };
    Ok(DerivedReadout {
        rolling: true,
        direction: Some(direction),
        first_observed_line_offset_ns,
        observed_readout_time_ns,
        reference_phase_nano,
    })
}

fn parse_id_list(value: &str, label: &str, line: usize) -> Result<Vec<u64>, String> {
    if value == "none" {
        return Ok(Vec::new());
    }
    if value.is_empty() {
        return Err(format!(
            "line {line}: {label} must be comma-separated nonzero integers or none"
        ));
    }
    value
        .split(',')
        .map(|entry| parse_nonzero_u64_line(entry, label, line))
        .collect()
}

fn parse_nonzero_u64_line(value: &str, label: &str, line: usize) -> Result<u64, String> {
    parse_nonzero_u64_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_u64_line(value: &str, label: &str, line: usize) -> Result<u64, String> {
    parse_u64_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_nonzero_u32_line(value: &str, label: &str, line: usize) -> Result<u32, String> {
    parse_nonzero_u32_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_u32_line(value: &str, label: &str, line: usize) -> Result<u32, String> {
    value
        .parse::<u32>()
        .map_err(|error| format!("line {line}: invalid {label} {value:?}: {error}"))
}

fn parse_i64_line(value: &str, label: &str, line: usize) -> Result<i64, String> {
    value
        .parse::<i64>()
        .map_err(|error| format!("line {line}: invalid {label} {value:?}: {error}"))
}

fn parse_bool_option(value: &str, label: &str) -> Result<bool, String> {
    match value {
        "true" => Ok(true),
        "false" => Ok(false),
        _ => Err(format!(
            "invalid {label} {value:?}; expected true or false"
        )),
    }
}

fn reject_empty_line(line: &str, label: &str, line_number: usize) -> Result<(), String> {
    if line.is_empty() {
        Err(format!("{label} line {line_number} is empty"))
    } else {
        Ok(())
    }
}

fn parse_format(value: &str) -> Result<OutputFormat, String> {
    match value {
        "text" => Ok(OutputFormat::Text),
        "json" => Ok(OutputFormat::Json),
        _ => Err(format!(
            "unknown output format {value:?}; expected text or json"
        )),
    }
}
