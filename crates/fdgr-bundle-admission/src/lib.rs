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
    clippy::too_many_lines,
    clippy::type_complexity
)]
//! Deterministic image-domain, seed-provenance, and held-out-independence admission.
//!
//! This crate audits an exact structural bundle problem before any numerical optimizer may consume
//! it. It does not estimate calibration, triangulate or refine landmarks, minimize reprojection
//! error, prove numerical rank, admit metric scale, or publish sparse geometry.

use fdgr_bundle_problem::{
    BundleCameraDisposition, BundleLandmarkDisposition, BundleObservationDisposition,
    BundleObservationRole, BundleProblemDecision, BundleProblemError, BundleProblemGeneration,
};
use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("audit.inc");
include!("model.inc");
include!("tests.inc");
