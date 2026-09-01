#![forbid(unsafe_code)]
//! Authorized regular-file source adapter and evidence-ledger coordinator.
//!
//! Exact bytes are published before their ledger event. If the append basis becomes stale after
//! storage, the adapter reports `StoredAwaitingLedger` and preserves both the durable evidence and
//! the current anchor needed for reconciliation. It never pretends filesystem publication rolled
//! back.

use fdgr_ledger::{EventKind, LedgerAnchor, LedgerError, LedgerEvent, ReferenceLedger};
use fdgr_object_store::{ImportReceipt, LocalObjectStore, ObjectStoreError};
use std::error::Error;
use std::fmt::{self, Display, Formatter};
use std::path::Path;

/// Canonical semantic event kind for a published original-file representation.
pub const ORIGINAL_MEDIA_IMPORTED_KIND: &str = "original_media_imported";

/// Immutable result of publishing exact source bytes, before ledger append.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct StoredSourceFile {
    /// Root-last local-store receipt.
    pub receipt: ImportReceipt,
}

/// Terminal status of the file-source integration step.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SourceFileImportStatus {
    /// Evidence is published and a new ledger event was appended.
    Completed,
    /// Evidence and the same semantic event were already present.
    AlreadyRecorded,
    /// Evidence is published, but the supplied ledger basis is stale.
    StoredAwaitingLedger,
}

impl SourceFileImportStatus {
    /// Stable lower-snake-case machine text.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Completed => "completed",
            Self::AlreadyRecorded => "already_recorded",
            Self::StoredAwaitingLedger => "stored_awaiting_ledger",
        }
    }
}

/// Evidence-preserving source-file integration outcome.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SourceFileImportOutcome {
    /// Semantic outcome status.
    pub status: SourceFileImportStatus,
    /// Durable evidence publication receipt.
    pub storage: ImportReceipt,
    /// New or existing event when the ledger already records the import.
    pub event: Option<LedgerEvent>,
    /// Exact current ledger anchor after the attempt.
    pub anchor: LedgerAnchor,
}

/// Stores exact source bytes without appending a ledger event.
///
/// # Errors
///
/// Returns a typed store/source/manifest/publication error.
pub fn store_original_file(
    store: &mut LocalObjectStore,
    source: impl AsRef<Path>,
    chunk_size: u32,
) -> Result<StoredSourceFile, SourceFileError> {
    let receipt = store.import_file(source, chunk_size)?;
    Ok(StoredSourceFile { receipt })
}

/// Records previously stored evidence against an optimistic ledger anchor.
///
/// If the same event already exists, this is an idempotent success independent of the supplied
/// anchor. If the event is new and the supplied anchor is stale, durable evidence remains visible
/// and the result is `StoredAwaitingLedger` rather than an error or blind retry.
///
/// # Errors
///
/// Returns a typed event-kind, anchor-validation, or ledger-identity error.
pub fn record_stored_original(
    ledger: &mut ReferenceLedger,
    expected_anchor: &LedgerAnchor,
    stored: &StoredSourceFile,
) -> Result<SourceFileImportOutcome, SourceFileError> {
    let kind = EventKind::parse(ORIGINAL_MEDIA_IMPORTED_KIND).map_err(LedgerError::from)?;
    if let Some(existing) = ledger.events().iter().find(|event| {
        event.kind == kind && event.payload_root == stored.receipt.manifest_digest
    }) {
        return Ok(SourceFileImportOutcome {
            status: SourceFileImportStatus::AlreadyRecorded,
            storage: stored.receipt.clone(),
            event: Some(existing.clone()),
            anchor: ledger.anchor()?,
        });
    }

    expected_anchor.validate()?;
    let current = ledger.anchor()?;
    if expected_anchor != &current {
        return Ok(SourceFileImportOutcome {
            status: SourceFileImportStatus::StoredAwaitingLedger,
            storage: stored.receipt.clone(),
            event: None,
            anchor: current,
        });
    }

    let event = ledger.append(
        expected_anchor,
        kind,
        stored.receipt.manifest_digest.clone(),
    )?;
    Ok(SourceFileImportOutcome {
        status: SourceFileImportStatus::Completed,
        storage: stored.receipt.clone(),
        event: Some(event),
        anchor: ledger.anchor()?,
    })
}

/// Performs the reference end-to-end file import when the append basis is current at entry.
///
/// This convenience path checks the ledger basis before filesystem work, then stores and records.
/// More concurrent implementations must preserve the same `StoredAwaitingLedger` outcome if the
/// basis changes after evidence publication.
///
/// # Errors
///
/// Returns a typed stale-entry, store, event-kind, or ledger error.
pub fn import_original_file(
    store: &mut LocalObjectStore,
    ledger: &mut ReferenceLedger,
    expected_anchor: &LedgerAnchor,
    source: impl AsRef<Path>,
    chunk_size: u32,
) -> Result<SourceFileImportOutcome, SourceFileError> {
    expected_anchor.validate()?;
    let current = ledger.anchor()?;
    if expected_anchor != &current {
        return Err(SourceFileError::Ledger(LedgerError::StaleAnchor {
            expected_digest: expected_anchor.anchor_digest.clone(),
            observed_digest: current.anchor_digest,
            expected_count: expected_anchor.event_count,
            observed_count: current.event_count,
        }));
    }
    let stored = store_original_file(store, source, chunk_size)?;
    record_stored_original(ledger, expected_anchor, &stored)
}

/// Stable file-source adapter failures.
#[derive(Debug)]
pub enum SourceFileError {
    /// Filesystem evidence publication failed.
    Store(ObjectStoreError),
    /// Ledger identity, anchor, replay, or append failed.
    Ledger(LedgerError),
}

impl Display for SourceFileError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Store(error) => write!(formatter, "source-file store error: {error}"),
            Self::Ledger(error) => write!(formatter, "source-file ledger error: {error}"),
        }
    }
}

impl Error for SourceFileError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Store(error) => Some(error),
            Self::Ledger(error) => Some(error),
        }
    }
}

impl From<ObjectStoreError> for SourceFileError {
    fn from(error: ObjectStoreError) -> Self {
        Self::Store(error)
    }
}

impl From<LedgerError> for SourceFileError {
    fn from(error: LedgerError) -> Self {
        Self::Ledger(error)
    }
}

#[cfg(all(test, unix))]
mod tests {
    use super::{
        SourceFileImportStatus, import_original_file, record_stored_original,
        store_original_file,
    };
    use fdgr_codec::hash_bytes;
    use fdgr_ledger::{EventKind, ReferenceLedger};
    use fdgr_object_store::LocalObjectStore;
    use std::fs;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

    fn test_root(label: &str) -> PathBuf {
        let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
        std::env::temp_dir().join(format!(
            "fdgr-source-file-{label}-{}-{id}",
            std::process::id()
        ))
    }

    fn prepare(label: &str, bytes: &[u8]) -> Option<(PathBuf, PathBuf)> {
        let root = test_root(label);
        if fs::create_dir_all(&root).is_err() {
            return None;
        }
        let source = root.join("source.mp4");
        if fs::write(&source, bytes).is_err() {
            return None;
        }
        Some((root, source))
    }

    #[test]
    fn import_publishes_evidence_then_records_manifest_root() {
        let prepared = prepare("complete", b"synthetic original media");
        assert!(prepared.is_some());
        if let Some((root, source)) = prepared {
            let store = LocalObjectStore::open(root.join("store"));
            assert!(store.is_ok());
            if let Ok(mut store) = store {
                let mut ledger = ReferenceLedger::new(hash_bytes(b"capture-lineage"), 0);
                let anchor = ledger.anchor();
                assert!(anchor.is_ok());
                if let Ok(anchor) = anchor {
                    let outcome = import_original_file(
                        &mut store,
                        &mut ledger,
                        &anchor,
                        &source,
                        4,
                    );
                    assert!(matches!(
                        outcome,
                        Ok(ref value)
                            if value.status == SourceFileImportStatus::Completed
                                && value.anchor.event_count == 1
                                && matches!(
                                    value.event.as_ref(),
                                    Some(event)
                                        if event.payload_root == value.storage.manifest_digest
                                )
                    ));
                    if let Ok(outcome) = outcome {
                        assert!(store
                            .verify_manifest(&outcome.storage.manifest_digest)
                            .is_ok());
                    }
                }
            }
            assert!(fs::remove_dir_all(root).is_ok());
        }
    }

    #[test]
    fn stale_after_storage_is_explicit_and_reconcilable() {
        let prepared = prepare("stale", b"stored before stale append");
        assert!(prepared.is_some());
        if let Some((root, source)) = prepared {
            let store = LocalObjectStore::open(root.join("store"));
            assert!(store.is_ok());
            if let Ok(mut store) = store {
                let mut ledger = ReferenceLedger::new(hash_bytes(b"capture-lineage"), 0);
                let stale = ledger.anchor();
                let stored = store_original_file(&mut store, &source, 5);
                assert!(stale.is_ok());
                assert!(stored.is_ok());
                if let (Ok(stale), Ok(stored)) = (stale, stored) {
                    let current = ledger.anchor();
                    let kind = EventKind::parse("unrelated_event");
                    assert!(current.is_ok());
                    assert!(kind.is_ok());
                    if let (Ok(current), Ok(kind)) = (current, kind) {
                        assert!(ledger.append(&current, kind, hash_bytes(b"other")).is_ok());
                    }
                    let outcome = record_stored_original(&mut ledger, &stale, &stored);
                    assert!(matches!(
                        outcome,
                        Ok(ref value)
                            if value.status == SourceFileImportStatus::StoredAwaitingLedger
                                && value.event.is_none()
                                && value.anchor.event_count == 1
                    ));
                    let fresh = ledger.anchor();
                    assert!(fresh.is_ok());
                    if let Ok(fresh) = fresh {
                        assert!(matches!(
                            record_stored_original(&mut ledger, &fresh, &stored),
                            Ok(ref value)
                                if value.status == SourceFileImportStatus::Completed
                                    && value.anchor.event_count == 2
                        ));
                    }
                }
            }
            assert!(fs::remove_dir_all(root).is_ok());
        }
    }

    #[test]
    fn retry_is_semantically_idempotent() {
        let prepared = prepare("idempotent", b"same exact source");
        assert!(prepared.is_some());
        if let Some((root, source)) = prepared {
            let store = LocalObjectStore::open(root.join("store"));
            assert!(store.is_ok());
            if let Ok(mut store) = store {
                let mut ledger = ReferenceLedger::new(hash_bytes(b"capture-lineage"), 0);
                let first_anchor = ledger.anchor();
                assert!(first_anchor.is_ok());
                if let Ok(first_anchor) = first_anchor {
                    assert!(import_original_file(
                        &mut store,
                        &mut ledger,
                        &first_anchor,
                        &source,
                        4,
                    )
                    .is_ok());
                }
                let stored = store_original_file(&mut store, &source, 4);
                let current = ledger.anchor();
                assert!(stored.is_ok());
                assert!(current.is_ok());
                if let (Ok(stored), Ok(current)) = (stored, current) {
                    assert!(matches!(
                        record_stored_original(&mut ledger, &current, &stored),
                        Ok(ref value)
                            if value.status == SourceFileImportStatus::AlreadyRecorded
                                && value.anchor.event_count == 1
                    ));
                    assert_eq!(ledger.len(), 1);
                }
            }
            assert!(fs::remove_dir_all(root).is_ok());
        }
    }
}
