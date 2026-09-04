# Bundle Problem Reference

`fdgr-bundle-problem` is the deterministic **structural compilation** boundary between an exact component-relative pose refinement and the stronger image/provenance audit required before any future landmark-bearing optimizer.

Its job is not to optimize and not to prove that a factor is physically usable. Its job is to establish one exact camera/landmark observation universe, compute a deterministic support core, and describe its bipartite topology without erasing rejected evidence.

## Authority ladder

```text
pose refinement
  → structural bundle-problem compilation
    → image-domain and seed-provenance audit
      → bundle optimization proposal          [future]
        → held-out reprojection adjudication  [future]
          → sparse reconstruction generation  [future]
```

The compiler may emit only:

```text
admitted_relative_bundle_problem
```

This is **structural authority only**. Before optimization, a consumer must also obtain an admitted `fdgr.bundle_admission/1` generation. See [`BUNDLE_ADMISSION_REFERENCE.md`](BUNDLE_ADMISSION_REFERENCE.md).

## Exact basis

Every structural generation binds:

```text
pose_refinement_digest
camera_binding_basis_digest
landmark_seed_basis_digest
observation_basis_digest
policy_digest
generation
```

The compiler independently recomputes every table identity before admission. Local paths, parsing work, pruning rounds, graph path expansions, and successful operation ceilings remain operation evidence rather than semantic identity.

## Camera and calibration identity binding

Every camera in the exact pose-refinement generation must have exactly one structural binding containing:

```text
camera node identity
source sample identity
exact frame-object digest
exact effective-calibration digest
```

The sample identity must equal the retained pose sample. Missing, duplicate, stale, or extra bindings fail closed. Observations repeat the sample and frame identities, and those identities must equal the camera binding.

The effective-calibration digest is opaque at this layer. The structural compiler does **not** bind image width or height, prove that an observation lies inside the calibrated pixel domain, parse a calibration object, or establish calibration accuracy. Those stronger checks belong to `fdgr-bundle-admission`.

## Landmark seed proposals

Each landmark seed carries:

```text
landmark identity
source track identity
component root
relative scale-gauge root
seed evidence digest
component-relative seed position
seed uncertainty
```

A landmark must live in the same pose component and arbitrary relative-scale gauge as every observing camera. Seeds remain proposals; structural compilation does not certify their coordinates.

The `fdgr.bundle_problem/1` format predates explicit observation-level seed provenance. It therefore does **not** prove which optimize observations initialized a seed or prove that held-out measurements were excluded from initialization. That proof is mandatory in the downstream bundle-admission audit.

## Optimize and held-out observations

Every observation identifies:

```text
observation identity
landmark identity
camera identity
source feature-observation identity
evidence digest
image coordinate
localization uncertainty
dynamic-mask state
role = optimize | held_out
```

At most one observation may name a camera/landmark pair, and one camera feature may support at most one landmark. Roles are immutable inputs.

Optimize observations may enter the structural factor graph. Held-out observations remain outside it. At this layer, a held-out observation is structurally eligible when it passes its quality gates and names an active landmark. Its camera may still be pruned from the optimize core, and the structural compiler cannot know whether its measurement influenced seed initialization. Consequently, structural held-out counts are **candidate held-out evidence**, not yet an independence certificate.

## Deterministic support-core pruning

Before graph analysis, the compiler repeatedly applies these gates to optimize observations until a fixed point:

1. reject dynamic-masked or over-uncertain optimize observations;
2. prune landmarks observed by fewer than the required independent cameras;
3. prune non-root cameras supporting fewer than the required active landmarks;
4. remove optimize observations whose camera or landmark was pruned;
5. repeat until no membership changes.

Component roots are never silently removed. A root without sufficient active landmark support is an explicit gauge-root evidence defect.

Every input observation ends with one stable disposition. Pruning cannot erase evidence or turn an observation into an implicit absence.

## Bipartite topology certificate

Active cameras and landmarks form one deterministic bipartite graph. Internal graph-node identities are canonical and separate from domain identities. Active optimize observations become graph edges. The graph oracle derives:

```text
connected components
maximum-priority forest
fundamental cycles
bridges
cycle rank
root reachability
```

The compiler maps every bridge back to the exact observation that created it. A connected but bridge-dependent problem is not described as redundant.

## Structural component decisions

Each pose component receives exactly one structural status and decision:

| Status | Meaning | Decision |
|---|---|---|
| `blocked_pose_refinement` | Upstream pose decision is blocked | `block` |
| `no_training_core` | No supported optimize core survives | `block` |
| `gauge_root_unobserved` | The fixed root lacks required landmark support | `block` |
| `disconnected_from_gauge_root` | Active cameras are not connected to the root through landmark evidence | `block` |
| `tree_like` | The active bipartite graph has no redundant cycle | `admit_diagnostic` |
| `fragile` | Cycles exist, but one or more optimize observations remain bridges | `admit_diagnostic` |
| `missing_held_out_evidence` | Training topology is redundant but candidate held-out evidence is insufficient | `admit_diagnostic` |
| `redundant` | Root-connected, cyclic, bridge-free, and candidate-held-out-supported | `admit` |

A structural `admit` does not permit optimization by itself. It means only that the topology is ready for the stronger image-domain, seed-provenance, and held-out-independence audit.

A nominal equation count may be reported as planning evidence, but it is never presented as a numerical-rank proof.

## Agent-facing recommendations

The structural status compiles directly into one minimum-cost next action:

```text
resolve upstream pose conflict
add multi-view landmark support
observe the gauge-root neighborhood
capture a bridging view
reinforce bridge observations
reserve candidate held-out views
proceed to bundle-admission audit
```

This is a decision certificate, not prose inferred independently by each agent.

## Mandatory downstream audit

`fdgr-bundle-admission` binds the exact image domain for every camera and the exact optimize-only observation provenance for every seed. It then:

- checks coordinates against half-open image bounds;
- rejects held-out observations used during seed initialization;
- requires sufficient seed support surviving in the optimize core;
- removes held-out evidence whose camera is absent from that core;
- recomputes the component decision without inheriting structural `admit` blindly.

No future bundle optimizer should consume `fdgr.bundle_problem/1` without an admitted audit generation over the exact same structural digest.

## Hard nonclaims

The structural compiler does not:

- estimate or refine camera rotations, centers, intrinsics, distortion, rolling shutter, or landmarks;
- bind calibrated image dimensions or prove coordinates are inside them;
- prove seed initialization used only optimize observations;
- prove held-out observations are statistically or procedurally independent;
- minimize reprojection error;
- compute a Hessian, covariance, Schur complement, or numerical rank;
- prove calibration accuracy merely by retaining a calibration digest;
- prove favorable parallax, conditioning, or physical accuracy from graph topology;
- admit metric scale;
- publish sparse geometry.

A future optimized implementation must prove equivalence to this scalar ordered-map/full-recompute oracle before replacing the reference path, and it must still consume the separate downstream audit certificate.
