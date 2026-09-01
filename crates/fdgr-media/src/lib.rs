#![forbid(unsafe_code)]
#![allow(clippy::module_name_repetitions)]
//! Bounded native ISO Base Media File Format metadata inspection.
//!
//! This crate reads container structure, track metadata, and classic sample-table summaries. It
//! does not decode compressed media, infer missing timestamps, parse fragmented sample runs, or
//! replace the separately supervised FFmpeg oracle.

mod error;
mod fourcc;
mod iso_bmff;

pub use error::MediaError;
pub use fourcc::FourCc;
pub use iso_bmff::{
    IsoBmffSummary, ParseLimits, TrackSummary, inspect_iso_bmff, inspect_iso_bmff_file,
};

/// Public schema identity for bounded media-inspection output.
pub const MEDIA_INSPECTION_SCHEMA: &str = "fdgr.media_inspection/1";
