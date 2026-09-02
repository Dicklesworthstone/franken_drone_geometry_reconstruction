# franken_drone_geometry_reconstruction (`fdgr`)

[![License: MIT+Rider](https://img.shields.io/badge/License-MIT%2BOpenAI%2FAnthropic%20Rider-blue.svg)](./LICENSE)

**An agent-native, evidence-grade operating substrate for turning owner-authorized drone observations into metrically honest semantic digital twins.**

> **Current implementation boundary — 2026-09-02:** FDGR has a 20-package, dependency-free safe-Rust reference workspace covering immutable media custody, native ISO BMFF inspection, canonical sample timelines, clock evidence, camera-calibration representation, scale-witness resolution, deterministic keyframe selection, descriptor correspondence, collision-safe tracks, and fixed-point relative-pose candidate adjudication. It does **not** yet claim live DJI acquisition, owned FFmpeg execution, native feature detection, candidate generation, a global pose graph, bundle adjustment, metric reconstruction, dense fusion, semantic asset resolution, cloud recovery, or production qualification.

FDGR is designed for a person who manually flies a compact drone such as the DJI Flip around a home while an agent helps decide what to observe, what to compute, what can be trusted, what remains unknown, and what should happen next. The eventual system preserves original media, reconstructs geometry, resolves visible property assets, measures only what has earned metric authority, identifies evidence gaps, guides additional capture, archives every consequential artifact, and explains each claim.

The product is not “a mesh from a video.” It is a **multi-version evidence and constraint database for physical reality** operated through one coherent cognitive control loop.

## The system from the driver’s seat

A competent agent should never have to mentally join capture status, clocks, calibration, feature tracks, pose hypotheses, model outputs, storage state, and background work. After every success, progress event, or error, FDGR is designed to answer:

1. What is established by evidence?
2. What changed since the anchor I understood?
3. Which uncertainty matters to the active objective?
4. What work is active, blocked, draining, or indeterminate?
5. Which actions are available under current authority and policy?
6. Which next step has the highest expected value per unit of total control cost?
7. What would prove that step complete?
8. What should a fresh agent receive at handoff?

The target operating loop is:

```text
bootstrap → orient → focus → inspect → formulate → propose → compare
          → commit → watch → verify/reconcile → learn → handoff/resume
```

Every eventual agent operation projects through the same Agent Turn Packet, exact anchor, four ledgers, Decision Frame, and typed recovery vocabulary. The subsystem commands described below are current diagnostic/reference adapters, not additions to the target eleven-operation semantic waist.

See [`docs/AGENT_OPERATING_MODEL.md`](docs/AGENT_OPERATING_MODEL.md), [`architecture/AGENT_ABSTRACTION_TOWER.md`](architecture/AGENT_ABSTRACTION_TOWER.md), and [`architecture/AGENT_NARROW_WAIST.md`](architecture/AGENT_NARROW_WAIST.md).

## One evidence universe

```text
L9  Campaign, mission, policy
L8  Objective graph
L7  Questions, uncertainty, coverage, evidence deficits
L6  Candidate plans and counterfactual branches
L5  Obligations, effects, progress, reconciliation, surprise
L4  Claims, assets, measurements, topology, coverage certificates
L3  Constraints: clocks, calibration, scale, tracks, poses, depth
L2  Observation capsules and immutable generations
L1  Content-addressed objects, custody, transfer, repair, restore
L0  Fenced effects: DJI, files, ffmpeg, models, GPU, cloud, operator
```

Higher levels compress and organize lower-level evidence; they never silently strengthen it. An agent can traverse from a mission down to raw bytes, or from a surprising observation up to affected objectives, through exact typed handles.

## Three planes, one cognitive center

```text
┌──────────────────────────────────────────────────────────────────────┐
│ AGENT SEMANTIC WAIST                                                │
│ sessions · turns · objectives · questions · candidates · handoffs   │
└──────────────────────────────────────────────────────────────────────┘
                    │ intents                       │ evidence
                    ▼                               ▲
┌──────────────────────────────────────────────────────────────────────┐
│ AUTHORITATIVE EVIDENCE PLANE                                        │
│ capsules · anchors · witnesses · obligations · roots · receipts     │
└──────────────────────────────────────────────────────────────────────┘
        │ pinned generations                    │ fenced tickets
        ▼                                       ▼
┌────────────────────────────────┐  ┌─────────────────────────────────┐
│ RECONSTRUCTION / COGNITION     │  │ DEVICE / EFFECT                 │
│ tracks · poses · depth · graph │  │ DJI · files · workers · cloud  │
│ geometry · semantics · search  │  │ upload · lookup · reconciliation│
└────────────────────────────────┘  └─────────────────────────────────┘
```

Cognition may propose but cannot dispatch. Effects may produce observations and receipts but cannot define identity, scale, geometry, semantics, or completion.

## Current executable reference chain

The implemented workspace currently follows this dependency direction:

```text
exact bytes
  → canonical identity and immutable local custody
    → bounded native container/sample evidence
      → recorded-media root and canonical timeline
        → clock support and calibration scope
          → scale-witness authority
            → keyframe evidence
              → descriptor hypotheses and tracks
                → relative-motion candidate adjudication
```

### Media custody and time

FDGR currently provides reference implementations for:

- streaming SHA-256 and domain-separated object identities;
- canonical bounded codecs;
- append-only evidence events and deterministic replay;
- object-first, manifest-root-last local publication;
- bounded ISO BMFF metadata and classic sample-table inspection;
- exact DTS, PTS, duration, sync state, sample-description, and byte-range expansion;
- exact original-media publication and independent root reconstruction;
- canonical partial or whole-track timelines with signed composition offsets, gaps, and reordering;
- path-free media-worker plans and receipt validation;
- robust, correlation-aware affine clock fitting with explicit epochs and no extrapolation.

The worker protocol does not spawn FFmpeg. A plan, process exit, or receipt parse is not decoded-frame publication.

### Calibration and scale

The calibration reference represents:

- fixed-point pinhole intrinsics;
- Brown-Conrady or explicit no-distortion state;
- global or directional rolling shutter;
- camera-from-body rigid extrinsics;
- exact device/lens/temperature applicability scope;
- reprojection and declared uncertainty evidence;
- exact crop/resize propagation into a derived image domain.

The scale reference treats metric scale as a proof obligation. Correlated witnesses receive one robust vote, internally conflicting groups remain rejected, and metric mapping is refused until witnessed or surveyed authority exists.

A calibration object is not proof that the camera was accurately calibrated. An estimated scale is not permission to emit meters.

## Current two-view geometry frontier

```text
keyframe candidates
  → selected keyframe generation
    → feature observations
      → descriptor correspondence hypotheses
        → collision-safe tracks
          → calibrated bearing matches
            → exact relative-motion candidate set
              → epipolar, parallax, and cheirality evidence
```

### Deterministic keyframes

`fdgr-keyframe` uses exact input identities and fixed-point evidence for sharpness, texture, clipping, dynamic content, overlap, visibility cells, view sectors, and baseline bins. Selection maximizes marginal evidence and diversity under a hard capacity and records every rejection against the final selected basis.

Candidate visibility cells are proposals, not surface-coverage certificates.

### Descriptor correspondence and tracks

`fdgr-correspondence` performs bounded 256-bit Hamming matching with explicit:

- nearest-neighbor ties;
- second-best availability;
- distance and ratio gates;
- optional mutual-nearest support;
- response, uncertainty, and dynamic-mask eligibility;
- operation-budget consumption;
- unmatched observations;
- collision-safe multi-view union.

An accepted descriptor edge is not an epipolar inlier. Track union refuses any component containing two observations from the same frame.

### Relative-pose candidate adjudication

`fdgr-relative-pose` validates fixed-point unit bearings, rotations, and translation directions, then evaluates an exact supplied candidate set using:

- epipolar-plane conditioning;
- normalized epipolar residuals;
- parallax;
- two-view cheirality;
- inlier and positive-depth ratios;
- explicit candidate ambiguity.

Its result is one of:

```text
no_accepted_candidate
ambiguous
geometrically_verified
```

A unique result includes the selected transform directly. Translation remains a direction only. The crate does not generate five-point/eight-point candidates and does not establish global or metric pose.

See [`architecture/TWO_VIEW_EVIDENCE_REFERENCE.md`](architecture/TWO_VIEW_EVIDENCE_REFERENCE.md).

## Public reference commands

```bash
cargo run -p fdgr-cli -- capabilities --format json
cargo run -p fdgr-cli -- doctor --format json
cargo run -p fdgr-cli -- file-manifest <path> --format json
cargo run -p fdgr-cli -- import-file <store-root> <path> --format json
cargo run -p fdgr-cli -- media-inspect <path> --format json
cargo run -p fdgr-cli -- media-samples <path> --track-id <id> --format json
cargo run -p fdgr-cli -- stored-media-inspect <store-root> <manifest> --format json
cargo run -p fdgr-cli -- stored-media-samples <store-root> <manifest> --track-id <id> --format json
cargo run -p fdgr-cli -- recorded-media-ingest <store-root> <path> --format json
cargo run -p fdgr-cli -- recorded-media-verify <store-root> <root> --format json
cargo run -p fdgr-cli -- recorded-media-timeline <store-root> <root> --track-id <id> --format json
cargo run -p fdgr-cli -- media-decode-plan <store-root> <root> [exact options] --format json
cargo run -p fdgr-cli -- clock-fit <anchors.tsv> [exact options] --format json
cargo run -p fdgr-cli -- keyframe-select <candidates.tsv> [exact options] --format json
cargo run -p fdgr-cli -- correspondence-build <features.tsv> <pairs.tsv> [exact options] --format json
cargo run -p fdgr-cli -- relative-pose-verify <bearings.tsv> <candidates.tsv> [exact options] --format json
cargo run -p fdgr-cli -- verify-file <path> [exact identities] --format json
cargo run -p fdgr-cli -- verify-store <store-root> <manifest> --format json
```

Successful machine output omits ambient local paths and names evidence by content identity. Input tables are bounded, versioned, authenticated before parsing, and rejected after mutation under stale digests.

## What FDGR does not yet claim

The current repository does not claim:

- live DJI Flip video or telemetry acquisition;
- aircraft control;
- Asupersync-owned worker process execution and descendant cleanup;
- immutable decoded-frame publication;
- native feature detection or descriptor extraction;
- minimal relative-pose candidate generation;
- a view graph, loop closure, pose graph, global trajectory, or bundle adjustment;
- metric camera poses;
- sparse or dense reconstruction;
- TSDF, surfel, occupancy, mesh, topology, or appearance publication;
- multimodal-model execution or semantic asset resolution;
- Cloudflare R2 or Backblaze B2 replication and restore;
- a complete Agent Turn Packet or FastMCP runtime;
- production accuracy, latency, cost, security, privacy, or recovery qualification.

Read [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) before interpreting target-state prose as current functionality.

## DJI integration without architectural contamination

The DJI Flip is a motivating profile, not FDGR’s data model. Sources are admitted through a ladder:

1. exact original-media import from microSD or an explicit export;
2. controller/phone recording and bounded display/HDMI/USB capture;
3. documented SDK, UVC, RTMP, RTSP, or vendor export for exact supported profiles;
4. owner-authorized, read-only-first protocol research against the operator’s own paired system.

Profiles include aircraft, controller, firmware, app, phone OS, region, radio mode, pairing/account state, observed endpoints, and capability transcripts. The project does not bypass pairing, authentication, encryption, account controls, geofencing, or another operator’s equipment.

See [`DJI_ADAPTER_RESEARCH.md`](DJI_ADAPTER_RESEARCH.md).

## Safe-Rust and dependency doctrine

The production trust domain is strict Rust on the pinned nightly toolchain, with:

- `#![forbid(unsafe_code)]` in every FDGR crate;
- Asupersync as the sole admitted async runtime at orchestration seams;
- exact admitted Franken-suite revisions;
- only narrowly reviewed fundamental exceptions such as Serde;
- no Tokio, Rayon, C/C++ FFI, in-process Python, linked FFmpeg, OpenCV, COLMAP, Ceres, generic database/graph/search engine, or unpinned Git dependency in the core.

Media, model, GPU, vendor, and research stacks remain supervised external processes with sealed manifests, bounded resources, no-network defaults where possible, descendant ownership, output quarantine, and reconciliation.

See [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md).

## Local qualification is release authority

GitHub-hosted workflow results have no release authority. The repository-owned qualifier and Doodlestein job graph specify:

```bash
./scripts/qualify.sh --mode static
./scripts/qualify.sh --mode full
./scripts/qualify.sh --mode release --sibling-root /exact/checkouts
```

The newest correspondence and relative-pose wave was authored in an environment without `cargo`, `rustc`, or `rustfmt`, so native success for those commits is not claimed until a retained local receipt names the exact commit.

See [`QUALIFICATION.md`](QUALIFICATION.md) and [`LOCAL_QUALIFICATION_AND_RELEASE.md`](LOCAL_QUALIFICATION_AND_RELEASE.md).

## Documentation map

Start with:

1. [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md)
2. [`DESIGN_INDEX.md`](DESIGN_INDEX.md)
3. [`docs/AGENT_OPERATING_MODEL.md`](docs/AGENT_OPERATING_MODEL.md)
4. [`architecture/SEMANTICS_MANIFEST.md`](architecture/SEMANTICS_MANIFEST.md)
5. [`architecture/SENSOR_EVIDENCE_REFERENCE.md`](architecture/SENSOR_EVIDENCE_REFERENCE.md)
6. [`architecture/TWO_VIEW_EVIDENCE_REFERENCE.md`](architecture/TWO_VIEW_EVIDENCE_REFERENCE.md)
7. [`ARCHITECTURE.md`](ARCHITECTURE.md)
8. [`COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md`](COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md)
9. [`FRANKENSTACK_DEEP_DIVE.md`](FRANKENSTACK_DEEP_DIVE.md)
10. [`CHANGELOG.md`](CHANGELOG.md)
