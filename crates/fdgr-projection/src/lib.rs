#![forbid(unsafe_code)]
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::doc_markdown,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::similar_names
)]
//! Deterministic fixed-point projection kernels for explicitly rectified camera domains.
//!
//! This crate projects points already expressed in a camera frame through an exact derived
//! calibration. The initial reference path accepts only global-shutter, distortion-free image
//! domains. Rolling-shutter and distorted projection require additional motion or distortion
//! semantics and therefore fail closed rather than being approximated.

use fdgr_calibration::{CalibrationError, DerivedCalibration, DistortionModel, NANO_SCALE};
use std::error::Error;
use std::fmt::{self, Display, Formatter};

include!("types.inc");
include!("project.inc");
include!("tests.inc");
