#![forbid(unsafe_code)]
#![allow(
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
//! Evidence-bound frame-quality and deterministic keyframe selection for FDGR.
//!
//! Candidate metrics are exact evidence inputs. The reference selector applies hard image-quality
//! gates, then maximizes marginal visibility and viewpoint diversity under a bounded capacity and
//! temporal-spacing policy. Every rejected candidate retains a stable explanation.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::BTreeSet;
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("error.inc");
include!("helpers.inc");
include!("select.inc");
include!("model.inc");
include!("tests.inc");
