#![forbid(unsafe_code)]
#![allow(
    clippy::cast_lossless,
    clippy::doc_markdown,
    clippy::indexing_slicing,
    clippy::missing_errors_doc,
    clippy::module_name_repetitions,
    clippy::must_use_candidate,
    clippy::needless_pass_by_value,
    clippy::similar_names,
    clippy::struct_field_names,
    clippy::too_many_lines,
    clippy::type_complexity
)]
//! Deterministic correlation-aware reconciliation of relative pose-edge baselines.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_graph::{
    EdgePriority, GraphAnalysis, GraphBasis, GraphBudget, GraphEdge, GraphError, GraphNode,
    analyze_graph,
};
use fdgr_pose_graph::{PoseGraphError, PoseGraphGeneration};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types_core.inc");
include!("types_output.inc");
include!("error.inc");
include!("math.inc");
include!("build.inc");
include!("validate.inc");
include!("aggregate.inc");
include!("consensus.inc");
include!("topology.inc");
include!("propagate.inc");
include!("components.inc");
include!("model.inc");
include!("encoding.inc");
include!("tests.inc");
