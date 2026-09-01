#![forbid(unsafe_code)]
//! Bounded, identity-carrying protocol for external media decode workers.
//!
//! This crate defines plans, framehash evidence, termination classes, and receipt validation. It
//! does not spawn a process and therefore does not claim an admitted FFmpeg execution path.

mod framehash;
mod plan;
mod receipt;
mod receipt_json;

pub use framehash::{
    FrameHashError, FrameHashLimits, FrameHashRecord, FrameHashReport, parse_framehash_v2,
};
pub use plan::{DecodePixelFormat, DecodePlanError, MediaDecodePlan, MediaDecodePlanInput};
pub use receipt::{
    DecodeReceiptError, DecodeTermination, MediaDecodeReceipt, MediaDecodeReceiptInput,
};
pub use receipt_json::render_media_decode_receipt_json;

/// Public schema identity for deterministic decode plans.
pub const MEDIA_DECODE_PLAN_SCHEMA: &str = "fdgr.media_decode_plan/1";
/// Public schema identity for worker receipts.
pub const MEDIA_DECODE_RECEIPT_SCHEMA: &str = "fdgr.media_decode_receipt/1";
