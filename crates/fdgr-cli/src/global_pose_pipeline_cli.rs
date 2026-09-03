#![forbid(unsafe_code)]
//! Shared exact construction for component-relative global-pose and refinement CLI operations.

use crate::pose_graph_input_cli::{
    parse_digest_option, parse_nonzero_u64_option, parse_u64_option,
};
use crate::pose_scale_pipeline_cli::{
    PoseScalePipelineOptions, PoseScalePipelineParser, build_pose_and_edge_scale,
};
use fdgr_global_pose::{
    GlobalPoseBasis, GlobalPoseInitialization, GlobalPosePolicy, initialize_global_pose,
};
use fdgr_types::EvidenceDigest;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct GlobalPosePipelineOptions {
    pub(crate) pose_scale: PoseScalePipelineOptions,
    pub(crate) policy_digest: EvidenceDigest,
    pub(crate) generation: u64,
    pub(crate) policy: GlobalPosePolicy,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct GlobalPosePipelineParser {
    pose_scale: PoseScalePipelineParser,
    policy_digest: Option<EvidenceDigest>,
    generation: Option<u64>,
    policy: GlobalPosePolicy,
}

impl GlobalPosePipelineParser {
    pub(crate) fn new(node_path: &str, edge_path: &str, witness_path: &str) -> Self {
        Self {
            pose_scale: PoseScalePipelineParser::new(node_path, edge_path, witness_path),
            policy_digest: None,
            generation: None,
            policy: GlobalPosePolicy::default(),
        }
    }

    pub(crate) fn apply_option(&mut self, flag: &str, value: &str) -> Result<bool, String> {
        if self.pose_scale.apply_option(flag, value)? {
            return Ok(true);
        }
        match flag {
            "--global-pose-policy-digest" => {
                self.policy_digest = Some(parse_digest_option(value, "global-pose policy digest")?);
            }
            "--global-pose-generation" => {
                self.generation = Some(parse_nonzero_u64_option(value, "global-pose generation")?);
            }
            "--max-translation-cycle-residual-ppm" => {
                self.policy.max_translation_cycle_residual_ppm =
                    parse_u64_option(value, "maximum translation cycle residual ppm")?;
            }
            "--max-camera-center-abs-nano" => {
                self.policy.max_camera_center_abs_nano =
                    parse_nonzero_u64_option(value, "maximum camera-center magnitude nano")?;
            }
            "--max-global-pose-operations" => {
                self.policy.max_operations =
                    parse_nonzero_u64_option(value, "maximum global-pose operations")?;
            }
            _ => return Ok(false),
        }
        Ok(true)
    }

    pub(crate) fn finish(self) -> Result<GlobalPosePipelineOptions, String> {
        Ok(GlobalPosePipelineOptions {
            pose_scale: self.pose_scale.finish()?,
            policy_digest: self
                .policy_digest
                .ok_or_else(|| "missing --global-pose-policy-digest".to_owned())?,
            generation: self
                .generation
                .ok_or_else(|| "missing --global-pose-generation".to_owned())?,
            policy: self.policy,
        })
    }
}

pub(crate) fn build_global_pose(
    options: &GlobalPosePipelineOptions,
) -> Result<GlobalPoseInitialization, String> {
    let (pose_graph, edge_scale) = build_pose_and_edge_scale(&options.pose_scale)?;
    let pose_graph_generation_digest = pose_graph
        .digest()
        .map_err(|error| format!("pose-graph identity failed: {error}"))?;
    let edge_scale_generation_digest = edge_scale
        .digest()
        .map_err(|error| format!("edge-scale identity failed: {error}"))?;
    initialize_global_pose(
        GlobalPoseBasis {
            pose_graph_generation_digest,
            edge_scale_generation_digest,
            policy_digest: options.policy_digest.clone(),
            generation: options.generation,
        },
        options.policy,
        pose_graph,
        edge_scale,
    )
    .map_err(|error| format!("global-pose initialization rejected: {error}"))
}
