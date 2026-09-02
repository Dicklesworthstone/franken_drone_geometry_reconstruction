#![forbid(unsafe_code)]
#![allow(
    clippy::cast_lossless,
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::needless_pass_by_value,
    clippy::similar_names,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_arguments,
    clippy::too_many_lines
)]
//! Bounded deterministic descriptor matching and collision-safe feature-track assembly.
//!
//! Pairwise descriptor matches remain hypotheses. This crate preserves ambiguity and rejection
//! evidence, enforces one observation per frame in every track, and never claims epipolar or pose
//! correctness.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("error.inc");
include!("helpers.inc");
include!("matching.inc");
include!("tracks.inc");
include!("model.inc");
include!("tests.inc");
