#![forbid(unsafe_code)]
#![allow(clippy::module_name_repetitions)]
//! Bounded native ISO Base Media File Format metadata and classic-sample inspection.
//!
//! This crate reads container structure, track metadata, classic sample-table summaries, and
//! explicitly bounded exact sample windows. It does not decode compressed media, infer missing
//! timestamps, parse fragmented sample runs, or replace the separately supervised FFmpeg oracle.

mod error;
mod fourcc;
mod iso_bmff;
mod sample_index;

pub use error::MediaError;
pub use fourcc::FourCc;
pub use iso_bmff::{
    IsoBmffSummary, ParseLimits, TrackSummary, inspect_iso_bmff, inspect_iso_bmff_file,
};
pub use sample_index::{
    DEFAULT_SAMPLE_INDEX_SCAN_BUDGET, DEFAULT_SAMPLE_WINDOW_RECORDS, SampleIndexError, SampleRecord,
    SampleWindowLimits, SampleWindowRequest, TrackSampleWindow, read_classic_sample_window,
    read_classic_sample_window_file,
};

/// Public schema identity for bounded media-inspection output.
pub const MEDIA_INSPECTION_SCHEMA: &str = "fdgr.media_inspection/1";
/// Public schema identity for bounded exact classic-sample windows.
pub const SAMPLE_WINDOW_SCHEMA: &str = "fdgr.sample_window/1";
