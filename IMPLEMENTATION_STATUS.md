# Implementation Status

**Snapshot:** 2026-09-04  
**Current implementation class:** deterministic safe-Rust reference source under active construction

FDGR currently provides a coherent exact-evidence path from immutable recorded-media custody through an **audited relative bundle problem**. It is not yet a production DJI acquisition, joint bundle-adjustment, sparse/dense reconstruction, semantic-twin, archive-recovery, or agent-control system.

## Maturity vocabulary

| Label | Meaning |
|---|---|
| **Source present** | Code, a schema, or a design exists. No execution claim follows. |
| **Reference implemented** | A deterministic scalar/reference algorithm and focused fixtures exist. |
| **Publicly invokable** | The reference is reachable through `fdgr` with authenticated bounded inputs. |
| **Locally qualified** | The repository-owned pinned-toolchain and named E2E lanes passed for an exact commit and host, with retained evidence. |
| **Production admitted** | The owning gate’s positive, negative, fault, recovery, compatibility, accuracy, security, and performance evidence is complete. |

No lower label implies a higher one. A queued or hosted workflow is not local qualification.

## Workspace boundary

The workspace currently contains **28 package members**. Every member inherits edition 2024, the pinned toolchain policy, `unsafe_code = forbid`, and the closed dependency universe.

The current executable reference chain is:

```text
exact bytes and canonical identities
  → immutable local custody and recorded-media roots
    → bounded native media/sample evidence and canonical timelines
      → clock, calibration, and scale-witness semantics
        → deterministic keyframe evidence
          → descriptor correspondences and collision-safe tracks
            → calibrated epipolar proposal adjudication
              → physical two-view relative-pose adjudication
                → deterministic graph topology and component orientations
                  → correlation-aware relative edge-baseline gauges
                    → component-relative camera centers
                      → robust translation-only center refinement
                        → structural bundle-problem compilation
                          → image-domain, seed-provenance, and held-out audit
```

The final arrow admits an exact relative bundle problem for bounded optimization evaluation. It does not optimize cameras or landmarks, prove calibration accuracy or numerical conditioning, emit meters, or publish geometry.

## Current executable surfaces

| Surface | Current maturity | Earned boundary |
|---|---|---|
| Canonical identity and codec | Reference implemented | Streaming SHA-256, domain separation, bounded deterministic codecs, typed failures |
| Evidence ledger | Reference implemented | Append/replay invariants, optimistic anchors, immutable event identities; no production database adapter |
| Local immutable object store | Reference implemented | Staged writes, object-first/manifest-root-last visibility, collision refusal, readback verification |
| Native ISO BMFF inspection | Reference implemented | Bounded container and classic sample-table parsing; no compressed-video decode |
| Recorded-media custody | Publicly invokable | Exact original publication, source-bound inspection, root-last graph, independent closure verification |
| Canonical media timeline | Publicly invokable | DTS/PTS/duration/byte spans, signed composition offsets, gaps, reordering, explicit partial coverage |
| Clock evidence | Publicly invokable | Robust correlation-aware affine fitting, exact epochs/support, no extrapolation |
| Calibration | Reference implemented | Fixed-point intrinsics, distortion, shutter/readout, rigid extrinsics, crop/resize propagation and scope checks |
| Metric scale-witness semantics | Reference implemented | Correlation-aware candidate fitting, conflict retention, relative/estimated/witnessed/surveyed authority and metric refusal |
| Keyframe selection | Publicly invokable | Exact basis, quality gates, marginal visibility, view/baseline diversity, deterministic rejection ledger |
| Descriptor correspondence | Publicly invokable | Bounded Hamming matching, tie/ratio/mutual gates, operation budgets, collision-safe tracks |
| Epipolar proposal verification | Publicly invokable | Exact calibrated correspondence basis, essential-matrix proposal residuals and degeneracy evidence; no motion authority |
| Relative-pose candidate adjudication | Publicly invokable | Rotation/direction validation, epipolar/parallax/cheirality evidence, explicit no-winner/ambiguity/unique winner |
| Graph topology | Reference implemented through downstream public paths | Deterministic components, forest, bridges, non-forest edges, and fundamental-cycle witnesses; no geometry authority |
| Pose-graph orientation | Publicly invokable | Component-local orientations and rotation-cycle status; translation magnitudes remain underdetermined |
| Relative edge-scale reconciliation | Publicly invokable | Correlation-aware baseline ratios and explicit arbitrary scale components; disconnected gauges remain incomparable |
| Component-relative global pose | Publicly invokable | Deterministic orientations and camera centers, zero-origin component gauges, parent-edge provenance, translation-cycle status |
| Translation-only pose refinement | Publicly invokable | Robust deterministic camera-center relaxation against fixed rotations/scales with retained factor evidence; no landmark or metric authority |
| Structural bundle problem | Publicly invokable | Exact camera/frame/calibration identities, landmark proposals, optimize/held-out roles, fixed-point support pruning, bipartite topology, bridges, structural decisions |
| Bundle-admission audit | Publicly invokable | Exact image domains, coordinate bounds, optimize-only seed provenance, surviving seed support, active-camera held-out independence, recomputed decisions |
| Media worker protocol | Contract/reference implemented | Path-free decode plans and typed receipt validation; **no process spawn** |
| Capability and doctor surfaces | Publicly invokable | Stable maturity labels and read-only prerequisite probes |
| Agent operating model | Normative target | Schemas, registries, ADRs, and acceptance scenarios; no complete Agent Turn Packet runtime |
| Local qualification | Executable specification | Static, format, check, Clippy, tests, and public-path E2Es through bundle admission; no retained current-head success receipt in this execution environment |

## Geometry authority ladder

The current frontier must be read as distinct claims:

```text
graph topology
≠ component-local orientation
≠ relative edge-baseline gauge
≠ component-relative camera centers
≠ translation-only refined centers
≠ structural bundle topology
≠ audited optimizer input
≠ bundle-adjusted trajectory and landmarks
≠ metric camera pose
≠ published geometry
```

`fdgr-pose-graph` composes `R_node_from_component_root` and assesses rotation cycles. It emits no camera centers.

`fdgr-edge-scale` reconciles ratios among pairwise baseline magnitudes. Its scale components are arbitrary gauges, not metric transforms.

`fdgr-global-pose` combines those exact generations to initialize camera centers in `component_edge_scale_unit_nano`. Each connected component has its own zero origin and scale root. Translation-cycle conflict remains visible.

`fdgr-pose-refinement` changes camera centers only, against fixed rotations and fixed relative edge scales. It retains both initialization and factor evidence. It is not full pose optimization.

`fdgr-bundle-problem` compiles a structural support core and bipartite graph. Its effective-calibration digests are opaque, its image coordinates are not yet checked against bound dimensions, and its held-out counts are candidate evidence rather than a proof of independence.

`fdgr-bundle-admission` binds exact per-camera image domains and exact optimize-only seed provenance, rejects held-out seed leakage, requires support that survived into the optimize core, removes held-out evidence from pruned cameras, and recomputes component admission. Its positive authority is only `audited_relative_bundle_problem`.

See:

- [`architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md`](architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md)
- [`architecture/BUNDLE_PROBLEM_REFERENCE.md`](architecture/BUNDLE_PROBLEM_REFERENCE.md)
- [`architecture/BUNDLE_ADMISSION_REFERENCE.md`](architecture/BUNDLE_ADMISSION_REFERENCE.md)

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

These subsystem commands are diagnostic/reference adapters. They do not expand or bypass the target eleven-operation agent narrow waist.

## Important non-claims

| Surface | Current status |
|---|---|
| DJI live-view or telemetry acquisition | Research only; no admitted adapter |
| Aircraft control | Not implemented and outside the initial authority model |
| Asupersync-owned FFmpeg execution | Not implemented; current worker objects are plans and validators |
| Immutable decoded-frame generation | Not implemented |
| Native compressed-video decoder/encoder | Not implemented |
| Live arrival/display/telemetry clock fusion | Not implemented |
| Calibration estimation and measured device accuracy | Not implemented; representation/derivation semantics only |
| Automatic metric scale-witness acquisition | Not implemented |
| Native feature/descriptor extraction | Not implemented; exact supplied feature evidence is consumed |
| Five-point/eight-point candidate generation | Not implemented |
| Loop-candidate search and closure admission | Not implemented as a complete public lifecycle |
| Rotation-aware or joint pose/landmark bundle adjustment | Not implemented |
| Reprojection residual and held-out improvement adjudication | Not implemented |
| Numerical rank, Hessian, covariance, and Schur-complement evidence | Not implemented |
| Resumable refinement checkpoints and decision cards | Not implemented |
| Metric camera pose | Not implemented; current camera centers use arbitrary component gauges |
| Sparse triangulated map and dense depth/fusion | Not implemented |
| Occupancy, surfel, TSDF, mesh, topology, or appearance publication | Not implemented |
| Qwen/SAM or other model execution | Protocol/design only; no model lane admitted |
| Semantic resolver and evidence-linked scene graph | Not implemented |
| B2/R2 replication, readback, repair, and restore | Not implemented |
| Complete Agent Turn Packet, Decision Frame, or FastMCP runtime | Not implemented |
| Ground-truth accuracy, latency, cost, or ergonomic claims | No claim without retained measured receipts |
| Production security/recovery qualification | No claim |

## `WP-018` progress boundary

The following reference substrate now exists:

- deterministic graph topology;
- component-local orientation propagation and rotation-cycle evidence;
- correlation-aware relative edge-scale reconciliation;
- component-relative camera-center initialization;
- translation-only robust camera-center refinement;
- deterministic structural camera/landmark support-core compilation;
- bipartite graph cycles, bridges, root reachability, and structural decisions;
- exact camera image-domain audit;
- optimize-only seed-provenance verification;
- held-out-camera independence correction;
- canonical schemas and capability discovery;
- exact-byte public CLI paths;
- focused unit fixtures and public-path E2E specifications.

`WP-018` remains open. Its unimplemented or unqualified scope includes reprojection models, joint rotation/translation/landmark optimization, numerical rank and conditioning, robust factor admission/retraction generations, checkpoints, cancellation/crash/recovery, held-out improvement adjudication, agent projection, optimized/reference equivalence, and measured accuracy evidence.

## Qualification interpretation

A passing unit or E2E lane proves only its named semantics at exact source, toolchain, host, fixture, and policy identities. It does not close a work package or acceptance gate by itself.

In particular:

- source presence is not qualification;
- process exit is not decoded-frame publication;
- a media timeline is not synchronized telemetry;
- a calibration object is not calibration accuracy;
- an estimated scale is not metric authority;
- a keyframe visibility cell is not a coverage certificate;
- a descriptor track is not an epipolar inlier;
- an essential-matrix proposal is not physical motion authority;
- a relative-pose winner is not a pose graph;
- a pose graph is not a camera trajectory;
- an initialized or translation-refined camera center is not bundle adjustment or metric pose;
- a structurally redundant factor graph is not proof that its image coordinates or seed partition are valid;
- an audited bundle problem is not optimized geometry or held-out accuracy evidence;
- a queued or hosted workflow is not a local Doodlestein receipt.

The current exact head has not yet produced a retained repository-owned full qualification receipt in this execution environment. The self-hosted workflow specification may remain queued when no qualified runner is available; that state is visible and does not become success by timeout or assumption.
