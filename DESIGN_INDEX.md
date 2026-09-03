# FDGR Design and Implementation Index

FDGR is organized around one agent control loop and one evidence universe rather than isolated subsystem APIs. Normative conflicts are resolved in favor of the comprehensive plan, Agent Operating Model, semantics manifest, ADRs, and machine registries. Current implementation truth is recorded separately in `IMPLEMENTATION_STATUS.md`.

## Start from the agent’s seat

1. [`README.md`](README.md)
2. [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md)
3. [`docs/AGENT_QUICKSTART.md`](docs/AGENT_QUICKSTART.md)
4. [`docs/AGENT_OPERATING_MODEL.md`](docs/AGENT_OPERATING_MODEL.md)
5. [`architecture/AGENT_ABSTRACTION_TOWER.md`](architecture/AGENT_ABSTRACTION_TOWER.md)
6. [`architecture/QUESTION_OBJECTIVE_GRAPH.md`](architecture/QUESTION_OBJECTIVE_GRAPH.md)
7. [`architecture/AGENT_NARROW_WAIST.md`](architecture/AGENT_NARROW_WAIST.md)
8. [`architecture/CONTEXT_PACKS.md`](architecture/CONTEXT_PACKS.md)
9. [`architecture/DECISION_FRAME.md`](architecture/DECISION_FRAME.md)
10. [`architecture/AGENT_ACCRETION.md`](architecture/AGENT_ACCRETION.md)
11. [`docs/AGENT_ACCEPTANCE_SCENARIOS.md`](docs/AGENT_ACCEPTANCE_SCENARIOS.md)

## System constitution

- [`README.md`](README.md) — product narrative, current executable frontier, and explicit non-claims.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — compact synthetic system map.
- [`COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md`](COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md) — normative plan and original traceability appendix.
- [`architecture/SEMANTICS_MANIFEST.md`](architecture/SEMANTICS_MANIFEST.md) — identities, anchors, time, space, epistemics, authority, and publication.
- [`architecture/REGISTRY_TRACEABILITY_SUPPLEMENT.md`](architecture/REGISTRY_TRACEABILITY_SUPPLEMENT.md) — stable IDs introduced after the embedded plan snapshot.
- [`FRANKENSTACK_DEEP_DIVE.md`](FRANKENSTACK_DEEP_DIVE.md) — sibling-project and compound design synthesis.
- [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md) — strict safe-Rust closed universe.
- [`LOCAL_QUALIFICATION_AND_RELEASE.md`](LOCAL_QUALIFICATION_AND_RELEASE.md) — Doodlestein-native release authority.
- [`QUALIFICATION.md`](QUALIFICATION.md) — exact earned and unearned evidence boundary.
- [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) — source, reference, public-path, qualification, and production-admission status.
- [`CHANGELOG.md`](CHANGELOG.md) — executable evolution and semantic corrections.

## Current executable evidence stack

The workspace currently has **25 package members**. The principal dependency direction is:

```text
fdgr-types
  → fdgr-codec
    → fdgr-evidence / fdgr-ledger / fdgr-object-store
      → fdgr-media / fdgr-media-custody / fdgr-recorded-media
        → fdgr-recorded-media-verify / fdgr-media-timeline / fdgr-media-worker
          → fdgr-clock / fdgr-calibration / fdgr-scale
            → fdgr-keyframe
              → fdgr-correspondence
                → fdgr-epipolar
                  → fdgr-relative-pose
                    → fdgr-graph
                      → fdgr-pose-graph
                        → fdgr-edge-scale
                          → fdgr-global-pose
                            → fdgr-core / fdgr-cli
```

This is a deterministic reference chain, not an end-to-end reconstruction or production-qualification claim.

### Media, time, calibration, and scale

- [`architecture/SENSOR_EVIDENCE_REFERENCE.md`](architecture/SENSOR_EVIDENCE_REFERENCE.md)
- [`crates/fdgr-codec/`](crates/fdgr-codec/)
- [`crates/fdgr-evidence/`](crates/fdgr-evidence/)
- [`crates/fdgr-ledger/`](crates/fdgr-ledger/)
- [`crates/fdgr-object-store/`](crates/fdgr-object-store/)
- [`crates/fdgr-media/`](crates/fdgr-media/)
- [`crates/fdgr-media-custody/`](crates/fdgr-media-custody/)
- [`crates/fdgr-recorded-media/`](crates/fdgr-recorded-media/)
- [`crates/fdgr-recorded-media-verify/`](crates/fdgr-recorded-media-verify/)
- [`crates/fdgr-media-timeline/`](crates/fdgr-media-timeline/)
- [`crates/fdgr-media-worker/`](crates/fdgr-media-worker/)
- [`crates/fdgr-clock/`](crates/fdgr-clock/)
- [`crates/fdgr-calibration/`](crates/fdgr-calibration/)
- [`crates/fdgr-scale/`](crates/fdgr-scale/)

### Two-view evidence

- [`architecture/TWO_VIEW_EVIDENCE_REFERENCE.md`](architecture/TWO_VIEW_EVIDENCE_REFERENCE.md) — authority ladder from keyframe evidence through physical candidate adjudication.
- [`crates/fdgr-keyframe/`](crates/fdgr-keyframe/) — deterministic quality and diversity selection.
- [`crates/fdgr-correspondence/`](crates/fdgr-correspondence/) — bounded descriptor hypotheses and collision-safe tracks.
- [`crates/fdgr-epipolar/`](crates/fdgr-epipolar/) — calibrated essential-matrix proposal adjudication without motion authority.
- [`crates/fdgr-relative-pose/`](crates/fdgr-relative-pose/) — fixed-point epipolar, parallax, and cheirality candidate verification.
- [`schemas/keyframe_selection.schema.json`](schemas/keyframe_selection.schema.json)
- [`schemas/correspondence_generation.schema.json`](schemas/correspondence_generation.schema.json)
- [`schemas/epipolar_verification.schema.json`](schemas/epipolar_verification.schema.json)
- [`schemas/relative_pose_verification.schema.json`](schemas/relative_pose_verification.schema.json)

### Multi-view pose evidence

- [`architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md`](architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md) — exact transform convention, four-level authority ladder, gauge rules, conflict semantics, and remaining `WP-018` boundary.
- [`crates/fdgr-graph/`](crates/fdgr-graph/) — deterministic components, forests, bridges, non-forest edges, and cycle witnesses without geometric authority.
- [`crates/fdgr-pose-graph/`](crates/fdgr-pose-graph/) — component-local orientations and rotation-cycle evidence; no camera centers.
- [`crates/fdgr-edge-scale/`](crates/fdgr-edge-scale/) — correlation-aware relative edge-baseline gauges; no metric authority.
- [`crates/fdgr-global-pose/`](crates/fdgr-global-pose/) — deterministic component-relative camera centers; no bundle adjustment or trajectory publication.
- [`schemas/graph_analysis.schema.json`](schemas/graph_analysis.schema.json)
- [`schemas/pose_graph_generation.schema.json`](schemas/pose_graph_generation.schema.json)
- [`schemas/edge_scale_generation.schema.json`](schemas/edge_scale_generation.schema.json)
- [`schemas/global_pose_initialization.schema.json`](schemas/global_pose_initialization.schema.json)

The required interpretation is:

```text
graph topology
≠ orientation composition
≠ relative edge scale
≠ arbitrary-gauge camera centers
≠ bundle-adjusted trajectory
≠ metric pose
≠ published geometry
```

### Public adapters and executable campaigns

- [`crates/fdgr-cli/`](crates/fdgr-cli/)
- [`crates/fdgr-core/`](crates/fdgr-core/)
- [`scripts/qualify.sh`](scripts/qualify.sh)
- [`scripts/e2e/`](scripts/e2e/)

Current public-path campaigns include recorded-media ingest/verify, timeline, clock fit, keyframes, correspondences, epipolar verification, relative-pose verification, pose-graph construction, edge-scale reconciliation, and global-pose initialization.

Subsystem commands are diagnostic/reference surfaces. The target agent protocol remains the eleven-operation semantic waist.

## Physical-world reasoning and target architecture

- [`architecture/PROCESS_REGION_TREE.md`](architecture/PROCESS_REGION_TREE.md)
- [`architecture/TARGET_CRATE_GRAPH.md`](architecture/TARGET_CRATE_GRAPH.md)
- [`architecture/OBJECT_GRAPH_FORMAT.md`](architecture/OBJECT_GRAPH_FORMAT.md)
- [`architecture/ALGORITHM_PORTFOLIO.md`](architecture/ALGORITHM_PORTFOLIO.md)
- [`architecture/ACTIVE_PERCEPTION.md`](architecture/ACTIVE_PERCEPTION.md)
- [`architecture/SENSOR_EVIDENCE_REFERENCE.md`](architecture/SENSOR_EVIDENCE_REFERENCE.md)
- [`architecture/TWO_VIEW_EVIDENCE_REFERENCE.md`](architecture/TWO_VIEW_EVIDENCE_REFERENCE.md)
- [`architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md`](architecture/POSE_GRAPH_AND_GLOBAL_POSE_REFERENCE.md)
- [`architecture/SPATIAL_SEMANTIC_HANDLES.md`](architecture/SPATIAL_SEMANTIC_HANDLES.md)
- [`architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md`](architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md)
- [`architecture/MULTI_AGENT_COORDINATION.md`](architecture/MULTI_AGENT_COORDINATION.md)
- [`architecture/SELF_DESCRIPTION.md`](architecture/SELF_DESCRIPTION.md)
- [`DJI_ADAPTER_RESEARCH.md`](DJI_ADAPTER_RESEARCH.md)
- [`MODEL_REGISTRY.md`](MODEL_REGISTRY.md)

## Exact Franken studies

- [`research/DEEP_DIVE_METHOD.md`](research/DEEP_DIVE_METHOD.md)
- [`research/TRANSFER_MATRIX.md`](research/TRANSFER_MATRIX.md)
- [`research/CROSS_PROJECT_COMPOSITION.md`](research/CROSS_PROJECT_COMPOSITION.md)
- [`research/deep-dives/01_ASUPERSYNC.md`](research/deep-dives/01_ASUPERSYNC.md)
- [`research/deep-dives/02_FRANKENSQLITE.md`](research/deep-dives/02_FRANKENSQLITE.md)
- [`research/deep-dives/03_FRANKENFS.md`](research/deep-dives/03_FRANKENFS.md)
- [`research/deep-dives/04_FRANKENSEARCH.md`](research/deep-dives/04_FRANKENSEARCH.md)
- [`research/deep-dives/05_FRANKEN_MARKDOWN.md`](research/deep-dives/05_FRANKEN_MARKDOWN.md)
- [`research/deep-dives/06_FRANKENGRAPHDB.md`](research/deep-dives/06_FRANKENGRAPHDB.md)
- [`research/deep-dives/07_FRANKEN_NETWORKX.md`](research/deep-dives/07_FRANKEN_NETWORKX.md)
- [`research/deep-dives/08_DWARF_FORTRESS_MCP.md`](research/deep-dives/08_DWARF_FORTRESS_MCP.md)
- [`research/deep-dives/09_FASTMCP_RUST.md`](research/deep-dives/09_FASTMCP_RUST.md)
- [`research/deep-dives/10_EIDETIC_ENGINE.md`](research/deep-dives/10_EIDETIC_ENGINE.md)
- [`research/deep-dives/11_DOODLESTEIN_SELF_RELEASER.md`](research/deep-dives/11_DOODLESTEIN_SELF_RELEASER.md)
- [`research/source-inventory/REPOSITORY_FORENSICS.md`](research/source-inventory/REPOSITORY_FORENSICS.md)
- [`research/source-inventory/SYMBOL_LOCUS_INDEX.md`](research/source-inventory/SYMBOL_LOCUS_INDEX.md)
- [`research/source-inventory/source_manifest.json`](research/source-inventory/source_manifest.json)

## Machine contracts

- [`architecture/agent_turn_contract.json`](architecture/agent_turn_contract.json)
- [`architecture/dependency_allowlist.toml`](architecture/dependency_allowlist.toml)
- [`architecture/qualification_lanes.toml`](architecture/qualification_lanes.toml)
- [`architecture/deep_traceability.json`](architecture/deep_traceability.json)
- [`registries/`](registries/)
- [`schemas/`](schemas/)
- [`release/source_closure.lock.json`](release/source_closure.lock.json)
- [`release/doodlestein_job_graph.json`](release/doodlestein_job_graph.json)

## Shared cockpit and agent legibility

- [`architecture/DECISION_FRAME.md`](architecture/DECISION_FRAME.md)
- [`architecture/ATTENTION_AND_EPISTEMIC_DEBT.md`](architecture/ATTENTION_AND_EPISTEMIC_DEBT.md)
- [`architecture/AGENT_METRICS.md`](architecture/AGENT_METRICS.md)
- [`architecture/AGENT_ACCRETION.md`](architecture/AGENT_ACCRETION.md)
