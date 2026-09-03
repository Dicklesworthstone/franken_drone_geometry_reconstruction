# Bundle Problem Reference

`fdgr-bundle-problem` is the deterministic structural admission boundary between a refined component-relative camera state and any future landmark-bearing bundle optimizer.

Its job is not to optimize. Its job is to prove that the exact camera, calibration, landmark, and observation evidence supplied to an optimizer forms a coherent, gauge-explicit, sufficiently supported problem, while retaining every pruning and held-out decision.

## Authority ladder

```text
pose refinement
  → bundle problem compilation
    → bundle optimization proposal          [future]
      → held-out reprojection adjudication  [future]
        → sparse reconstruction generation  [future]
```

The compiler may emit only:

```text
admitted_relative_bundle_problem
```

It emits no optimized camera, landmark, reprojection, covariance, metric, or sparse-geometry authority.

## Exact basis

Every generation binds:

```text
pose_refinement_digest
camera_binding_basis_digest
landmark_seed_basis_digest
observation_basis_digest
policy_digest
generation
```

The compiler independently recomputes every table identity before admission. Local paths, parsing work, pruning rounds, graph path expansions, and successful operation ceilings remain operation evidence rather than semantic identity.

## Camera and calibration binding

Every camera in the exact pose-refinement generation must have exactly one binding containing:

```text
camera node identity
source sample identity
exact frame-object digest
exact effective calibration digest
```

The sample identity must equal the retained pose sample. Missing, duplicate, stale, or extra camera bindings fail closed. This prevents a later optimizer from combining one pose generation with another crop, stabilization, lens, or calibration state.

## Landmark seeds

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

A landmark must live in the same pose component and arbitrary relative-scale gauge as every observing camera. Seeds remain proposals; compilation does not certify their coordinates.

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

At most one observation may name a camera/landmark pair, and one camera feature may support at most one landmark. Optimize observations may enter the factor graph. Held-out observations are retained outside it and can later adjudicate an optimizer without self-certification on its training residuals.

## Deterministic support-core pruning

Before graph analysis, the compiler repeatedly applies these semantic gates to optimize observations until a fixed point:

1. reject dynamic-masked or over-uncertain observations;
2. prune landmarks observed by fewer than the required independent cameras;
3. prune non-root cameras supporting fewer than the required active landmarks;
4. remove observations whose camera or landmark was pruned;
5. repeat until no membership changes.

Component roots are never silently removed. A root without sufficient active landmark support is an explicit gauge-root evidence defect.

Every input observation ends with one stable disposition. Pruning cannot erase evidence or turn an observation into an implicit absence.

## Bipartite topology certificate

Active cameras and landmarks form one deterministic bipartite graph. Internal graph-node identities are canonical and separate from domain identities. Optimize observations become graph edges. The graph oracle derives:

```text
connected components
maximum-priority forest
fundamental cycles
bridges
cycle rank
root reachability
```

The compiler maps every bridge back to the exact observation that created it. A connected but bridge-dependent problem is not described as redundant.

## Component decisions

Each pose component receives exactly one structural status and decision:

| Status | Meaning | Decision |
|---|---|---|
| `blocked_pose_refinement` | Upstream pose decision is blocked | `block` |
| `no_training_core` | No supported optimize core survives | `block` |
| `gauge_root_unobserved` | The fixed root lacks required landmark support | `block` |
| `disconnected_from_gauge_root` | Active cameras are not connected to the root through landmark evidence | `block` |
| `tree_like` | The active bipartite graph has no redundant cycle | `admit_diagnostic` |
| `fragile` | Cycles exist, but one or more optimize observations remain bridges | `admit_diagnostic` |
| `missing_held_out_evidence` | Training topology is redundant but independent adjudication evidence is insufficient | `admit_diagnostic` |
| `redundant` | Root-connected, cyclic, bridge-free, and held-out-supported | `admit` |

A nominal equation count may be reported as planning evidence, but it is never presented as a numerical-rank proof.

## Agent-facing recommendations

The component status compiles directly into one minimum-cost next action:

```text
resolve upstream pose conflict
add multi-view landmark support
observe the gauge-root neighborhood
capture a bridging view
reinforce bridge observations
reserve independent held-out views
proceed to bounded bundle optimization
```

This is a decision certificate, not prose inferred independently by each agent.

## Hard nonclaims

The compiler does not:

- estimate or refine camera rotations, centers, intrinsics, distortion, rolling shutter, or landmarks;
- minimize reprojection error;
- compute a Hessian, covariance, Schur complement, or numerical rank;
- admit a metric scale;
- publish sparse geometry;
- treat a structurally connected graph as accuracy evidence;
- let optimize observations serve simultaneously as held-out evidence.

A future optimized implementation must prove equivalence to this scalar ordered-map/full-recompute oracle before it can replace the reference path.
