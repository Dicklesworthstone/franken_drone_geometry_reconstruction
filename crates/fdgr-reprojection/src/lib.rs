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
//! Deterministic calibrated reprojection evidence for audited relative bundle problems.
//!
//! This crate materializes exact effective calibrations and evaluates the currently admitted
//! component-relative camera and landmark state. It does not optimize any state or grant metric,
//! numerical-rank, covariance, held-out-improvement, or sparse-publication authority.

use fdgr_bundle_admission::{
    BundleAdmissionDecision, BundleAdmissionError, BundleAdmissionGeneration,
    ObservationAuditDisposition,
};
use fdgr_bundle_problem::{BundleObservationDisposition, BundleObservationRole};
use fdgr_calibration::{CalibrationError, DerivedCalibration, MAX_READOUT_TIME_NS, NANO_SCALE};
use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_projection::{
    CalibratedProjectionPolicy, MAX_CAMERA_POINT_ABS_NANO,
    MAX_NORMALIZED_COORDINATE_ABS_NANO, MAX_PROJECTED_COORDINATE_ABS_NANO_PIXELS,
    ProjectionError, project_calibrated_camera_point,
};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("math.inc");
include!("evaluate.inc");
include!("model.inc");
include!("tests.inc");
