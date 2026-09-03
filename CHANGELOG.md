# Changelog

All notable implementation changes are recorded here. Target-state design remains in the comprehensive plan; this file records executable evolution, semantic corrections, and changes to earned capability boundaries.

## Unreleased

### Added

#### Evidence custody and media

- Dependency-free streaming SHA-256, domain-separated identities, deterministic codecs, and typed decode errors.
- Append-only evidence-ledger reference semantics with optimistic anchors and deterministic replay.
- A local immutable object store with staged writes, object-first/manifest-root-last publication, collision refusal, readback, and closure verification.
- Bounded native ISO BMFF inspection and classic sample-table expansion with exact DTS, PTS, duration, synchronization state, sample-description identity, and encoded byte ranges.
- Root-last recorded-media ingest and independent reconstruction of the published source/inspection graph.
- `fdgr.media_timeline/1`, including signed composition offsets, decode gaps, presentation and byte-order reordering, and explicit whole-track versus partial-window coverage.
- A path-free media-worker plan/receipt protocol with typed termination and indeterminate-effect semantics. Process spawning remains unimplemented.

#### Sensor evidence

- `fdgr.clock_model/1`: correlation-aware robust affine clock fitting, exact epochs, support intervals, drift/residual evidence, outlier retention, and refusal to extrapolate.
- `fdgr.calibration_model/1`: fixed-point pinhole intrinsics, Brown-Conrady distortion, global/directional rolling shutter, rigid camera/body extrinsics, exact applicability scope, and crop/resize derivation.
- `fdgr.scale_model/1`: independent correlation-group votes, retained conflicts, exact inlier reclassification, conservative uncertainty, and explicit relative/estimated/witnessed/surveyed authority.

#### Two-view geometry

- `fdgr-keyframe`, a deterministic quality, visibility, view-sector, and baseline-diversity selector with exact input bases and explicit rejection ledgers.
- `fdgr-correspondence`, a bounded 256-bit Hamming matcher with nearest-tie, second-best, ratio, mutual, response, uncertainty, and dynamic-mask gates.
- Collision-safe deterministic feature-track union that refuses any component containing two observations from one frame.
- `fdgr.correspondence_generation/1`, retaining accepted descriptor hypotheses, rejection evidence, tracks, unmatched observations, and operation-cost evidence.
- `fdgr-epipolar`, an exact essential-matrix proposal adjudicator over authenticated calibrated correspondence evidence.
- `fdgr-relative-pose`, a fixed-point candidate-set verifier using rotation/translation validation, normalized epipolar residuals, parallax, cheirality, inlier ratios, and explicit ambiguity.
- `fdgr.relative_pose_verification/1`, with a self-contained selected transform only when one candidate is uniquely admitted.

#### Multi-view pose frontier

- `fdgr-graph`, deriving deterministic connected components, forests, bridges, non-forest edges, and fundamental-cycle witnesses without geometric authority.
- `fdgr-pose-graph`, composing `R_node_from_component_root` orientations and retaining rotation-cycle consistency or conflict while leaving translation magnitudes underdetermined.
- `fdgr-edge-scale`, reconciling correlation-aware relative baseline ratios inside explicit arbitrary edge-scale components, with independent-group evidence and cycle conflict preservation.
- `fdgr-global-pose`, initializing deterministic camera centers in one zero-origin arbitrary gauge per admitted pose component.
- `fdgr.global_pose_initialization/1`, including exact upstream generation identities, parent-edge provenance, translation-cycle evidence, component status, operation evidence, and explicit `relative_component_gauge` authority.
- A shared pose/scale CLI construction seam so edge-scale reconciliation and global-pose initialization cannot silently parse different evidence universes.
- [`architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md`](architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md), defining the transform convention, authority ladder, agent interpretation rules, and unearned `WP-018` boundary.

#### Public paths and qualification campaigns

- Exact-byte public commands for clock fit, keyframe selection, correspondence construction, epipolar verification, relative-pose verification, pose-graph construction, relative edge-scale resolution, and component-relative global-pose initialization.
- Public-path E2E lanes for timeline, clock fitting, keyframes, correspondences, epipolar verification, relative pose, pose graph, edge scale, and global pose.
- Global-pose scenarios covering deterministic replay, cycle-consistent initialization, preserved translation conflict, disconnected component-local gauges, successful budget-ceiling identity invariance, stale exact-byte refusal, and operation-budget refusal.
- Truthful capability and schema registry entries for each current reference seam.

### Changed

- The workspace now contains 25 package members under the strict safe-Rust, edition-2024, closed-dependency policy.
- The executable reference chain now separates descriptor evidence, epipolar adjudication, pairwise physical motion, graph topology, orientation composition, relative edge-scale reconciliation, and component-relative camera-center initialization.
- The implementation sequence places component-relative pose initialization before nonlinear bundle refinement, depth, fusion, coverage, and semantics.
- Public global-pose output names the unit `component_edge_scale_unit_nano` and authority `relative_component_gauge`; neither can be interpreted as meters or a published trajectory.
- Local qualification invokes the new pose-graph, edge-scale, and global-pose campaigns. Hosted GitHub Actions remain non-authoritative.
- Status documentation distinguishes source presence, reference implementation, public invocation, local qualification, and production admission.

### Fixed

- Repaired the `fdgr-cli` entry in `Cargo.lock` after adding its `fdgr-global-pose` dependency, restoring consistency for every `cargo ... --locked` lane.
- Partial sample windows can no longer masquerade as complete-track evidence.
- Presentation-domain `i128` timestamps are rendered losslessly rather than as IEEE-754-limited JSON numbers.
- Scale authority can no longer be elevated by a rejected high-grade witness correlated with an admitted lower-grade witness; an internally conflicting group is rejected as a whole.
- Keyframe rejection marginal coverage is evaluated against the final selected basis rather than mixed intermediate bases.
- Correspondence union refuses same-frame collisions instead of silently creating impossible tracks.
- Descriptor ambiguity, epipolar support, candidate selection, graph topology, orientation, edge-scale gauge, initialized camera center, bundle refinement, and metric pose are represented as separate authority states.
- Disconnected components retain independent origins and scale gauges rather than receiving a manufactured transform.
- Public implementation documentation no longer describes graph topology, pose orientation, edge-scale reconciliation, and component-relative initialization as absent.

### Qualification

- The repository-owned full lane specifies formatting, workspace check, Clippy with warnings denied, unit tests, static contract checks, and deterministic public-path E2E campaigns.
- No hosted status badge, queued self-hosted run, source presence, unit fixture, or partial E2E receipt may promote a work package or release root.
- The current global-pose source has not yet earned a retained exact-commit local qualification receipt. `WP-018` also remains open for robust nonlinear refinement, checkpoints, cancellation/recovery, decision cards, differential equivalence, and accuracy evidence.
