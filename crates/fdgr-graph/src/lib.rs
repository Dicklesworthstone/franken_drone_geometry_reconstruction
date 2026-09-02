#![forbid(unsafe_code)]
#![allow(
    clippy::cast_lossless,
    clippy::doc_markdown,
    clippy::indexing_slicing,
    clippy::missing_errors_doc,
    clippy::must_use_candidate,
    clippy::module_name_repetitions,
    clippy::struct_field_names,
    clippy::too_many_lines,
    clippy::type_complexity
)]
//! Deterministic bounded graph topology and fundamental-cycle analysis.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("analysis.inc");
include!("topology.inc");
include!("model.inc");
include!("tests.inc");
