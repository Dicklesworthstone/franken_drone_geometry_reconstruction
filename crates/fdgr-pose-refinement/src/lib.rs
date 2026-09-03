#![forbid(unsafe_code)]
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::doc_markdown,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::needless_pass_by_value,
    clippy::similar_names,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::type_complexity
)]
//! Deterministic translation-only refinement of component-relative camera centers.
//!
//! This reference solver keeps admitted orientations and relative edge-scale gauges fixed. It
//! relaxes camera centers against comparable pose-edge displacement factors, pins every component
//! root, preserves upstream conflicts, and never upgrades the result to metric, landmark, global
//! trajectory, or full bundle-adjustment authority.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_edge_scale::ReconciledEdgeScale;
use fdgr_global_pose::{
    GlobalPoseComponentStatus, GlobalPoseError, GlobalPoseInitialization,
};
use fdgr_pose_graph::RelativePoseEdge;
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::BTreeMap;
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("refine.inc");
include!("model.inc");
include!("tests.inc");
