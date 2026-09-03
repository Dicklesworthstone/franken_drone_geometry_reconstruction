# Deterministic Translation-Only Pose Refinement Reference

**Status:** frozen reference contract; production admission remains open  
**Public schema:** `fdgr.pose_refinement/1`  
**Capability:** `CAP-POSE-REFINE`  
**Owning work:** `WP-018/C01`, `WP-018/I02`, `WP-018/V04`, and the diagnostic adapter portion of `WP-026`

This contract defines the first refinement step after
`fdgr.global_pose_initialization/1`. It deliberately solves a smaller problem than bundle
adjustment so agents and downstream code cannot confuse “the pose residual went down” with
“the physical reconstruction is globally or metrically correct.”

## Authority boundary

```text
component-relative pose initialization
  → fixed-rotation, fixed-relative-scale translation relaxation
    → component-relative refined camera centers
```

The operation may change camera centers inside an already admitted component gauge. It may not
change or infer:

- camera rotations;
- pairwise relative edge-scale gauges;
- calibration or clock models;
- landmarks, depth, surfaces, or appearance;
- metric scale;
- transforms between disconnected components;
- global trajectory publication;
- full pose-and-landmark bundle adjustment.

The output authority remains:

```text
relative_component_gauge
```

Its positional unit remains:

```text
component_edge_scale_unit_nano
```

## Exact basis

Every generation binds:

- the exact `fdgr.global_pose_initialization/1` semantic digest;
- the exact semantic refinement-policy digest;
- one immutable generation number.

The retained initialization is replayed before use. A stale or substituted basis fails before any
refined result is returned.

## Fixed factor model

Each comparable pose edge supplies a displacement constraint:

```text
C_right - C_left = d_edge
```

`d_edge` is derived from the already admitted pairwise translation direction, relative edge scale,
and component-root orientation convention. The operation does not re-estimate those quantities.

A factor is active only when its edge scale belongs to the same arbitrary gauge as the containing
pose component. An edge from another scale gauge remains
`incomparable_scale_gauge` and contributes no optimization weight.

Components with upstream `orientation_conflicted` or `scale_conflicted` status are reported as
`blocked_upstream_conflict`. Translation-cycle conflict is not an upstream block; it is the
principal evidence this reference relaxation is intended to adjudicate without erasing.

## Robust fixed weights

The scalar oracle assigns each active factor:

1. a support-derived base weight capped by policy;
2. a fixed robust weight derived from its initial maximum-coordinate residual;
3. full weight inside the configured `huber_delta_nano`;
4. a deterministic positive downweight outside that transition.

Weights are frozen for the generation. This makes the objective exact and replayable rather than
allowing iteration-dependent weight drift to hide a changed estimator.

The minimized objective is the fixed robust weighted sum of squared displacement residuals. Output
reports a bounded weighted RMS residual rather than serializing an unsafe-width raw sum.

## Deterministic solver

The reference solver is deliberately simple:

- process components by canonical root ID;
- pin each component root at its initialization zero origin;
- process non-root nodes in canonical node order;
- compute the weighted mean of incident factor proposals;
- retain a positive damping pseudo-weight on the current iterate;
- use fixed-point checked arithmetic and deterministic rounding;
- accept only sweeps that strictly reduce the objective;
- revert a non-improving sweep;
- stop at convergence or the semantic iteration ceiling;
- enforce an independent nonsemantic operation ceiling before partial output can escape.

A larger successful operation ceiling does not mint a different semantic digest.

## Component decision card

Every component publishes a typed terminal status:

```text
singleton
blocked_upstream_conflict
no_comparable_factors
already_satisfied
converged
iteration_limit
no_improvement
```

It also publishes exactly one decision:

```text
accept_refined
retain_initialization
blocked
```

and one reason:

```text
singleton
already_satisfied
objective_improved
no_improvement
no_comparable_factors
upstream_conflict
```

`accept_refined` requires strict objective improvement. Equal objective leaves
`do_nothing_dominates = true`.

## Factor evidence

Every pose edge remains visible with:

- endpoints and component root;
- base and robust weights;
- measured displacement when gauge-comparable;
- initialization-implied and refined-implied displacement;
- initial and final maximum-coordinate residual;
- one disposition:
  - `active_full_weight`;
  - `active_downweighted`;
  - `incomparable_scale_gauge`;
  - `blocked_upstream_conflict`.

Downweighting or a lower aggregate objective does not silently reject, delete, or rewrite the
underlying relative-pose observation.

## Agent interpretation

An agent may say:

- “component 1’s fixed translation-factor objective improved”;
- “edge 30 was retained but downweighted from its initial residual”;
- “the component root stayed pinned and the result remains in scale gauge 10”;
- “retaining the initialization dominates because the active factors were already satisfied.”

An agent must not say:

- “bundle adjustment succeeded”;
- “the camera trajectory is globally accurate”;
- “the result is in meters”;
- “the downweighted edge is false”;
- “landmarks or depth were optimized”;
- “a converged scalar objective proves physical-world accuracy.”

## Remaining `WP-018` work

This reference does not close `WP-018`. Remaining work includes at least:

- rotation and landmark variables;
- calibrated reprojection residuals and robust residual-family comparison;
- loop-candidate branch policy and explicit factor admission/rejection;
- conditioning and gauge-rank evidence;
- resumable checkpoint serialization and stale-checkpoint refusal;
- cancellation, crash, restart, drain, and reconciliation;
- scalar-versus-optimized differential admission;
- held-out-view and ground-truth improvement evidence;
- Agent Turn Packet and Decision Frame projection;
- retained local qualification receipts.

The reference exists to give those later implementations an exact semantic oracle and a safe,
agent-legible lower bound.
