# Research Sources and Exact Design Basis

FDGR's architectural research is retained in three layers:

1. [`research/source-inventory/source_manifest.json`](research/source-inventory/source_manifest.json)
   freezes exact public commit and tree identities inspected on 2026-08-31.
2. [`research/deep-dives/`](research/deep-dives/) records one mechanism-level analysis per sibling.
3. [`architecture/deep_traceability.json`](architecture/deep_traceability.json) maps transferred
   mechanisms into FDGR's abstraction tower and admission boundaries.

These are design inputs, not automatically admitted dependencies. Current external device/model and
standard sources are summarized in `DJI_ADAPTER_RESEARCH.md`, `MODEL_REGISTRY.md`, and the
comprehensive plan. A release uses `release/source_closure.lock.json` plus local Doodlestein receipts,
not the moving heads of any repository.
