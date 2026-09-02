#![forbid(unsafe_code)]
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
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
//! Bounded deterministic verification of calibrated epipolar hypotheses.
//!
//! Descriptor correspondences and essential matrices remain proposals. This crate normalizes
//! homogeneous candidates, checks the essential cubic manifold, scores every exact match against
//! uncertainty-aware geometric gates, preserves rejected and ambiguous alternatives, and grants
//! no pose or geometry authority.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("error.inc");
include!("helpers.inc");
include!("essential.inc");
include!("evaluate.inc");
include!("model.inc");
include!("tests.inc");
