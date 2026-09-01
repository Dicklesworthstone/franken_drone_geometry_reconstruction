#![forbid(unsafe_code)]
//! Reference immutable local object store with root-last publication.
//!
//! This crate establishes filesystem semantics without claiming race-free hostile-path
//! confinement. Production admission still requires the FrankenFS capability and fault gates.

mod error;
mod layout;
mod store;
mod verified;

pub use error::ObjectStoreError;
pub use store::{ImportReceipt, LocalObjectStore, StoreEntryStatus};
pub use verified::VerifiedObject;

/// Public schema identity for successful local import receipts.
pub const IMPORT_RECEIPT_SCHEMA: &str = "fdgr.local_import_receipt/1";
/// Maximum attempts to allocate a unique staging directory in one process.
pub const MAX_STAGING_ATTEMPTS: u64 = 1024;
/// Maximum unfinished staging directories returned by one diagnostic scan.
pub const MAX_STAGING_ENTRIES: usize = 4096;
/// Canonical object-file suffix.
pub(crate) const OBJECT_SUFFIX: &str = ".fdgr-object";
/// Canonical manifest-file suffix.
pub(crate) const MANIFEST_SUFFIX: &str = ".fdgr-manifest";
