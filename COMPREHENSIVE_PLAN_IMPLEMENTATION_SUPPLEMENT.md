# Comprehensive Plan Implementation Supplement

**Document class:** normative implementation delta to the comprehensive plan  
**Snapshot:** 2026-09-04  
**Applies to:** the current safe-Rust reference workspace and all future bundle-optimization work

This supplement records implementation-earned architecture that postdates the comprehensive plan’s embedded revision-0.4 registry appendix. The original plan remains normative. Where this supplement narrows an implementation authority boundary more strictly than older target prose, the stricter boundary governs current and future code until an explicit schema epoch or ADR replaces it.

## Current executable frontier

The current 28-member workspace reaches:

```text
exact media evidence
  → canonical time/calibration/scale semantics
    → keyframes and descriptor tracks
      → epipolar and physical relative-pose adjudication
        → graph topology and component orientation
          → relative edge-baseline gauges
            → component-relative camera-center initialization
              → translation-only robust center refinement
                → structural bundle-problem compilation
                  → image-domain, seed-provenance, and held-out audit
```

No current generation grants joint pose-landmark optimization, numerical-rank, held-out reprojection improvement, sparse reconstruction, metric pose, or physical-world accuracy authority.

## Two-stage bundle preparation is mandatory

The bundle frontier is split into two immutable generations because the required evidence cannot be added to the existing structural format without changing its durable meaning.

### Structural generation

`fdgr.bundle_problem/1` establishes:

- exact pose-refinement basis;
- exact camera/sample/frame/effective-calibration identities;
- exact landmark seed proposals and component-relative gauges;
- immutable optimize versus candidate-held-out roles;
- deterministic fixed-point support pruning;
- camera/landmark bipartite topology;
- forest, cycle, bridge, and root-reachability evidence;
- planning-only equation counts;
- typed structural block, diagnostic, or admit decisions.

Its positive authority is `admitted_relative_bundle_problem`, meaning only that the problem is structurally ready for the stronger audit.

The structural generation does not contain exact image dimensions or observation-level seed initialization provenance. Its held-out counts are therefore candidate independence evidence only.

### Admission-audit generation

`fdgr.bundle_admission/1` consumes the exact structural digest and adds:

- one exact image domain per camera;
- frame and effective-calibration identity equality;
- half-open top-left nano-pixel coordinate validation;
- one exact optimize-only seed-provenance record per landmark;
- hard refusal of held-out observations used during seed initialization;
- minimum surviving seed-support observations and cameras;
- seed-uncertainty policy;
- exclusion of held-out evidence from cameras pruned out of the optimize core;
- recomputed component decisions and typed next actions.

Its positive authority is `audited_relative_bundle_problem`, meaning only that the exact problem may enter bounded optimization evaluation.

A future optimizer MUST consume an admitted audit generation. It MUST NOT consume `fdgr.bundle_problem/1` directly, reconstruct a parallel audit from ambient files, or infer missing evidence from plausible geometry.

## Why this is a separate schema epoch

Changing `fdgr.bundle_problem/1` to add image dimensions and seed-support observation IDs would change the semantic meaning of existing content-addressed generations. That would violate the project’s durable identity doctrine.

The correct accretive pattern is:

```text
old immutable structural evidence
  + new exact evidence tables
    → new independently replayable audit generation
```

Existing structural digests remain reproducible. New consumers require the stronger downstream certificate.

## Held-out evidence doctrine

An observation is independently held out only when all of the following are true:

1. its immutable role is `held_out`;
2. its landmark remains active in the optimize core;
3. its camera remains active in that core when policy requires it;
4. its coordinate lies within the exact bound image domain;
5. it was not used to initialize the landmark seed;
6. it does not enter the optimization objective;
7. later adjudication evaluates it against a proposal produced without that measurement.

Withholding a residual while allowing the same measurement to initialize the landmark is self-certification, not validation.

## Calibration identity versus accuracy

An effective-calibration digest establishes which image model a camera binding refers to. It does not establish that the model is accurate.

A future reprojection layer must separately consume the exact calibration object and validate:

- image derivation and pixel convention;
- intrinsics and distortion model;
- shutter/readout semantics;
- applicability scope;
- calibration residual and uncertainty evidence;
- any interpolation or rolling-shutter correction basis.

The bundle-admission audit prevents silent calibration substitution but grants no calibration-accuracy authority.

## Next implementation DAG

The next dependency-critical work is:

```text
bundle-admission audit
  → deterministic calibrated reprojection oracle
    → scalar joint pose-landmark proposal solver
      → factor admission/downweight/retraction generation
        → exact resumable checkpoint and cancellation semantics
          → independent held-out reprojection adjudication
            → immutable relative sparse reconstruction
              → independently witnessed metric mapping
```

### Reprojection oracle

The first next crate should define deterministic projection, distortion, visibility, cheirality, image-domain, uncertainty, and residual-family semantics without optimizing anything.

### Reference bundle solver

The initial solver should be scalar, ordered-map, full-recompute, fixed-budget, and deterministic. It should preserve the admitted audit generation as an immutable basis, fix component gauges explicitly, retain all proposal iterations, and never publish stronger authority than a proposal.

### Independent adjudication

Optimization residual on training factors cannot certify the proposal. Promotion requires exact held-out observations that were excluded from both seed initialization and optimization, plus explicit comparison against the retained prior.

### Publication

A sparse reconstruction generation may be published only after proposal adjudication. Component-relative coordinates remain nonmetric until an independently admitted scale generation maps the exact reconstruction basis into meters.

## Qualification consequences

The full local/Doodlestein predecessor chain MUST include:

```text
pose refinement
  → structural bundle compilation
    → bundle-admission audit
      → future reprojection and optimization lanes
```

No hosted status, source presence, isolated unit test, or skipped predecessor may substitute for the audit receipt.

The current execution environment used to author this supplement does not expose the pinned Rust toolchain. Therefore the new source and public-path campaign are not described as locally qualified until a retained exact-head Doodlestein or direct local receipt exists.

## Agent interpretation

An agent should interpret the current bundle statuses as follows:

- structural `block`: repair upstream pose/support/topology evidence;
- structural `admit_diagnostic`: gather topology or candidate held-out support;
- structural `admit`: run the exact bundle-admission audit;
- audit `block`: repair image-domain or upstream structural evidence;
- audit `admit_diagnostic`: repair seed provenance, independent held-out evidence, or upstream diagnostic topology;
- audit `admit`: proceed only to a bounded optimizer proposal;
- no current status permits metric or geometry publication.

This hierarchy is designed to make the least-cost safe next action mechanically legible to an operating agent while preserving every earlier evidence generation.
