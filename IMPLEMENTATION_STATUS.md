# Implementation Status

**Snapshot:** 2026-09-01

FDGR now contains a growing set of deterministic, dependency-free, safe-Rust reference
implementations. These references establish semantics and public evidence boundaries; they are not
by themselves production qualification for live DJI acquisition, external process supervision,
metric reconstruction, semantic recognition, or cloud recovery.

## Implemented reference surfaces

| Surface | Current status | Evidence boundary |
|---|---|---|
| Rust workspace | **14 workspace package targets after this wave** | Strict edition-2024 workspace, pinned nightly, `unsafe_code = forbid`, no external Rust dependencies |
| Canonical identity and codec | Reference implemented | Streaming SHA-256, domain-separated identities, bounded deterministic codecs, typed decode failures |
| Evidence ledger | Reference implemented | Append/replay invariants and exact event identities; FrankenSQLite production adapter not admitted |
| Local immutable object store | Reference implemented | Object-first, manifest-root-last publication, readback, collision refusal, staging diagnostics; hostile-path and full crash qualification remain open |
| Native ISO BMFF inspection | Reference implemented | Bounded metadata and classic `stbl` parsing; no compressed-media decode and no fragmented-sample reconstruction |
| Classic sample indexing | Reference implemented | Exact DTS/PTS/duration/byte-range windows with explicit scan budgets and `mdat` containment |
| Recorded-media ingest | Integrated reference | Exact original publication, source-bound native inspection, root-last graph, and independent closure verification |
| Canonical media timeline | Integrated reference | Root-bound sample timeline with exact partial coverage, composition offsets, reordering, gaps, byte-span checks, deterministic digest, and JSON |
| Media worker protocol | Contract/reference implemented | Path-free decode plans, framehash-v2 parsing, typed terminations, indeterminacy, receipt validation; **no process spawn** |
| CLI | Integrated reference | Deterministic commands for manifests, custody, native media inspection, recorded-media ingest/verify, decode planning, and timeline projection |
| Capability/doctor surfaces | Reference implemented | Truthful maturity labels and read-only prerequisite probes |
| Agent operating model | Normative target | Schemas, registries, ADRs, and acceptance scenarios; no complete Agent Turn Packet runtime yet |
| Local qualification | Executable local lanes | Static checks, pinned Rust format/check/Clippy/test, recorded-media E2E, and timeline E2E; hosted Actions are non-authoritative |

## Current public reference commands

```bash
cargo run -p fdgr-cli -- capabilities --format json
cargo run -p fdgr-cli -- doctor --format json
cargo run -p fdgr-cli -- file-manifest <path> --format json
cargo run -p fdgr-cli -- import-file <store-root> <path> --format json
cargo run -p fdgr-cli -- media-inspect <path> --format json
cargo run -p fdgr-cli -- media-samples <path> --track-id <id> --format json
cargo run -p fdgr-cli -- stored-media-inspect <store-root> <manifest-digest> --format json
cargo run -p fdgr-cli -- stored-media-samples <store-root> <manifest-digest> --track-id <id> --format json
cargo run -p fdgr-cli -- recorded-media-ingest <store-root> <path> --format json
cargo run -p fdgr-cli -- recorded-media-verify <store-root> <root-manifest-digest> --format json
cargo run -p fdgr-cli -- recorded-media-timeline <store-root> <root-manifest-digest> --track-id <id> --format json
cargo run -p fdgr-cli -- media-decode-plan <store-root> <root-manifest-digest> [required plan options] --format json
cargo run -p fdgr-cli -- verify-file <path> [required identities] --format json
cargo run -p fdgr-cli -- verify-store <store-root> <manifest-digest> --format json
```

`recorded-media-timeline` first independently verifies the complete recorded-media publication
root, then reopens the authenticated source object through the store, derives one bounded classic
sample window, and binds the resulting timeline to the exact root/source identities. A partial
window states its prefix and suffix omissions; it cannot masquerade as whole-track coverage.

## Important non-claims

| Surface | Status |
|---|---|
| DJI Fly/controller live-view acquisition | Research only; no admitted live adapter |
| Owner-authorized aircraft control | Not implemented and outside the initial authority model |
| Asupersync-owned FFmpeg execution | Not implemented; current decode objects are plans/receipts only |
| Native compressed-video decoder/encoder | Not implemented |
| Packet arrival/display/telemetry clock fusion | Not implemented; media timeline is the immutable encoded-sample basis only |
| Calibration and rolling-shutter registry | Not implemented |
| Metric scale-witness fusion | Not implemented |
| Keyframes, features, tracks, pose graph, bundle adjustment | Not implemented in the current `main` workspace |
| Depth, fusion, occupancy, mesh, topology | Not implemented |
| Qwen/SAM or other model execution | Protocol/design only; no model lane admitted |
| Semantic resolver and evidence-linked scene graph | Not implemented |
| B2/R2 archive, multipart resume, readback, restore | Not implemented |
| FastMCP and complete eleven-operation Agent Turn Packet | Not implemented |
| Ground-truth accuracy, latency, cost, or ergonomic claims | No claim without retained measured receipts |
| Production/security/recovery qualification | No claim |

## Qualification interpretation

A passing local workspace or E2E run proves only the named reference semantics at the exact source,
toolchain, fixture, and policy identities. It does not promote a work package or gate unless all
registered positive, negative, fault, recovery, compatibility, and benchmark evidence for that gate
also exists. In particular:

- a process exit is not decoded-frame publication;
- a decode plan is not dispatch authority;
- an authenticated sample timeline is not a telemetry clock model;
- reaching the end of a requested window is not whole-track coverage unless the window starts at
  sample zero;
- source presence is not production admission;
- a hosted status badge is not release authority.
