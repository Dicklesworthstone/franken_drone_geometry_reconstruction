#![forbid(unsafe_code)]
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::doc_markdown,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::needless_range_loop,
    clippy::needless_pass_by_value,
    clippy::similar_names,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::type_complexity
)]
//! Deterministic component-relative camera-center initialization.
//!
//! The initializer consumes exact pose-graph orientations and relative edge-baseline gauges. It
//! preserves every arbitrary component gauge and never upgrades relative positions to metric or
//! optimized trajectory authority.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_edge_scale::{
    EdgeScaleComponentStatus, EdgeScaleError, EdgeScaleGeneration, ReconciledEdgeScale,
};
use fdgr_pose_graph::{
    OrientationStatus, PoseGraphError, PoseGraphGeneration, RelativePoseEdge,
};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("implementation.inc");
include!("tests.inc");
