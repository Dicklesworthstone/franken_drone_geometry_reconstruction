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
- `fdgr-pose-refinement`, performing deterministic robust translation-only camera-center relaxation while fixing rotations, relative edge scales, component roots, and all upstream evidence.
- `fdgr.pose_refinement/1`, retaining initial/refined centers, factor weights and residuals, component decisions, do-nothing dominance, iteration count, and operation evidence without claiming full pose optimization.
- Shared exact CLI construction seams for pose/scale/global/refinement generations so downstream operations cannot silently parse different evidence universes.

#### Bundle preparation and admission

- `fdgr-bundle-problem`, a deterministic structural compiler over exact camera/frame/calibration identities, landmark proposals, optimize/held-out roles, and fixed-point camera/landmark support pruning.
- A bipartite structural topology certificate retaining graph components, maximum-priority forest edges, non-forest edges, fundamental cycles, observation-level bridges, root reachability, planning-only equation counts, and typed component decisions.
- `fdgr.bundle_problem/1` with explicit `admitted_relative_bundle_problem` structural authority and no optimization, image-domain, seed-provenance, numerical-rank, metric, or geometry-publication authority.
- `fdgr-bundle-admission`, a separate mandatory optimizer-entry audit over the exact structural problem digest.
- Exact per-camera image-domain records binding frame identity, effective-calibration identity, width, and height.
- Half-open top-left nano-pixel coordinate checks for every active or candidate-held-out observation.
- Exact per-landmark seed-provenance records naming the optimize observations used during initialization.
- Hard refusal when a held-out observation is used to initialize a landmark seed.
- Surviving seed-support checks across the final optimize core, including minimum observation count, camera count, and uncertainty gates.
- Held-out independence correction that excludes candidate evidence from cameras pruned out of the final optimize core.
- `fdgr.bundle_admission/1`, retaining observation audits, landmark audits, component decisions, typed next actions, and operation-cost evidence with `audited_relative_bundle_problem` authority.
- Agent-facing statuses for blocked upstream structure, invalid image domains, unproven seed partitions, insufficient independent held-out evidence, upstream diagnostic problems, and fully admitted problems.
- [`architecture/BUNDLE_PROBLEM_REFERENCE.md`](architecture/BUNDLE_PROBLEM_REFERENCE.md), corrected to describe structural authority only.
- [`architecture/BUNDLE_ADMISSION_REFERENCE.md`](architecture/BUNDLE_ADMISSION_REFERENCE.md), defining the non-bypassable image-domain, seed-provenance, and held-out audit.

#### Public paths and qualification campaigns

- Exact-byte public commands for clock fit, keyframe selection, correspondence construction, epipolar verification, relative-pose verification, pose-graph construction, relative edge-scale resolution, component-relative global-pose initialization, translation refinement, structural bundle compilation, and bundle-admission auditing.
- Public-path E2E lanes through the complete current geometry chain.
- Bundle-problem scenarios covering deterministic replay, structural admission, missing held-out diagnostics, unobserved-root blocking, canonical row order, successful execution-ceiling identity invariance, stale exact-byte refusal, and operation-budget refusal.
- Bundle-admission scenarios covering admitted evidence, image-domain blocking, held-out-camera demotion, held-out seed-leak refusal, canonical row order, successful execution-ceiling identity invariance, stale exact-byte refusal, and operation-budget refusal.
- Doodlestein promotion dependencies through global pose, singleton continuity, translation refinement, structural bundle compilation, and bundle admission.
- Truthful capability and schema registry entries for each current reference seam.

### Changed

- The workspace now contains **28 package members** under the strict safe-Rust, edition-2024, closed-dependency policy.
- The executable reference chain now separates descriptor evidence, epipolar adjudication, pairwise physical motion, graph topology, orientation composition, relative edge-scale reconciliation, component-relative camera-center initialization, translation-only refinement, structural bundle topology, and audited optimizer input.
- Structural `admit` no longer means optimizer-ready. A future optimizer must consume an admitted `fdgr.bundle_admission/1` generation over the exact structural digest.
- Structural held-out counts are now described as candidate independence evidence rather than a proof of independence.
- The implementation sequence places bundle-problem audit before any reprojection optimization, sparse reconstruction, metric mapping, fusion, coverage, or semantics.
- Public relative-pose outputs continue to name arbitrary component gauges rather than meters or a published trajectory.
- Local qualification and Doodlestein now require the bundle-admission campaign before promotion. Hosted GitHub Actions remain non-authoritative.
- Status documentation distinguishes source presence, reference implementation, public invocation, local qualification, and production admission.

### Fixed

- Closed an authority leak in which an opaque effective-calibration digest could be mistaken for a bound image domain.
- Closed an out-of-frame factor path by requiring exact per-camera dimensions and half-open coordinate checks before optimizer admission.
- Closed a held-out self-certification path by requiring exact optimize-only landmark-seed provenance.
- Closed a second held-out independence gap in which observations from a camera pruned out of the optimize core could still satisfy candidate held-out counts.
- Corrected `architecture/BUNDLE_PROBLEM_REFERENCE.md`, which had described image-domain and seed-provenance evidence absent from the immutable `fdgr.bundle_problem/1` format.
- Introduced a versioned downstream audit rather than silently changing the semantics of existing structural bundle-problem digests.
- Refactored the structural CLI through one shared exact reconstruction/parser seam so bundle compilation and admission cannot drift into different input grammars.
- Restored deterministic registry-traceability labels to the exact values generated from capability authority entries.
- Repaired the Doodlestein geometry dependency graph so promotion cannot skip global-pose initialization, singleton continuity, pose refinement, structural bundle compilation, or bundle admission.
- Repaired the `fdgr-cli` lockfile dependency closure after adding bundle-problem and bundle-admission adapters.
- Admitted zero-based media sample indices in structural camera and observation bindings.
- Aligned pose-refinement schema factor cardinalities with the pose-graph edge ceiling.
- Partial sample windows can no longer masquerade as complete-track evidence.
- Presentation-domain `i128` timestamps are rendered losslessly rather than as IEEE-754-limited JSON numbers.
- Scale authority can no longer be elevated by a rejected high-grade witness correlated with an admitted lower-grade witness; an internally conflicting group is rejected as a whole.
- Keyframe rejection marginal coverage is evaluated against the final selected basis rather than mixed intermediate bases.
- Correspondence union refuses same-frame collisions instead of silently creating impossible tracks.
- Descriptor ambiguity, epipolar support, candidate selection, graph topology, orientation, edge-scale gauge, initialized camera center, translation refinement, structural bundle readiness, audited optimizer readiness, bundle adjustment, and metric pose are represented as separate authority states.
- Disconnected components retain independent origins and scale gauges rather than receiving a manufactured transform.

### Qualification

- The repository-owned full lane specifies generated-contract checks, dependency and registry controls, formatting, locked workspace check, Clippy with warnings denied, unit tests, and deterministic public-path E2E campaigns through bundle admission.
- No hosted status badge, queued self-hosted run, source presence, unit fixture, or partial E2E receipt may promote a work package or release root.
- The current source tip has not yet earned a retained exact-commit full local qualification receipt in this execution environment.
- `WP-018` remains open for reprojection models, joint rotation/translation/landmark optimization, numerical conditioning, robust factor admission/retraction, checkpoints, cancellation/recovery, held-out improvement adjudication, differential equivalence, and measured accuracy evidence.
