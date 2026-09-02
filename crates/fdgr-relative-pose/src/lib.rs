#![forbid(unsafe_code)]
#![allow(
    clippy::cast_lossless,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::similar_names,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::type_complexity
)]
//! Deterministic fixed-point adjudication of two-view relative-pose candidates.
//!
//! The crate does not generate pose candidates and does not promote descriptor matches directly to
//! pose authority. It evaluates an exact candidate set against exact calibrated bearing matches,
//! retaining epipolar, parallax, cheirality, ambiguity, and operation-cost evidence.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::cmp::Ordering;
use std::collections::BTreeSet;
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("error.inc");
include!("math.inc");
include!("verify.inc");
include!("model.inc");
include!("tests.inc");
