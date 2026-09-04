#![forbid(unsafe_code)]
#![allow(
    clippy::cast_lossless,
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
//! Deterministic structural admission for relative bundle problems.
//!
//! The compiler authenticates camera/calibration bindings, landmark seeds, and optimize versus
//! held-out observations against one exact pose-refinement generation. It computes a stable
//! support core and bipartite topology certificate, but performs no triangulation, reprojection
//! minimization, covariance estimation, metric-scale admission, or sparse-geometry publication.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_graph::{
    EdgePriority, GraphAnalysis, GraphBasis, GraphBudget, GraphEdge, GraphError, GraphNode,
    analyze_graph,
};
use fdgr_pose_refinement::{
    PoseRefinementDecision, PoseRefinementError, PoseRefinementGeneration,
};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("compile_api.inc");
include!("compile_validate.inc");
include!("compile_support.inc");
include!("compile_graph_build.inc");
include!("compile_components.inc");
include!("compile_helpers.inc");
include!("model.inc");
include!("tests.inc");
