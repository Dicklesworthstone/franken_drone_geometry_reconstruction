# Bundle Admission Reference

`fdgr-bundle-admission` is the deterministic audit boundary between a structurally compiled bundle problem and any future reprojection optimizer.

The existing `fdgr.bundle_problem/1` generation proves graph support, gauge reachability, bridge structure, and held-out counts. It intentionally retains opaque effective-calibration identities and proposal landmark seeds. Before numerical optimization, FDGR needs one additional generation that proves the image domain and training provenance those opaque identities refer to.

## Authority ladder

```text
pose refinement
  → structural bundle compilation
    → bundle admission audit
      → bundle optimization proposal          [future]
        → held-out reprojection adjudication  [future]
          → sparse reconstruction publication [future]
```

The audit may emit only:

```text
audited_relative_bundle_problem
```

It emits no optimized camera, landmark, reprojection, covariance, numerical-rank, metric, or sparse-geometry authority.

## Exact basis

Every audit binds:

```text
exact structural bundle-problem digest
exact camera image-domain table digest
exact landmark seed-provenance table digest
exact audit-policy digest
generation
```

The complete structural bundle problem is retained for replay. Execution ceilings and observed operation count remain cost evidence and do not alter successful semantic identity.

## Camera image-domain binding

Every camera in the structural problem must have exactly one domain record containing:

```text
camera node identity
exact frame-object digest
exact effective-calibration digest
calibrated image width
calibrated image height
```

The frame and calibration identities must equal the structural camera binding. Missing, duplicate, stale, or extra domains fail closed.

Every observation is checked against the exact top-left half-open image domain:

```text
0 ≤ x < width
0 ≤ y < height
```

Coordinates use nano-pixels. An active optimize or otherwise eligible held-out observation outside its domain blocks numerical use of the component. An already excluded observation remains visible but cannot contaminate the admitted factor set.

Binding a calibration digest does not prove calibration accuracy. It proves only that the optimizer cannot silently switch to a different crop, resize, lens state, stabilization state, or pixel domain.

## Landmark seed provenance

Every landmark seed must have exactly one provenance record listing the canonical observation identities that contributed to initialization.

Each listed observation must:

- exist in the exact structural problem;
- observe the same landmark;
- have immutable role `optimize`;
- be unique inside the provenance record.

A held-out observation may never initialize a landmark. Such input fails closed rather than being downgraded to a warning.

For an active landmark to be seed-proven, enough listed support observations must survive the final optimize core and span the configured minimum number of cameras. Seed uncertainty must also remain within policy. A component with an otherwise usable graph but unproven active seeds is diagnostic only and recommends rebuilding seeds from the optimize partition.

## Held-out independence

A held-out observation is independently usable only when:

- its landmark remains active;
- its camera remains active in the optimize core;
- it lies inside the bound image domain;
- it was not used to initialize the landmark.

This closes two subtle self-certification paths:

1. counting a view as held out after pruning that camera from the training core;
2. withholding a reprojection factor whose measurement already influenced the seed.

The audit recomputes held-out observation and camera counts after these stronger checks. It never inherits an upstream `admit` decision blindly.

## Component decisions

Each pose component receives exactly one audit status:

| Status | Meaning | Decision |
|---|---|---|
| `blocked_upstream` | Structural compilation already blocked the component | `block` |
| `invalid_image_domain` | An active or eligible observation lies outside its exact image domain | `block` |
| `unproven_seed_partition` | One or more active seeds lack sufficient surviving optimize-only provenance | `admit_diagnostic` |
| `insufficient_independent_held_out` | Stronger held-out checks no longer meet the structural policy | `admit_diagnostic` |
| `upstream_diagnostic` | The structural generation was intentionally diagnostic | `admit_diagnostic` |
| `admitted` | Structural admission, image domains, seed provenance, and held-out independence all pass | `admit` |

The associated recommendation is a stable machine value, not prose reconstructed independently by every agent.

## Hard nonclaims

The audit does not:

- parse or estimate a camera calibration model;
- prove calibration accuracy;
- triangulate or refine landmarks;
- minimize reprojection error;
- estimate camera poses, intrinsics, distortion, or rolling shutter;
- compute a Hessian, Schur complement, covariance, or numerical rank;
- admit metric scale;
- publish sparse geometry.

A future optimizer must consume an admitted audit generation, retain it as an immutable basis, preserve every rejected or diagnostic factor, and prove held-out improvement before any stronger reconstruction authority can be published.
