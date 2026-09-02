# Implementation Status

**Snapshot:** 2026-09-02

FDGR is an evidence-first, dependency-free safe-Rust reference implementation under active construction. The current workspace establishes deterministic semantic boundaries and public diagnostic paths; it does not yet constitute a production DJI acquisition, reconstruction, semantic-twin, or cloud-recovery system.

## Maturity vocabulary

| Label | Meaning |
|---|---|
| **Source present** | Code or a schema exists; no execution claim follows. |
| **Reference implemented** | A deterministic scalar/reference algorithm and unit fixtures exist. |
| **Publicly invokable** | The reference is reachable through the unified `fdgr` CLI with exact input identities. |
| **Locally qualified** | The repository-owned pinned-toolchain and E2E lanes passed for an exact commit and host. |
| **Production admitted** | Registered positive, negative, fault, recovery, compatibility, accuracy, security, and performance evidence has satisfied the owning gate. |

No lower label implies a higher one.

## Current executable surface

The workspace currently contains **20 package members**. Every member inherits edition 2024, the pinned toolchain policy, `unsafe_code = forbid`, and the closed dependency universe.

| Surface | Current maturity | Earned boundary |
|---|---|---|
| Canonical identity and codec | Reference implemented | Streaming SHA-256, length-framed domain separation, deterministic bounded codecs, typed failures |
| Evidence ledger | Reference implemented | Append/replay invariants, optimistic anchors, immutable event identities; FrankenSQLite production adapter remains unadmitted |
| Local immutable object store | Reference implemented | Staged object publication, object-first/manifest-root-last visibility, collision refusal, readback verification |
| Native ISO BMFF inspection | Reference implemented | Bounded container and classic `stbl` parsing; no compressed-video decode or fragmented-sample reconstruction |
| Recorded-media custody | Publicly invokable | Exact original publication, source-bound inspection, root-last graph, independent closure verification |
| Canonical media timeline | Publicly invokable | Exact DTS/PTS/duration/byte spans, signed composition offsets, gaps, reordering, and explicit partial coverage |
| Clock evidence | Publicly invokable | Robust, correlation-aware affine fitting; exact epochs/support; no extrapolation or cross-epoch interpolation |
| Calibration | Reference implemented | Fixed-point intrinsics, Brown-Conrady distortion, shutter/readout, rigid extrinsics, exact crop/resize propagation and scope checks |
| Scale witnesses | Reference implemented | Correlation-group-aware candidate fitting, retained conflicts, relative/estimated/witnessed/surveyed authority, metric refusal below witnessed authority |
| Keyframe selection | Publicly invokable | Exact candidate basis, quality gates, marginal visibility, view/baseline diversity, deterministic selection and rejection ledger |
| Descriptor correspondence | Publicly invokable | Bounded 256-bit Hamming matching, ambiguity/ratio/mutual checks, operation budgets, collision-safe multi-view tracks |
| Relative-pose candidate adjudication | Publicly invokable | Fixed-point rotation/translation validation, epipolar residual, parallax, cheirality, explicit no-candidate/ambiguous/verified candidate-set status |
| Media worker protocol | Contract/reference implemented | Path-free decode plans, typed termination, framehash receipt validation and indeterminacy; **no process spawn** |
| Capability and doctor surfaces | Publicly invokable | Stable maturity labels and read-only prerequisite probes |
| Agent operating model | Normative target | Schemas, registries, ADRs and acceptance scenarios; no complete Agent Turn Packet runtime |
| Local qualification | Executable local lanes | Static contracts, Rust format/check/Clippy/tests, and public-path E2E descriptions; hosted GitHub Actions are non-authoritative |

## Two-view evidence ladder

The current geometry frontier is deliberately layered:

```text
decoded-frame evidence
  → keyframe candidate evidence
    → selected keyframe generation
      → descriptor observations
        → pairwise descriptor hypotheses
          → collision-safe tracks
            → calibrated bearing matches
              → exact relative-motion candidate set
                → epipolar/parallax/cheirality adjudication
```

These statements are intentionally not interchangeable:

```text
descriptor match
≠ geometrically supported match
≠ selected relative-motion candidate
≠ globally optimized camera pose
≠ metric camera pose
≠ published geometry
```

`fdgr-relative-pose` does not generate five-point/eight-point candidates. It evaluates an exact content-addressed candidate set and preserves ambiguity instead of inventing a winner. Its translation is a direction only; it carries no metric baseline.

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
cargo run -p fdgr-cli -- media-decode-plan <store-root> <root-manifest-digest> [required options] --format json
cargo run -p fdgr-cli -- clock-fit <anchors.tsv> [exact basis and fit options] --format json
cargo run -p fdgr-cli -- keyframe-select <candidates.tsv> [exact basis and policy options] --format json
cargo run -p fdgr-cli -- correspondence-build <features.tsv> <pairs.tsv> [exact basis and policy options] --format json
cargo run -p fdgr-cli -- relative-pose-verify <bearings.tsv> <candidates.tsv> [exact basis and policy options] --format json
cargo run -p fdgr-cli -- verify-file <path> [required identities] --format json
cargo run -p fdgr-cli -- verify-store <store-root> <manifest-digest> --format json
```

The subsystem-oriented commands are diagnostic/reference adapters. They do not expand the target eleven-operation Agent Narrow Waist; future agent execution compiles through `fdgr.propose`, `fdgr.commit`, and `fdgr.watch`.

## Important non-claims

| Surface | Current status |
|---|---|
| DJI Fly/controller live-view acquisition | Research only; no admitted live adapter |
| Aircraft control | Not implemented and outside the initial authority model |
| Asupersync-owned FFmpeg execution | Not implemented; current decode objects are plans and receipt validators |
| Immutable decoded-frame generation | Not implemented |
| Native compressed-video decoder/encoder | Not implemented |
| Live arrival/display/telemetry clock fusion | Not implemented |
| Calibration estimation and measured real-device accuracy | Not implemented; representation/derivation semantics only |
| Automatic scale-witness acquisition | Not implemented; witness resolution semantics only |
| Native feature detector/descriptor extraction | Not implemented; correspondence consumes exact supplied feature evidence |
| Five-point/eight-point candidate generation | Not implemented |
| Pose graph, loop closure, bundle adjustment and global trajectory | Not implemented |
| Metric pose | Not implemented; relative-pose translation remains unit direction |
| Sparse triangulated map and dense depth/fusion | Not implemented |
| Occupancy, mesh, topology and appearance generations | Not implemented |
| Qwen/SAM or other model execution | Protocol/design only; no model lane admitted |
| Semantic resolver and evidence-linked scene graph | Not implemented |
| B2/R2 multipart replication, readback and restore | Not implemented |
| FastMCP and complete Agent Turn Packet runtime | Not implemented |
| Ground-truth accuracy, latency, cost or ergonomic claims | No claim without retained measured receipts |
| Production security/recovery qualification | No claim |

## Qualification interpretation

A passing unit or E2E lane proves only its named reference semantics at the exact source, toolchain, host, fixture and policy identities. It does not close a work package or acceptance gate by itself.

In particular:

- source presence is not native qualification;
- process exit is not decoded-frame publication;
- a decode plan is not dispatch authority;
- a media timeline is not a synchronized telemetry trajectory;
- a calibration object is not proof of calibration accuracy;
- an estimated scale is not metric authority;
- a keyframe visibility cell is not a coverage certificate;
- a descriptor track is not an epipolar inlier;
- a candidate-set winner is not a global pose graph;
- a hosted status badge is not release authority.

The authoring environment for the most recent correspondence/relative-pose wave did not expose `cargo`, `rustc`, or `rustfmt`. Native format/check/Clippy/test/E2E success is therefore not claimed for that wave until a Doodlestein or direct local qualification receipt names the exact commit.
