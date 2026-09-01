#![forbid(unsafe_code)]
//! Deterministic append-only evidence ledger and replay oracle.

mod error;
mod event;
mod kind;
mod ledger;

pub use error::LedgerError;
pub use event::LedgerEvent;
pub use kind::{EventKind, EventKindError};
pub use ledger::{EventPage, LedgerAnchor, ReferenceLedger};

/// Canonical internal ledger-event schema identity.
pub const LEDGER_EVENT_SCHEMA: &str = "fdgr.ledger_event/1";
/// Canonical ledger-anchor schema identity.
pub const LEDGER_ANCHOR_SCHEMA: &str = "fdgr.ledger_anchor/1";
/// Maximum UTF-8 bytes in an event kind.
pub const MAX_EVENT_KIND_BYTES: usize = 96;
/// Maximum events returned by one bounded page.
pub const MAX_PAGE_EVENTS: usize = 4096;
/// Maximum canonical event bytes accepted by the decoder.
pub const MAX_EVENT_BYTES: usize = 16 * 1024;

pub(crate) const EVENT_VERSION: u16 = 1;
pub(crate) const EVENT_DOMAIN: &str = "fdgr.ledger_event/1";
pub(crate) const ANCHOR_DOMAIN: &str = "fdgr.ledger_anchor/1";
