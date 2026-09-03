# Pose Graph and Component-Relative Global Pose Reference

**Status:** current deterministic reference behavior, not production admission  
**Public schemas:** `fdgr.graph_analysis/1`, `fdgr.pose_graph_generation/1`, `fdgr.edge_scale_generation/1`, `fdgr.global_pose_initialization/1`  
**Owning work:** `WP-014`, `WP-018`, and the public adapter portion of `WP-026`

This document is the shortest safe orientation for the current multi-view pose frontier. It exists because the words *graph*, *pose*, *scale*, *global*, and *trajectory* are easy to collapse into a stronger claim than the code has earned.

## One evidence path, four authority levels

```text
admitted pairwise relative-pose evidence
  → deterministic graph topology
    → component-local camera orientations
      → correlation-aware relative edge-baseline gauges
        → component-relative camera centers
```

Each arrow narrows and makes explicit an assumption. No step upgrades the result to meters, geodetic coordinates, a globally connected scene, a bundle-adjusted trajectory, or published reconstruction geometry.

| Level | Current object | What it establishes | What it does not establish |
|---|---|---|---|
| Topology | `fdgr.graph_analysis/1` | connected components, deterministic forest, bridges, non-forest edges, fundamental-cycle witnesses | any geometric transform |
| Orientation | `fdgr.pose_graph_generation/1` | `R_node_from_component_root` and rotation-cycle consistency inside each component | camera centers or synchronized translation magnitudes |
| Relative edge scale | `fdgr.edge_scale_generation/1` | ratios among pairwise baselines within explicit arbitrary edge-scale components | metric scale or comparability across disconnected gauges |
| Pose initialization | `fdgr.global_pose_initialization/1` | deterministic camera orientations and centers in one zero-origin arbitrary gauge per admitted component | nonlinear refinement, covariance, landmarks, metric pose, or trajectory publication |

The public capability names are correspondingly narrow:

```text
CAP-GRAPH-ANALYZE
CAP-POSE-GRAPH-BUILD
CAP-EDGE-SCALE-RESOLVE
CAP-GLOBAL-POSE-INITIALIZE
```

## Exact input universe

The public path consumes three authenticated, bounded tables:

1. **nodes** — stable node ID, sample index, and exact keyframe digest;
2. **pose edges** — exact relative-pose verification identity, selected candidate, endpoints, `R_right_from_left`, unit translation direction, support, and residual priority;
3. **scale witnesses** — exact evidence identity, declared correlation group, edge pair, rational baseline ratio, uncertainty, support, and provenance class.

Every file is hashed before parsing. A byte change under a stale supplied digest is refused. The pose graph and edge-scale generations are rebuilt through one shared CLI construction seam; global-pose initialization cannot silently parse a different evidence universe from `edge-scale-resolve`.

## Transform convention

For a pairwise edge from left camera `l` to right camera `r`:

```text
x_r = R_right_from_left · x_l + t_right_from_left
```

The stored translation is a unit direction in the right camera frame. After resolving an arbitrary relative baseline magnitude `s`, camera centers expressed in the component-root frame satisfy:

```text
C_r - C_l = -transpose(R_right_from_root) · (s · t_right_from_left)
```

Rotations, directions, relative scales, and camera centers use fixed-point nano units. The emitted positional unit is named:

```text
component_edge_scale_unit_nano
```

It is intentionally not called a meter, world unit, building coordinate, or trajectory unit.

## Deterministic gauge policy

For every connected pose component:

- the canonical component root is the zero camera center;
- the graph-selected forest determines parentage and propagation order;
- every initialized pose retains `parent_node_id` and `parent_edge_id`;
- all forest edges must belong to one comparable edge-scale gauge;
- disconnected pose components retain separate zero origins and separate scale roots;
- absent evidence never manufactures a transform between components.

A singleton camera is a valid component with a zero origin and no scale root. A tree can be initialized without redundant translation evidence, but its status remains `tree_initialized` rather than cycle-validated.

## Redundant evidence and conflict preservation

Every non-forest pose edge closes one deterministic fundamental cycle. The initializer compares:

- displacement implied by forest-propagated camera centers; and
- displacement measured by the closing edge when its scale gauge is comparable.

The cycle result is one of:

```text
consistent
conflicting
incomparable_scale_gauge
```

Component status conservatively preserves upstream and local contradiction:

```text
singleton
tree_initialized
cycle_consistent
orientation_conflicted
scale_conflicted
translation_conflicted
```

A conflict is output evidence, not a request to average until the contradiction disappears. A downstream optimizer must consume the conflict explicitly, branch, reject evidence under a registered policy, or refuse.

## Identity and budgets

Semantic generation identities cover exact upstream generation digests, semantic policy, canonical poses, cycle assessments, and component summaries. Successful semantic identity excludes operation ceilings and observed operation counts; increasing a ceiling without changing the result does not mint a different pose generation.

Operation ceilings remain visible evidence and fail closed before a partial result can be returned. Public numeric domains are bounded to values that round-trip exactly through the JSON contract.

## Public commands

```bash
fdgr pose-graph-build <nodes.tsv> <pose-edges.tsv> [exact identities and gates] --format json
fdgr edge-scale-resolve <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> [exact identities and gates] --format json
fdgr global-pose-initialize <nodes.tsv> <pose-edges.tsv> <scale-witnesses.tsv> [exact identities and gates] --format json
```

The final command returns the exact pose-graph and edge-scale generation identities it consumed, its own semantic initialization digest, component authority, operation evidence, initialized poses, translation-cycle assessments, and component summaries.

## Agent interpretation rules

An agent can safely say:

- “three cameras were initialized in component 1’s arbitrary edge-scale gauge”;
- “the component has a translation-cycle conflict on edge 30”;
- “components 1 and 10 have independent origins and incomparable scales”;
- “the forest parent of node 3 is node 2 through edge 20.”

An agent must not infer:

- meters, dimensions, velocity, or GPS position;
- a transform between disconnected components;
- that `cycle_consistent` means accurate against the physical world;
- that a high-support forest edge is an inlier after future global refinement;
- that initialization is bundle adjustment;
- that a camera-center generation contains sparse points, depth, surfaces, or uncertainty calibration.

When a consumer requires any of those stronger statements, the safe next step is not relabeling. It is to name the missing witness or work package.

## Remaining `WP-018` boundary

The current reference source is an initialization substrate. `WP-018` remains open for at least:

- robust objective and residual families;
- deterministic nonlinear pose/landmark refinement;
- explicit gauge constraints and conditioning evidence;
- loop-candidate branch policy and outlier decisions;
- resumable checkpoints and stale-checkpoint refusal;
- cancellation, crash, recovery, and indeterminate outcome semantics;
- agent-readable decision cards;
- scalar/reference versus optimized equivalence;
- retained local qualification receipts.

Metric authority remains separately gated by the scale-witness system. Pose refinement cannot create metric evidence merely by converging.

## Qualification boundary

The repository contains unit fixtures and a public-path E2E campaign for deterministic replay, cycle consistency, translation conflict, independent component gauges, stale-basis refusal, and operation-budget refusal. Those artifacts are necessary evidence, not a blanket `GATE-009` receipt. The exact commit still requires the repository-owned pinned-toolchain lane and the broader fault, recovery, differential, and accuracy evidence named by its Beads.
