#![forbid(unsafe_code)]
#![allow(clippy::too_many_lines)]
//! Shared exact reconstruction seam for structural bundle-problem operations.

use crate::geometry_observation_cli::read_bound_text;
use crate::pose_graph_input_cli::{
    parse_digest, parse_digest_option, parse_nonzero_u32_option, parse_nonzero_u64_option,
    parse_u64_option,
};
use crate::pose_refinement_cli::{
    PoseRefinementPipelineOptions, PoseRefinementPipelineParser, build_pose_refinement,
};
use fdgr_bundle_problem::{
    BundleCameraBinding, BundleLandmarkSeed, BundleObservation, BundleObservationRole,
    BundleProblemBasis, BundleProblemGeneration, BundleProblemPolicy, MAX_BUNDLE_CAMERAS,
    MAX_BUNDLE_LANDMARKS, MAX_BUNDLE_OBSERVATIONS, bundle_observation_table_digest,
    bundle_problem_policy_digest, camera_binding_table_digest, compile_bundle_problem,
    landmark_seed_table_digest,
};
use fdgr_types::EvidenceDigest;
use std::path::{Path, PathBuf};

const CAMERA_BINDING_HEADER: &str =
    "camera_node_id\tsample_index\tframe_digest\teffective_calibration_digest";
const LANDMARK_SEED_HEADER: &str = "landmark_id\tsource_track_id\tcomponent_root_node_id\tscale_component_root_edge_id\tseed_evidence_digest\tseed_x_nano\tseed_y_nano\tseed_z_nano\tseed_uncertainty_nano";
const OBSERVATION_HEADER: &str = "observation_id\tlandmark_id\tcamera_node_id\tsample_index\tframe_digest\tsource_feature_observation_id\tevidence_digest\tx_nano_pixels\ty_nano_pixels\tlocalization_uncertainty_nano_pixels\tdynamic_masked\trole";
const MAX_CAMERA_BINDING_TABLE_BYTES: u64 = 64 * 1024 * 1024;
const MAX_LANDMARK_SEED_TABLE_BYTES: u64 = 64 * 1024 * 1024;
const MAX_BUNDLE_OBSERVATION_TABLE_BYTES: u64 = 256 * 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct BundleProblemPipelineOptions {
    pub(crate) refinement: PoseRefinementPipelineOptions,
    pub(crate) camera_binding_path: PathBuf,
    pub(crate) camera_binding_file_digest: EvidenceDigest,
    pub(crate) landmark_seed_path: PathBuf,
    pub(crate) landmark_seed_file_digest: EvidenceDigest,
    pub(crate) observation_path: PathBuf,
    pub(crate) observation_file_digest: EvidenceDigest,
    pub(crate) generation: u64,
    pub(crate) policy: BundleProblemPolicy,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct BundleProblemPipelineParser {
    refinement: PoseRefinementPipelineParser,
    camera_binding_path: PathBuf,
    landmark_seed_path: PathBuf,
    observation_path: PathBuf,
    camera_binding_file_digest: Option<EvidenceDigest>,
    landmark_seed_file_digest: Option<EvidenceDigest>,
    observation_file_digest: Option<EvidenceDigest>,
    generation: Option<u64>,
    policy: BundleProblemPolicy,
}

impl BundleProblemPipelineParser {
    pub(crate) fn new(
        node_path: &str,
        edge_path: &str,
        witness_path: &str,
        camera_binding_path: &str,
        landmark_seed_path: &str,
        observation_path: &str,
    ) -> Self {
        Self {
            refinement: PoseRefinementPipelineParser::new(node_path, edge_path, witness_path),
            camera_binding_path: PathBuf::from(camera_binding_path),
            landmark_seed_path: PathBuf::from(landmark_seed_path),
            observation_path: PathBuf::from(observation_path),
            camera_binding_file_digest: None,
            landmark_seed_file_digest: None,
            observation_file_digest: None,
            generation: None,
            policy: BundleProblemPolicy::default(),
        }
    }

    pub(crate) fn apply_option(&mut self, flag: &str, value: &str) -> Result<bool, String> {
        if self.refinement.apply_option(flag, value)? {
            return Ok(true);
        }
        match flag {
            "--camera-binding-file-digest" => {
                self.camera_binding_file_digest =
                    Some(parse_digest_option(value, "camera-binding file digest")?);
            }
            "--landmark-seed-file-digest" => {
                self.landmark_seed_file_digest =
                    Some(parse_digest_option(value, "landmark-seed file digest")?);
            }
            "--bundle-observation-file-digest" => {
                self.observation_file_digest =
                    Some(parse_digest_option(value, "bundle-observation file digest")?);
            }
            "--bundle-problem-generation" => {
                self.generation =
                    Some(parse_nonzero_u64_option(value, "bundle-problem generation")?);
            }
            "--min-optimize-cameras-per-landmark" => {
                self.policy.min_optimize_cameras_per_landmark = parse_nonzero_u32_option(
                    value,
                    "minimum optimize cameras per landmark",
                )?;
            }
            "--min-active-landmarks-per-camera" => {
                self.policy.min_active_landmarks_per_camera = parse_nonzero_u32_option(
                    value,
                    "minimum active landmarks per camera",
                )?;
            }
            "--min-root-active-landmarks" => {
                self.policy.min_root_active_landmarks =
                    parse_nonzero_u32_option(value, "minimum root active landmarks")?;
            }
            "--max-bundle-observation-uncertainty-nano-pixels" => {
                self.policy.max_observation_uncertainty_nano_pixels = parse_nonzero_u64_option(
                    value,
                    "maximum bundle-observation uncertainty nano-pixels",
                )?;
            }
            "--min-held-out-observations-per-component" => {
                self.policy.min_held_out_observations_per_component = parse_nonzero_u32_option(
                    value,
                    "minimum held-out observations per component",
                )?;
            }
            "--min-held-out-cameras-per-component" => {
                self.policy.min_held_out_cameras_per_component = parse_nonzero_u32_option(
                    value,
                    "minimum held-out cameras per component",
                )?;
            }
            "--max-bundle-graph-path-expansions" => {
                self.policy.max_graph_path_expansions = parse_nonzero_u64_option(
                    value,
                    "maximum bundle graph path expansions",
                )?;
            }
            "--max-bundle-operations" => {
                self.policy.max_operations =
                    parse_nonzero_u64_option(value, "maximum bundle operations")?;
            }
            _ => return Ok(false),
        }
        Ok(true)
    }

    pub(crate) fn finish(self) -> Result<BundleProblemPipelineOptions, String> {
        Ok(BundleProblemPipelineOptions {
            refinement: self.refinement.finish()?,
            camera_binding_path: self.camera_binding_path,
            camera_binding_file_digest: self
                .camera_binding_file_digest
                .ok_or_else(|| "missing --camera-binding-file-digest".to_owned())?,
            landmark_seed_path: self.landmark_seed_path,
            landmark_seed_file_digest: self
                .landmark_seed_file_digest
                .ok_or_else(|| "missing --landmark-seed-file-digest".to_owned())?,
            observation_path: self.observation_path,
            observation_file_digest: self
                .observation_file_digest
                .ok_or_else(|| "missing --bundle-observation-file-digest".to_owned())?,
            generation: self
                .generation
                .ok_or_else(|| "missing --bundle-problem-generation".to_owned())?,
            policy: self.policy,
        })
    }
}

pub(crate) fn build_bundle_problem(
    options: &BundleProblemPipelineOptions,
) -> Result<BundleProblemGeneration, String> {
    let pose_refinement = build_pose_refinement(&options.refinement)?;
    let camera_bindings = read_camera_bindings(
        &options.camera_binding_path,
        &options.camera_binding_file_digest,
    )?;
    let landmark_seeds = read_landmark_seeds(
        &options.landmark_seed_path,
        &options.landmark_seed_file_digest,
    )?;
    let observations = read_bundle_observations(
        &options.observation_path,
        &options.observation_file_digest,
    )?;
    let pose_refinement_digest = pose_refinement
        .digest()
        .map_err(|error| format!("pose-refinement identity failed: {error}"))?;
    let camera_binding_basis_digest = camera_binding_table_digest(&camera_bindings)
        .map_err(|error| format!("camera-binding table rejected: {error}"))?;
    let landmark_seed_basis_digest = landmark_seed_table_digest(&landmark_seeds)
        .map_err(|error| format!("landmark-seed table rejected: {error}"))?;
    let observation_basis_digest = bundle_observation_table_digest(&observations)
        .map_err(|error| format!("bundle-observation table rejected: {error}"))?;
    let policy_digest = bundle_problem_policy_digest(options.policy)
        .map_err(|error| format!("bundle-problem policy rejected: {error}"))?;
    compile_bundle_problem(
        BundleProblemBasis {
            pose_refinement_digest,
            camera_binding_basis_digest,
            landmark_seed_basis_digest,
            observation_basis_digest,
            policy_digest,
            generation: options.generation,
        },
        options.policy,
        pose_refinement,
        camera_bindings,
        landmark_seeds,
        observations,
    )
    .map_err(|error| format!("bundle-problem compilation rejected: {error}"))
}

fn read_camera_bindings(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<BundleCameraBinding>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_CAMERA_BINDING_TABLE_BYTES,
        "bundle camera-binding",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "bundle camera-binding table is empty".to_owned())?;
    if header != CAMERA_BINDING_HEADER {
        return Err(format!(
            "bundle camera-binding header mismatch: expected {CAMERA_BINDING_HEADER:?}"
        ));
    }
    let mut bindings = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        reject_empty_line(line, "bundle camera-binding", line_number)?;
        let fields = line.split('\t').collect::<Vec<_>>();
        let [camera_node_id, sample_index, frame_digest, effective_calibration_digest] =
            fields.as_slice()
        else {
            return Err(format!(
                "bundle camera-binding line {line_number} must contain exactly four tab-separated fields"
            ));
        };
        bindings.push(BundleCameraBinding {
            camera_node_id: parse_nonzero_u64_line(camera_node_id, "camera node id", line_number)?,
            sample_index: parse_u64_line(sample_index, "sample index", line_number)?,
            frame_digest: parse_digest(frame_digest, "frame digest", line_number)?,
            effective_calibration_digest: parse_digest(
                effective_calibration_digest,
                "effective calibration digest",
                line_number,
            )?,
        });
        if bindings.len() > MAX_BUNDLE_CAMERAS {
            return Err(format!(
                "bundle camera-binding table contains more than {MAX_BUNDLE_CAMERAS} records"
            ));
        }
    }
    Ok(bindings)
}

fn read_landmark_seeds(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<BundleLandmarkSeed>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_LANDMARK_SEED_TABLE_BYTES,
        "bundle landmark-seed",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "bundle landmark-seed table is empty".to_owned())?;
    if header != LANDMARK_SEED_HEADER {
        return Err(format!(
            "bundle landmark-seed header mismatch: expected {LANDMARK_SEED_HEADER:?}"
        ));
    }
    let mut seeds = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        reject_empty_line(line, "bundle landmark-seed", line_number)?;
        let fields = line.split('\t').collect::<Vec<_>>();
        let [landmark_id, source_track_id, component_root_node_id, scale_component_root_edge_id, seed_evidence_digest, seed_x_nano, seed_y_nano, seed_z_nano, seed_uncertainty_nano] = fields.as_slice() else {
            return Err(format!(
                "bundle landmark-seed line {line_number} must contain exactly nine tab-separated fields"
            ));
        };
        seeds.push(BundleLandmarkSeed {
            landmark_id: parse_nonzero_u64_line(landmark_id, "landmark id", line_number)?,
            source_track_id: parse_nonzero_u64_line(
                source_track_id,
                "source track id",
                line_number,
            )?,
            component_root_node_id: parse_nonzero_u64_line(
                component_root_node_id,
                "component root node id",
                line_number,
            )?,
            scale_component_root_edge_id: parse_optional_nonzero_u64(
                scale_component_root_edge_id,
                "scale component root edge id",
                line_number,
            )?,
            seed_evidence_digest: parse_digest(
                seed_evidence_digest,
                "seed evidence digest",
                line_number,
            )?,
            seed_position_from_root_nano: [
                parse_i64_line(seed_x_nano, "seed x nano", line_number)?,
                parse_i64_line(seed_y_nano, "seed y nano", line_number)?,
                parse_i64_line(seed_z_nano, "seed z nano", line_number)?,
            ],
            seed_uncertainty_nano: parse_u64_line(
                seed_uncertainty_nano,
                "seed uncertainty nano",
                line_number,
            )?,
        });
        if seeds.len() > MAX_BUNDLE_LANDMARKS {
            return Err(format!(
                "bundle landmark-seed table contains more than {MAX_BUNDLE_LANDMARKS} records"
            ));
        }
    }
    Ok(seeds)
}

fn read_bundle_observations(
    path: &Path,
    expected_digest: &EvidenceDigest,
) -> Result<Vec<BundleObservation>, String> {
    let text = read_bound_text(
        path,
        expected_digest,
        MAX_BUNDLE_OBSERVATION_TABLE_BYTES,
        "bundle observation",
    )?;
    let mut lines = text.split_terminator('\n');
    let header = lines
        .next()
        .ok_or_else(|| "bundle observation table is empty".to_owned())?;
    if header != OBSERVATION_HEADER {
        return Err(format!(
            "bundle observation header mismatch: expected {OBSERVATION_HEADER:?}"
        ));
    }
    let mut observations = Vec::new();
    for (offset, line) in lines.enumerate() {
        let line_number = offset.saturating_add(2);
        reject_empty_line(line, "bundle observation", line_number)?;
        let fields = line.split('\t').collect::<Vec<_>>();
        let [observation_id, landmark_id, camera_node_id, sample_index, frame_digest, source_feature_observation_id, evidence_digest, x_nano_pixels, y_nano_pixels, localization_uncertainty_nano_pixels, dynamic_masked, role] = fields.as_slice() else {
            return Err(format!(
                "bundle observation line {line_number} must contain exactly twelve tab-separated fields"
            ));
        };
        observations.push(BundleObservation {
            observation_id: parse_nonzero_u64_line(
                observation_id,
                "observation id",
                line_number,
            )?,
            landmark_id: parse_nonzero_u64_line(landmark_id, "landmark id", line_number)?,
            camera_node_id: parse_nonzero_u64_line(
                camera_node_id,
                "camera node id",
                line_number,
            )?,
            sample_index: parse_u64_line(sample_index, "sample index", line_number)?,
            frame_digest: parse_digest(frame_digest, "frame digest", line_number)?,
            source_feature_observation_id: parse_nonzero_u64_line(
                source_feature_observation_id,
                "source feature observation id",
                line_number,
            )?,
            evidence_digest: parse_digest(evidence_digest, "evidence digest", line_number)?,
            x_nano_pixels: parse_i64_line(x_nano_pixels, "x nano-pixels", line_number)?,
            y_nano_pixels: parse_i64_line(y_nano_pixels, "y nano-pixels", line_number)?,
            localization_uncertainty_nano_pixels: parse_u64_line(
                localization_uncertainty_nano_pixels,
                "localization uncertainty nano-pixels",
                line_number,
            )?,
            dynamic_masked: parse_bool_line(dynamic_masked, "dynamic masked", line_number)?,
            role: parse_role(role, line_number)?,
        });
        if observations.len() > MAX_BUNDLE_OBSERVATIONS {
            return Err(format!(
                "bundle observation table contains more than {MAX_BUNDLE_OBSERVATIONS} records"
            ));
        }
    }
    Ok(observations)
}

fn parse_nonzero_u64_line(value: &str, label: &str, line: usize) -> Result<u64, String> {
    parse_nonzero_u64_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_u64_line(value: &str, label: &str, line: usize) -> Result<u64, String> {
    parse_u64_option(value, label).map_err(|error| format!("line {line}: {error}"))
}

fn parse_i64_line(value: &str, label: &str, line: usize) -> Result<i64, String> {
    value
        .parse::<i64>()
        .map_err(|error| format!("line {line}: invalid {label} {value:?}: {error}"))
}

fn parse_optional_nonzero_u64(
    value: &str,
    label: &str,
    line: usize,
) -> Result<Option<u64>, String> {
    if value == "none" {
        Ok(None)
    } else {
        parse_nonzero_u64_line(value, label, line).map(Some)
    }
}

fn parse_bool_line(value: &str, label: &str, line: usize) -> Result<bool, String> {
    match value {
        "true" => Ok(true),
        "false" => Ok(false),
        _ => Err(format!(
            "line {line}: invalid {label} {value:?}; expected true or false"
        )),
    }
}

fn parse_role(value: &str, line: usize) -> Result<BundleObservationRole, String> {
    match value {
        "optimize" => Ok(BundleObservationRole::Optimize),
        "held_out" => Ok(BundleObservationRole::HeldOut),
        _ => Err(format!(
            "line {line}: invalid observation role {value:?}; expected optimize or held_out"
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

#[cfg(test)]
mod tests {
    use super::{parse_bool_line, parse_optional_nonzero_u64, parse_role};
    use fdgr_bundle_problem::BundleObservationRole;

    #[test]
    fn nullable_scale_root_and_closed_enums_are_exact() {
        assert_eq!(
            parse_optional_nonzero_u64("none", "scale root", 2),
            Ok(None)
        );
        assert_eq!(
            parse_optional_nonzero_u64("10", "scale root", 2),
            Ok(Some(10))
        );
        assert!(parse_optional_nonzero_u64("0", "scale root", 2).is_err());
        assert_eq!(parse_bool_line("true", "masked", 2), Ok(true));
        assert!(parse_bool_line("1", "masked", 2).is_err());
        assert_eq!(parse_role("held_out", 2), Ok(BundleObservationRole::HeldOut));
        assert!(parse_role("test", 2).is_err());
    }
}
