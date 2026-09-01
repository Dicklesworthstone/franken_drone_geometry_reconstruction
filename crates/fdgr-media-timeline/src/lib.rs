#![forbid(unsafe_code)]
#![allow(
    clippy::module_name_repetitions,
    clippy::needless_pass_by_value,
    clippy::similar_names,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_lines
)]
//! Canonical custody-bound sample timelines for recorded media.
//!
//! This crate converts an already validated classic-sample window into a deterministic timeline
//! bound to exact recorded-media and source-object identities. It makes partial coverage, decode
//! discontinuities, composition offsets, presentation reordering, source-byte reordering, and
//! sample-description changes explicit. It does not decode media, infer missing samples, repair
//! timestamps, or establish a clock relationship to telemetry.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_media::{SampleRecord, TrackSampleWindow};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::collections::BTreeSet;
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

include!("types.inc");
include!("timeline_impl.inc");
include!("build.inc");
include!("helpers.inc");
include!("error.inc");
include!("tests.inc");
