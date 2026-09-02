#![forbid(unsafe_code)]
#![allow(
    clippy::cast_lossless,
    clippy::doc_markdown,
    clippy::indexing_slicing,
    clippy::missing_errors_doc,
    clippy::module_name_repetitions,
    clippy::must_use_candidate,
    clippy::struct_field_names,
    clippy::too_many_lines,
    clippy::type_complexity
)]
//! Deterministic component-local orientation propagation and cycle evidence.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_graph::{
    EdgePriority, GraphAnalysis, GraphBasis, GraphBudget, GraphEdge, GraphError, GraphNode,
    analyze_graph,
};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("math.inc");
include!("build.inc");
include!("model.inc");
include!("tests.inc");
