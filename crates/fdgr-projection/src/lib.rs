#![forbid(unsafe_code)]
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::doc_markdown,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::similar_names,
    clippy::too_many_lines
)]
//! Deterministic fixed-point projection kernels for exact calibrated image domains.
//!
//! This crate projects points already expressed in a camera frame through an exact derived
//! calibration. Global-shutter pinhole and Brown-Conrady domains are supported. Rolling-shutter
//! projection still fails closed until an exact row-time motion model is supplied.

use fdgr_calibration::{CalibrationError, DerivedCalibration, DistortionModel, NANO_SCALE};
use std::error::Error;
use std::fmt::{self, Display, Formatter};

include!("types.inc");
include!("project.inc");
include!("tests.inc");
