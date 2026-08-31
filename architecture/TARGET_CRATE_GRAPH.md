# Target Crate Graph

Crates exist only to create semantic, trust, ownership, performance, or verification boundaries. The dependency graph is a strict DAG.

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

## Layer rules

- Foundation crates are deterministic and effect-free where practical.
- Evidence and codec crates do not depend on devices, models, search, or presentation.
- Geometry and scene cognition cannot depend on effect adapters.
- Agent packet construction depends on typed semantic projections, never presentation-framework types.
- Only `fdgr-mcp` may depend on `fastmcp_rust`.
- External workers communicate through owned bounded schemas and immutable objects.
- Franken adapters enter behind FDGR semantic traits after differential admission.
- No crate may introduce a second async runtime or C/C++ FFI.

## Reference implementations

Simple ordered-map, scalar, full-recompute, and single-threaded implementations define semantics first. Optimized FrankenSQLite, FrankenFS, Frankensearch, FrankenGraphDB/NetworkX, SIMD, GPU, incremental, and ATP paths must pass differential and fault gates against those references.
