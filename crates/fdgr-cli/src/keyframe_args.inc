fn parse(arguments: &[String]) -> Result<KeyframeCliOptions, String> {
    let mut arguments = arguments.iter();
    let usage = "usage: fdgr keyframe-select <candidates.tsv> --candidate-basis-digest <digest> --timeline-digest <digest> --decoded-frame-generation-digest <digest> --calibration-digest <digest> --policy-digest <digest> --generation <n> [policy options] [--format text|json]";
    let candidate_path = arguments.next().ok_or_else(|| usage.to_owned())?;
    let mut candidate_basis_digest = None;
    let mut timeline_digest = None;
    let mut decoded_frame_generation_digest = None;
    let mut calibration_digest = None;
    let mut policy_digest = None;
    let mut selection_generation = None;
    let mut policy = KeyframePolicy::default();
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--candidate-basis-digest" => {
                candidate_basis_digest = Some(parse_digest(value, "candidate basis digest")?);
            }
            "--timeline-digest" => timeline_digest = Some(parse_digest(value, "timeline digest")?),
            "--decoded-frame-generation-digest" => {
                decoded_frame_generation_digest =
                    Some(parse_digest(value, "decoded frame generation digest")?);
            }
            "--calibration-digest" => {
                calibration_digest = Some(parse_digest(value, "calibration digest")?);
            }
            "--policy-digest" => policy_digest = Some(parse_digest(value, "policy digest")?),
            "--generation" => {
                selection_generation = Some(parse_nonzero_u64(value, "selection generation")?);
            }
            "--max-selected" => policy.max_selected = parse_nonzero_u32(value, "maximum selected")?,
            "--min-sharpness-ppm" => {
                policy.min_sharpness_ppm = parse_ppm(value, "minimum sharpness")?;
            }
            "--min-texture-ppm" => {
                policy.min_texture_ppm = parse_ppm(value, "minimum texture")?;
            }
            "--max-dark-clipped-ppm" => {
                policy.max_dark_clipped_ppm = parse_ppm(value, "maximum dark clipping")?;
            }
            "--max-bright-clipped-ppm" => {
                policy.max_bright_clipped_ppm = parse_ppm(value, "maximum bright clipping")?;
            }
            "--max-dynamic-content-ppm" => {
                policy.max_dynamic_content_ppm = parse_ppm(value, "maximum dynamic content")?;
            }
            "--min-overlap-ppm" => {
                policy.min_overlap_ppm = parse_ppm(value, "minimum overlap")?;
            }
            "--min-presentation-spacing-ticks" => {
                policy.min_presentation_spacing_ticks =
                    parse_u64(value, "minimum presentation spacing ticks")?;
            }
            "--quality-weight" => policy.quality_weight = parse_u32(value, "quality weight")?,
            "--coverage-weight" => policy.coverage_weight = parse_u32(value, "coverage weight")?,
            "--view-diversity-weight" => {
                policy.view_diversity_weight = parse_u32(value, "view diversity weight")?;
            }
            "--baseline-diversity-weight" => {
                policy.baseline_diversity_weight = parse_u32(value, "baseline diversity weight")?;
            }
            "--format" => format = parse_format(value)?,
            _ => return Err(format!("unknown keyframe-select option {flag:?}")),
        }
    }
    Ok(KeyframeCliOptions {
        candidate_path: PathBuf::from(candidate_path),
        candidate_basis_digest: candidate_basis_digest
            .ok_or_else(|| "missing --candidate-basis-digest".to_owned())?,
        timeline_digest: timeline_digest.ok_or_else(|| "missing --timeline-digest".to_owned())?,
        decoded_frame_generation_digest: decoded_frame_generation_digest
            .ok_or_else(|| "missing --decoded-frame-generation-digest".to_owned())?,
        calibration_digest: calibration_digest
            .ok_or_else(|| "missing --calibration-digest".to_owned())?,
        policy_digest: policy_digest.ok_or_else(|| "missing --policy-digest".to_owned())?,
        selection_generation: selection_generation
            .ok_or_else(|| "missing --generation".to_owned())?,
        policy,
        format,
    })
}

fn read_candidate_table(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<CandidateTable, String> {
    let metadata = fs::symlink_metadata(path)
        .map_err(|error| format!("keyframe candidate metadata failed: {error}"))?;
    if metadata.file_type().is_symlink() {
        return Err("keyframe candidate table must not be a symlink".to_owned());
    }
    if !metadata.is_file() {
        return Err("keyframe candidate table must be a regular file".to_owned());
    }
    let mut file = File::open(path)
        .map_err(|error| format!("keyframe candidate table open failed: {error}"))?;
    let mut bytes = Vec::new();
    file.by_ref()
        .take(MAX_CANDIDATE_TABLE_BYTES.saturating_add(1))
        .read_to_end(&mut bytes)
        .map_err(|error| format!("keyframe candidate table read failed: {error}"))?;
    let byte_length = u64::try_from(bytes.len())
        .map_err(|_| "candidate table length overflow".to_owned())?;
    if byte_length > MAX_CANDIDATE_TABLE_BYTES {
        return Err(format!(
            "keyframe candidate table exceeds {MAX_CANDIDATE_TABLE_BYTES} bytes"
        ));
    }
    let observed_digest = hash_bytes(&bytes);
    if &observed_digest != expected_digest {
        return Err(format!(
            "keyframe candidate basis digest mismatch: expected {expected_digest}, observed {observed_digest}"
        ));
    }
    let text = std::str::from_utf8(&bytes)
        .map_err(|error| format!("keyframe candidate table is not UTF-8: {error}"))?;
    if text.contains('\r') {
        return Err("keyframe candidate table must use LF line endings".to_owned());
    }
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "keyframe candidate table is empty".to_owned())?;
    if header != CANDIDATE_HEADER {
        return Err(format!(
            "keyframe candidate header mismatch: expected {CANDIDATE_HEADER:?}"
        ));
    }
    let mut candidates = Vec::new();
    let mut coverage_references = 0_usize;
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        if line.is_empty() {
            return Err(format!("keyframe candidate line {line_number} is empty"));
        }
        let fields = line.split('\t').collect::<Vec<_>>();
        let [
            sample_index,
            frame_digest,
            presentation_tick,
            sharpness_ppm,
            texture_ppm,
            dark_clipped_ppm,
            bright_clipped_ppm,
            dynamic_content_ppm,
            overlap_ppm,
            view_sector,
            baseline_bin,
            coverage_cells,
        ] = fields.as_slice()
        else {
            return Err(format!(
                "keyframe candidate line {line_number} must contain exactly twelve tab-separated fields"
            ));
        };
        let coverage_cells = parse_coverage_cells(coverage_cells, line_number)?;
        coverage_references = coverage_references
            .checked_add(coverage_cells.len())
            .ok_or_else(|| "candidate coverage reference count overflow".to_owned())?;
        if coverage_references > MAX_TOTAL_COVERAGE_REFERENCES {
            return Err(format!(
                "keyframe candidate table exceeds {MAX_TOTAL_COVERAGE_REFERENCES} total coverage references"
            ));
        }
        candidates.push(KeyframeCandidate {
            sample_index: parse_u64(sample_index, "sample index")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            frame_digest: parse_digest(frame_digest, "frame digest")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            presentation_tick: parse_i128(presentation_tick, "presentation tick")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            sharpness_ppm: parse_ppm(sharpness_ppm, "sharpness")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            texture_ppm: parse_ppm(texture_ppm, "texture")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            dark_clipped_ppm: parse_ppm(dark_clipped_ppm, "dark clipping")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            bright_clipped_ppm: parse_ppm(bright_clipped_ppm, "bright clipping")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            dynamic_content_ppm: parse_ppm(dynamic_content_ppm, "dynamic content")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            overlap_ppm: parse_ppm(overlap_ppm, "overlap")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            view_sector: parse_nonzero_u32(view_sector, "view sector")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            baseline_bin: parse_nonzero_u32(baseline_bin, "baseline bin")
                .map_err(|error| format!("line {line_number}: {error}"))?,
            coverage_cells,
        });
        if candidates.len() > MAX_KEYFRAME_CANDIDATES {
            return Err(format!(
                "keyframe candidate table contains more than {MAX_KEYFRAME_CANDIDATES} records"
            ));
        }
    }
    if candidates.is_empty() {
        return Err("keyframe candidate table contains no records".to_owned());
    }
    Ok(CandidateTable {
        digest: observed_digest,
        candidates,
    })
}

fn parse_coverage_cells(value: &str, line_number: usize) -> Result<Vec<u64>, String> {
    if value == "-" {
        return Ok(Vec::new());
    }
    if value.is_empty() {
        return Err(format!(
            "line {line_number}: coverage cells must be '-' or a comma-separated list"
        ));
    }
    let mut cells = Vec::new();
    for text in value.split(',') {
        cells.push(
            parse_nonzero_u64(text, "coverage cell")
                .map_err(|error| format!("line {line_number}: {error}"))?,
        );
    }
    if cells
        .windows(2)
        .any(|pair| matches!(pair, [left, right] if left >= right))
    {
        return Err(format!(
            "line {line_number}: coverage cells must be strictly increasing"
        ));
    }
    Ok(cells)
}

fn parse_digest(value: &str, label: &str) -> Result<EvidenceDigest, String> {
    EvidenceDigest::parse(value).map_err(|error| format!("invalid {label}: {error}"))
}

fn parse_format(value: &str) -> Result<OutputFormat, String> {
    match value {
        "text" => Ok(OutputFormat::Text),
        "json" => Ok(OutputFormat::Json),
        _ => Err(format!("unknown output format {value:?}; expected text or json")),
    }
}

fn parse_ppm(value: &str, label: &str) -> Result<u32, String> {
    let parsed = parse_u32(value, label)?;
    if parsed > PPM_SCALE {
        Err(format!("{label} is {parsed}; maximum is {PPM_SCALE}"))
    } else {
        Ok(parsed)
    }
}

fn parse_nonzero_u32(value: &str, label: &str) -> Result<u32, String> {
    let parsed = parse_u32(value, label)?;
    if parsed == 0 {
        Err(format!("{label} must be nonzero"))
    } else {
        Ok(parsed)
    }
}

fn parse_nonzero_u64(value: &str, label: &str) -> Result<u64, String> {
    let parsed = parse_u64(value, label)?;
    if parsed == 0 {
        Err(format!("{label} must be nonzero"))
    } else {
        Ok(parsed)
    }
}

fn parse_u32(value: &str, label: &str) -> Result<u32, String> {
    value
        .parse::<u32>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn parse_u64(value: &str, label: &str) -> Result<u64, String> {
    value
        .parse::<u64>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn parse_i128(value: &str, label: &str) -> Result<i128, String> {
    value
        .parse::<i128>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}
