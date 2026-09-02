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

#### Two-view geometry frontier

- `fdgr-keyframe`, a deterministic quality, visibility, view-sector, and baseline-diversity selector with exact input bases and explicit rejection ledgers.
- `fdgr-correspondence`, a bounded 256-bit Hamming matcher with nearest-tie, second-best, ratio, mutual, response, uncertainty, and dynamic-mask gates.
- Collision-safe deterministic feature-track union that refuses any component containing two observations from one frame.
- `fdgr.correspondence_generation/1`, retaining accepted descriptor hypotheses, rejection evidence, tracks, unmatched observations, and operation-cost evidence.
- `fdgr-relative-pose`, a fixed-point candidate-set verifier using rotation/translation validation, normalized epipolar residuals, parallax, cheirality, inlier ratios, and explicit ambiguity.
- `fdgr.relative_pose_verification/1`, with a self-contained selected transform only when one candidate is uniquely admitted.
- Exact-byte public commands:
  - `clock-fit`
  - `keyframe-select`
  - `correspondence-build`
  - `relative-pose-verify`
- Public-path local E2E lanes for timeline, clock fitting, keyframe selection, correspondence construction, and relative-pose adjudication.
- Truthful capability-discovery entries for calibration, scale, keyframes, correspondences, and relative-pose verification.

### Changed

- The workspace now contains 20 package members under the strict safe-Rust, edition-2024, closed-dependency policy.
- The implementation sequence now places exact calibration, scale evidence, keyframe selection, descriptor tracks, and two-view adjudication before pose-graph admission.
- Relative-pose JSON returns the selected candidate’s source, evidence identity, rotation, and translation direction directly, avoiding an unnecessary candidate-table reopen.
- Local Doodlestein and repository qualification descriptions include the new public-path campaigns. Hosted GitHub Actions remain non-authoritative.
- `IMPLEMENTATION_STATUS.md` now distinguishes source presence, reference implementation, public invocation, local qualification, and production admission.

### Fixed

- Partial sample windows can no longer masquerade as complete-track evidence.
- Presentation-domain `i128` timestamps are rendered losslessly rather than as IEEE-754-limited JSON numbers.
- Scale authority can no longer be elevated by a rejected high-grade witness correlated with an admitted lower-grade witness; an internally conflicting group is rejected as a whole.
- Keyframe rejection marginal coverage is evaluated against the final selected basis rather than mixed intermediate bases.
- Correspondence union refuses same-frame collisions instead of silently creating impossible tracks.
- Descriptor ambiguity, epipolar support, candidate selection, global pose, and metric pose are now represented and documented as separate authority states.
- Capability discovery no longer hides executable calibration, scale, keyframe, correspondence, and relative-pose reference surfaces.
- Public implementation documentation no longer describes the repository as a three- or fourteen-crate scaffold or claims that calibration, scale, tracks, and two-view verification are absent.

### Qualification

- The repository-owned full lane specifies formatting, workspace check, Clippy with warnings denied, unit tests, static contract checks, and deterministic public-path E2E campaigns.
- No hosted status badge or partial E2E receipt may promote a release root.
- The current authoring environment lacks `cargo`, `rustc`, and `rustfmt`; native success for the newest correspondence/relative-pose commits is not claimed until a local Doodlestein or direct qualification receipt names the exact source commit.
