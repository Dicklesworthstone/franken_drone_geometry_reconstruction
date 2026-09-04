# Target Crate Graph

Crates exist only to create semantic, trust, ownership, performance, or verification boundaries. The dependency graph is a strict DAG.

## Current executable reference spine

The present 28-member workspace implements this principal semantic dependency direction:

```text
fdgr-types
  → fdgr-codec
    ├→ fdgr-evidence → fdgr-ledger
    ├→ fdgr-object-store
    ├→ fdgr-media
    │   ├→ fdgr-media-custody
    │   ├→ fdgr-media-timeline
    │   └→ fdgr-media-worker
    ├→ fdgr-recorded-media → fdgr-recorded-media-verify
    ├→ fdgr-clock
    ├→ fdgr-calibration
    ├→ fdgr-scale
    ├→ fdgr-keyframe
    ├→ fdgr-correspondence
    ├→ fdgr-epipolar
    └→ fdgr-relative-pose

fdgr-types + fdgr-codec
  → fdgr-graph
    → fdgr-pose-graph
      → fdgr-edge-scale
        → fdgr-global-pose
          → fdgr-pose-refinement
            → fdgr-bundle-problem
              → fdgr-bundle-admission

all reference surfaces
  → fdgr-core
  → fdgr-cli
```

`fdgr-bundle-problem` and `fdgr-bundle-admission` are intentionally separate:

```text
structural camera/landmark support and topology
  ≠ exact image-domain validity
  ≠ optimize-only seed provenance
  ≠ independently usable held-out evidence
  ≠ numerical conditioning
  ≠ optimization
```

A future optimizer must consume an admitted `fdgr.bundle_admission/1` generation over the exact
`fdgr.bundle_problem/1` digest. It may not bypass the audit by reconstructing equivalent-looking
raw tables independently.

## Target semantic graph

```text
fdgr-types        fdgr-error        fdgr-codec
      \              |              /
       └──────── fdgr-evidence ─────┘
                    |
     ┌──────────────┼─────────────────────────┐
     v              v                         v
fdgr-time-cal   fdgr-object-store        fdgr-policy
     |              |                         |
     └──────┬───────┴──────────────┬──────────┘
            v                      v
     fdgr-constraints         fdgr-custody
            |                      |
     ┌──────┼───────────┐          ├── fdgr-atp
     v      v           v          └── fdgr-archive
fdgr-pose fdgr-depth fdgr-coverage
     \      |          /
      └── fdgr-geometry ── fdgr-scene ── fdgr-graph
                                  \       /
                                   fdgr-query
                                      |
                                 fdgr-objectives
                                      |
                                  fdgr-planner
                                      |
                 ┌────────────────────┼──────────────────┐
                 v                    v                  v
             fdgr-agent          fdgr-obligation    fdgr-explain
                 |                    |                  |
                 └────────────── fdgr-service ──────────┘
                                   /        \
                              fdgr-cli    fdgr-mcp

Effect adapters, each behind traits and process/device capabilities:
fdgr-source-file · fdgr-source-dji · fdgr-media-worker · fdgr-model-worker

Verification:
fdgr-reference · fdgr-lab · fdgr-harness · fdgr-bench · fdgr-qualify
```

## Planned bundle-optimization subgraph

The next geometry crates must preserve the current audit as their immutable root:

```text
fdgr-bundle-admission
  → fdgr-reprojection
    → fdgr-bundle-reference
      → fdgr-bundle-checkpoint
        → fdgr-bundle-adjudication
          → fdgr-sparse-reconstruction
```

The intended responsibilities are:

- `fdgr-reprojection`: deterministic calibrated projection/unprojection, distortion, image-domain,
  and residual-family reference semantics; no optimizer.
- `fdgr-bundle-reference`: scalar ordered-map/full-recompute joint pose-landmark proposal solver;
  no promotion authority by itself.
- `fdgr-bundle-checkpoint`: exact resumable proposal state, cancellation, stale-basis refusal,
  and crash/recovery semantics.
- `fdgr-bundle-adjudication`: compare proposals on held-out observations, conditioning evidence,
  prior retention, and explicit accept/reject/diagnostic decisions.
- `fdgr-sparse-reconstruction`: publish an immutable relative sparse generation only after exact
  adjudication; metric mapping remains a later witnessed-scale operation.

The optimizer may consume only active optimize observations certified by the audit. Held-out
observations remain outside its objective and are reserved for independent adjudication.

## Layer rules

- Foundation crates are deterministic and effect-free where practical.
- Evidence and codec crates do not depend on devices, models, search, or presentation.
- Geometry and scene cognition cannot depend on effect adapters.
- Structural bundle compilation cannot be interpreted as numerical optimizer admission.
- Bundle optimization cannot consume a structural problem without its exact admitted audit.
- Held-out observations cannot enter seed initialization or the optimization objective.
- A calibration digest establishes identity, not calibration accuracy.
- A nominal equation surplus is planning evidence, not a numerical-rank certificate.
- Agent packet construction depends on typed semantic projections, never presentation-framework types.
- Only `fdgr-mcp` may depend on `fastmcp_rust`.
- External workers communicate through owned bounded schemas and immutable objects.
- Franken adapters enter behind FDGR semantic traits after differential admission.
- No crate may introduce a second async runtime or C/C++ FFI.

## Reference implementations

Simple ordered-map, scalar, full-recompute, and single-threaded implementations define semantics first. Optimized FrankenSQLite, FrankenFS, Frankensearch, FrankenGraphDB/NetworkX, SIMD, GPU, incremental, and ATP paths must pass differential and fault gates against those references.

A faster bundle solver may replace neither `fdgr-bundle-problem` nor `fdgr-bundle-admission`. It must
prove semantic equivalence for its proposal-generation contract and then pass independent held-out
adjudication before any stronger authority is published.
