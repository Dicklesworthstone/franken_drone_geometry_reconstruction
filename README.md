# franken_drone_geometry_reconstruction (`fdgr`)

[![License: MIT+Rider](https://img.shields.io/badge/License-MIT%2BOpenAI%2FAnthropic%20Rider-blue.svg)](./LICENSE)

**An agent-native, evidence-grade operating substrate for turning owner-authorized drone observations into metrically honest semantic digital twins.**

> **Current implementation boundary — 2026-09-04:** FDGR has a 28-member, dependency-closed safe-Rust reference workspace. The executable path covers immutable recorded-media custody, bounded native ISO BMFF/sample evidence, canonical timelines, clock/calibration/scale semantics, deterministic keyframes and correspondences, epipolar and relative-pose adjudication, graph topology, component-local orientation, relative edge-baseline gauges, component-relative camera-center initialization, translation-only robust pose refinement, structural bundle-problem compilation, and a separate image-domain/seed-provenance/held-out-independence audit. It does **not** yet claim live DJI acquisition, owned FFmpeg execution, native feature or motion-candidate generation, joint pose-landmark bundle adjustment, metric camera pose, sparse/dense reconstruction, semantic resolution, cloud recovery, a complete agent runtime, or production qualification.

FDGR is designed for a person who manually flies a compact drone such as the DJI Flip around a property while an agent helps decide what to observe, what to compute, what can be trusted, what remains unknown, and what should happen next.

The product is not “a mesh from a video.” It is a **multi-version evidence and constraint database for physical reality**, operated through one coherent cognitive control loop.

## The system from the driver’s seat

After every success, progress event, refusal, or error, a competent agent should be able to answer:

1. What is established at the exact current anchor?
2. What changed since the anchor I understood?
3. Which uncertainty matters to the active objective?
4. What work is active, blocked, draining, or indeterminate?
5. Which actions are expressible under current evidence and authority?
6. Which next step has the highest expected value per total control cost?
7. What evidence would prove that step complete?
8. What compact handoff lets another agent resume safely?

The target loop is:

```text
bootstrap → orient → focus → inspect → formulate → propose → compare
          → commit → watch → verify/reconcile → learn → handoff/resume
```

Every eventual agent operation projects through the same exact anchor, Agent Turn Packet, four synchronized ledgers, Decision Frame, and typed recovery vocabulary. The current subsystem commands are reference/diagnostic adapters, not a competing control plane.

## One evidence universe

```text
L9  Campaign, mission, policy
L8  Objective graph
L7  Questions, uncertainty, coverage, evidence deficits
L6  Candidate plans and counterfactual branches
L5  Obligations, effects, progress, reconciliation, surprise
L4  Claims, assets, measurements, topology, coverage certificates
L3  Constraints: clocks, calibration, scale, tracks, poses, depth, factors
L2  Observation capsules and immutable generations
L1  Content-addressed objects, custody, transfer, repair, restore
L0  Fenced effects: DJI, files, workers, GPU, cloud, operator
```

Higher levels compress lower-level evidence. They never silently strengthen it. Cognition may propose but cannot dispatch; effects may produce observations but cannot define identity, scale, geometry, semantics, or completion.

## Current executable reference chain

```text
exact bytes and canonical identities
  → immutable local custody and recorded-media roots
    → bounded native container/sample evidence
      → canonical media timelines
        → clock, calibration, and metric-scale witness semantics
          → deterministic keyframe evidence
            → descriptor correspondences and collision-safe tracks
              → calibrated epipolar proposal adjudication
                → physical two-view relative-pose adjudication
                  → deterministic graph topology
                    → component-local camera orientations
                      → correlation-aware relative edge-baseline gauges
                        → component-relative camera centers
                          → robust translation-only camera-center refinement
                            → structural camera/landmark factor graph
                              → image-domain, seed-provenance, and held-out audit
```

Each seam has an exact basis, a narrow authority ceiling, deterministic replay, and typed failure states. The final audit admits a **problem for bounded optimization evaluation**; it is not itself bundle adjustment or geometry publication.

### Media custody, time, calibration, and scale

The reference workspace provides:

- streaming SHA-256 and domain-separated identities;
- bounded canonical codecs and typed failures;
- append-only evidence events and deterministic replay;
- object-first, manifest-root-last local publication;
- bounded ISO BMFF metadata and classic sample-table inspection;
- exact DTS, PTS, duration, sync state, sample-description, and encoded byte ranges;
- exact original-media publication and independent root reconstruction;
- canonical partial or whole-track timelines with signed composition offsets, gaps, and reordering;
- path-free media-worker plans and receipt validation;
- robust affine clock fitting with exact epochs and no extrapolation;
- fixed-point camera intrinsics, distortion, shutter/readout, body extrinsics, and crop/resize derivation;
- correlation-aware scale-witness resolution with explicit relative, estimated, witnessed, and surveyed authority.

A decode plan is not worker execution. A calibration object is not proof of calibration accuracy. Estimated scale is not permission to emit meters.

### Two-view evidence

```text
keyframe candidates
  → selected keyframe generation
    → feature observations
      → descriptor hypotheses
        → collision-safe tracks
          → calibrated bearing matches
            → exact essential/motion candidate sets
              → epipolar, parallax, and cheirality evidence
```

`fdgr-correspondence` performs bounded deterministic 256-bit Hamming matching with tie, second-best, ratio, mutual, response, uncertainty, dynamic-mask, and operation-budget gates. Track union refuses a component containing two observations from one frame.

`fdgr-epipolar` adjudicates exact supplied essential-matrix proposals. It grants no rotation or translation authority.

`fdgr-relative-pose` validates exact supplied rotations and translation directions, then evaluates epipolar residuals, parallax, and cheirality. Its result is `no_accepted_candidate`, `ambiguous`, or `geometrically_verified`. It does not generate five-point/eight-point candidates, and translation remains a direction without metric baseline.

### Multi-view pose authority

The words *graph*, *pose*, *scale*, *global*, and *refined* are intentionally not interchangeable:

```text
graph topology
≠ component-local orientation
≠ relative edge-baseline gauge
≠ component-relative camera centers
≠ translation-only refined centers
≠ bundle-adjusted trajectory
≠ metric pose
≠ published geometry
```

- `fdgr-graph` derives deterministic connected components, forests, bridges, non-forest edges, and cycle witnesses. It has no geometric authority.
- `fdgr-pose-graph` composes `R_node_from_component_root` and assesses rotation cycles. It emits no camera centers.
- `fdgr-edge-scale` reconciles ratios among pairwise baseline magnitudes. Disconnected or unsupported scale gauges remain incomparable.
- `fdgr-global-pose` initializes camera centers in one zero-origin arbitrary gauge per pose component, retaining parent-edge provenance and translation-cycle consistency/conflict.
- `fdgr-pose-refinement` relaxes camera centers against fixed admitted rotations and edge-scale factors. It does not alter orientation, optimize landmarks, or create metric authority.

The coordinate unit remains `component_edge_scale_unit_nano`. It is neither meters nor a cross-component world frame.

See [`architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md`](architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md).

### Bundle preparation and admission

Bundle preparation is deliberately split into two generations:

```text
fdgr.bundle_problem/1
  = structural support-core and bipartite-topology evidence

fdgr.bundle_admission/1
  = exact image-domain, optimize-only seed-provenance,
    and independently usable held-out evidence
```

`fdgr-bundle-problem` authenticates camera/frame/calibration identities, landmark proposals, optimize/held-out roles, and a deterministic fixed-point support core. It maps bridge factors back to observations and emits block, diagnostic, or structural-admit decisions. Its held-out counts are only **candidate** independence evidence because the structural format does not bind image dimensions or observation-level seed provenance.

`fdgr-bundle-admission` is the mandatory optimizer-entry audit. It:

- binds one exact frame, effective-calibration identity, width, and height per camera;
- rejects active or eligible observations outside their half-open top-left image domain;
- binds the exact optimize observations used to initialize each landmark seed;
- rejects any held-out observation used for seed initialization;
- requires seed support to survive in the final optimize core across enough cameras;
- excludes held-out observations whose camera was pruned from that core;
- recomputes component decisions instead of inheriting structural `admit` blindly.

A positive audit grants only `audited_relative_bundle_problem`. It does not prove calibration accuracy, favorable numerical conditioning, held-out reprojection improvement, optimized landmarks, bundle-adjusted poses, or metric geometry.

See [`architecture/BUNDLE_PROBLEM_REFERENCE.md`](architecture/BUNDLE_PROBLEM_REFERENCE.md) and [`architecture/BUNDLE_ADMISSION_REFERENCE.md`](architecture/BUNDLE_ADMISSION_REFERENCE.md).

## Public reference commands

```bash
cargo run -p fdgr-cli -- capabilities --format json
cargo run -p fdgr-cli -- doctor --format json
cargo run -p fdgr-cli -- file-manifest <path> --format json
cargo run -p fdgr-cli -- import-file <store-root> <path> --format json
cargo run -p fdgr-cli -- media-inspect <path> --format json
cargo run -p fdgr-cli -- media-samples <path> --track-id <id> --format json
cargo run -p fdgr-cli -- recorded-media-ingest <store-root> <path> --format json
cargo run -p fdgr-cli -- recorded-media-verify <store-root> <root> --format json
cargo run -p fdgr-cli -- recorded-media-timeline <store-root> <root> --track-id <id> --format json
cargo run -p fdgr-cli -- media-decode-plan <store-root> <root> [exact options] --format json
cargo run -p fdgr-cli -- clock-fit <anchors.tsv> [exact options] --format json
cargo run -p fdgr-cli -- keyframe-select <candidates.tsv> [exact options] --format json
cargo run -p fdgr-cli -- correspondence-build <features.tsv> <pairs.tsv> [exact options] --format json
cargo run -p fdgr-cli -- epipolar-verify <observations.tsv> <candidates.tsv> [exact options] --format json
cargo run -p fdgr-cli -- relative-pose-verify <bearings.tsv> <candidates.tsv> [exact options] --format json
cargo run -p fdgr-cli -- pose-graph-build <nodes.tsv> <pose-edges.tsv> [exact options] --format json
cargo run -p fdgr-cli -- edge-scale-resolve <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> [exact options] --format json
cargo run -p fdgr-cli -- global-pose-initialize <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> [exact options] --format json
cargo run -p fdgr-cli -- pose-refine <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> [exact options] --format json
cargo run -p fdgr-cli -- bundle-problem-build <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> <camera-bindings.tsv> <landmark-seeds.tsv> <bundle-observations.tsv> [exact options] --format json
cargo run -p fdgr-cli -- bundle-admission-audit <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> <camera-bindings.tsv> <landmark-seeds.tsv> <bundle-observations.tsv> <camera-domains.tsv> <seed-provenance.tsv> [exact options] --format json
cargo run -p fdgr-cli -- verify-file <path> [exact identities] --format json
cargo run -p fdgr-cli -- verify-store <store-root> <manifest> --format json
```

Successful machine output omits ambient input paths. Bounded tables are authenticated before parsing and rejected after any byte mutation under stale supplied identities.

## What FDGR does not yet claim

The current repository does not claim:

- live DJI Flip video or telemetry acquisition;
- aircraft control;
- Asupersync-owned worker execution and descendant cleanup;
- immutable decoded-frame publication or native compressed-video decode;
- native feature/descriptor extraction;
- minimal relative-motion candidate generation;
- complete loop-candidate search and closure lifecycle;
- joint camera/landmark reprojection optimization or nonlinear bundle adjustment;
- numerical-rank, Hessian, covariance, or Schur-complement authority;
- resumable refinement checkpoints, crash recovery, or decision cards;
- metric camera poses or transforms between disconnected components;
- sparse triangulation, dense depth, fusion, occupancy, TSDF, surfels, mesh, topology, or appearance publication;
- multimodal-model execution or semantic asset resolution;
- Cloudflare R2 or Backblaze B2 replication and restore;
- a complete Agent Turn Packet, Decision Frame, or FastMCP runtime;
- production accuracy, latency, cost, security, privacy, recovery, or agent-ergonomics qualification.

Read [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) before interpreting target-state prose as current functionality.

## DJI integration without architectural contamination

The DJI Flip is a motivating profile, not FDGR’s data model. Source admission proceeds from exact original-media import and recorded/mirrored paths toward documented or owner-authorized read-only live paths. Profiles bind aircraft, controller, firmware, application, OS, region, radio mode, account/pairing state, observed endpoints, clocks, crop, codec, and limitations.

The project does not bypass pairing, authentication, encryption, account controls, geofencing, or another operator’s equipment. Initial pilot guidance is human-mediated and grants no flight-control authority.

## Safe-Rust and dependency doctrine

The production trust domain requires:

- Rust edition 2024 on the pinned nightly;
- `#![forbid(unsafe_code)]` in every FDGR crate;
- Asupersync as the sole admitted in-process async runtime when orchestration lands;
- exact admitted sibling revisions and narrowly reviewed fundamental dependencies;
- no Tokio, Rayon, C/C++ FFI, in-process Python, linked FFmpeg, OpenCV, COLMAP, Ceres, generic database/graph/search engine, or unpinned Git dependency in the core.

Media, model, GPU, vendor, and research stacks remain bounded external sidecars until an owned safe-Rust implementation earns its registered maturity gate.

## Local qualification is release authority

```bash
./scripts/qualify.sh --mode static
./scripts/qualify.sh --mode full
./scripts/qualify.sh --mode release --sibling-root /exact/checkouts
```

The full lane runs generated-contract checks, dependency and registry controls, formatting, locked workspace check, Clippy with warnings denied, all tests, and deterministic public-path E2Es through bundle admission. Doodlestein makes the bundle-admission receipt a predecessor of promotion. A queued self-hosted run, hosted badge, source file, unit test, or isolated E2E is not a retained local qualification receipt.

The current exact head has not yet earned a retained full local receipt in this execution environment. `WP-018` remains open for reprojection factors, joint pose-landmark optimization, numerical conditioning evidence, checkpoints, cancellation/recovery, differential equivalence, and measured held-out improvement.

## Documentation map

Start with:

1. [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md)
2. [`DESIGN_INDEX.md`](DESIGN_INDEX.md)
3. [`architecture/BUNDLE_PROBLEM_REFERENCE.md`](architecture/BUNDLE_PROBLEM_REFERENCE.md)
4. [`architecture/BUNDLE_ADMISSION_REFERENCE.md`](architecture/BUNDLE_ADMISSION_REFERENCE.md)
5. [`architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md`](architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md)
6. [`docs/AGENT_OPERATING_MODEL.md`](docs/AGENT_OPERATING_MODEL.md)
7. [`architecture/SEMANTICS_MANIFEST.md`](architecture/SEMANTICS_MANIFEST.md)
8. [`architecture/SENSOR_EVIDENCE_REFERENCE.md`](architecture/SENSOR_EVIDENCE_REFERENCE.md)
9. [`architecture/TWO_VIEW_EVIDENCE_REFERENCE.md`](architecture/TWO_VIEW_EVIDENCE_REFERENCE.md)
10. [`ARCHITECTURE.md`](ARCHITECTURE.md)
11. [`COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md`](COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md)
12. [`CHANGELOG.md`](CHANGELOG.md)
