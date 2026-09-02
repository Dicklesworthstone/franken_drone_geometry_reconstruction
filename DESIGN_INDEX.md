# FDGR Design and Implementation Index

FDGR is organized around one agent control loop and one evidence universe rather than around isolated subsystem APIs. Normative conflicts are resolved in favor of the comprehensive plan, Agent Operating Model, semantics manifest, ADRs, and machine registries. Current implementation truth is recorded separately in `IMPLEMENTATION_STATUS.md`.

## Start from the agent’s seat

1. [`docs/AGENT_QUICKSTART.md`](docs/AGENT_QUICKSTART.md)
2. [`docs/AGENT_OPERATING_MODEL.md`](docs/AGENT_OPERATING_MODEL.md)
3. [`architecture/AGENT_ABSTRACTION_TOWER.md`](architecture/AGENT_ABSTRACTION_TOWER.md)
4. [`architecture/QUESTION_OBJECTIVE_GRAPH.md`](architecture/QUESTION_OBJECTIVE_GRAPH.md)
5. [`architecture/AGENT_NARROW_WAIST.md`](architecture/AGENT_NARROW_WAIST.md)
6. [`architecture/CONTEXT_PACKS.md`](architecture/CONTEXT_PACKS.md)
7. [`architecture/DECISION_FRAME.md`](architecture/DECISION_FRAME.md)
8. [`architecture/AGENT_ACCRETION.md`](architecture/AGENT_ACCRETION.md)
9. [`docs/AGENT_ACCEPTANCE_SCENARIOS.md`](docs/AGENT_ACCEPTANCE_SCENARIOS.md)

## System constitution

- [`README.md`](README.md) — product narrative, current reference frontier, and explicit non-claims.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — compact synthetic system map.
- [`COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md`](COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md) — normative plan and original traceability appendix.
- [`architecture/SEMANTICS_MANIFEST.md`](architecture/SEMANTICS_MANIFEST.md) — identities, anchors, time, space, epistemics, authority, and publication.
- [`architecture/REGISTRY_TRACEABILITY_SUPPLEMENT.md`](architecture/REGISTRY_TRACEABILITY_SUPPLEMENT.md) — stable IDs introduced after the embedded plan snapshot.
- [`FRANKENSTACK_DEEP_DIVE.md`](FRANKENSTACK_DEEP_DIVE.md) — sibling-project and compound design synthesis.
- [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md) — strict safe-Rust closed universe.
- [`LOCAL_QUALIFICATION_AND_RELEASE.md`](LOCAL_QUALIFICATION_AND_RELEASE.md) — Doodlestein-native release authority.
- [`QUALIFICATION.md`](QUALIFICATION.md) — current earned and unearned evidence boundary.
- [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) — source, reference, public-path, qualification, and production-admission status.
- [`CHANGELOG.md`](CHANGELOG.md) — executable evolution and semantic corrections.

## Current executable evidence stack

The current workspace has 20 package members. The principal implemented chain is:

```text
fdgr-types
  → fdgr-codec
    → fdgr-evidence / fdgr-ledger / fdgr-object-store
      → fdgr-media / fdgr-media-custody / fdgr-recorded-media
        → fdgr-recorded-media-verify / fdgr-media-timeline / fdgr-media-worker
          → fdgr-clock / fdgr-calibration / fdgr-scale
            → fdgr-keyframe
              → fdgr-correspondence
                → fdgr-relative-pose
                  → fdgr-core / fdgr-cli
```

These are reference semantics, not a claim of end-to-end reconstruction or production qualification.

### Media, time, calibration, and scale

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

- [`architecture/TWO_VIEW_EVIDENCE_REFERENCE.md`](architecture/TWO_VIEW_EVIDENCE_REFERENCE.md) — authority ladder from keyframe evidence through candidate-set adjudication.
- [`crates/fdgr-keyframe/`](crates/fdgr-keyframe/) — deterministic quality and diversity selection.
- [`crates/fdgr-correspondence/`](crates/fdgr-correspondence/) — bounded descriptor hypotheses and collision-safe tracks.
- [`crates/fdgr-relative-pose/`](crates/fdgr-relative-pose/) — fixed-point epipolar, parallax, and cheirality candidate verification.
- [`schemas/keyframe_selection.schema.json`](schemas/keyframe_selection.schema.json)
- [`schemas/correspondence_generation.schema.json`](schemas/correspondence_generation.schema.json)
- [`schemas/relative_pose_verification.schema.json`](schemas/relative_pose_verification.schema.json)

### Public adapters

- [`crates/fdgr-cli/`](crates/fdgr-cli/)
- [`crates/fdgr-core/`](crates/fdgr-core/)
- [`scripts/e2e/`](scripts/e2e/)

Subsystem commands are diagnostic/reference surfaces. The target agent protocol remains the eleven-operation semantic waist.

## Physical-world reasoning and target architecture

- [`architecture/PROCESS_REGION_TREE.md`](architecture/PROCESS_REGION_TREE.md)
- [`architecture/TARGET_CRATE_GRAPH.md`](architecture/TARGET_CRATE_GRAPH.md)
- [`architecture/OBJECT_GRAPH_FORMAT.md`](architecture/OBJECT_GRAPH_FORMAT.md)
- [`architecture/ALGORITHM_PORTFOLIO.md`](architecture/ALGORITHM_PORTFOLIO.md)
- [`architecture/ACTIVE_PERCEPTION.md`](architecture/ACTIVE_PERCEPTION.md)
- [`architecture/SENSOR_EVIDENCE_REFERENCE.md`](architecture/SENSOR_EVIDENCE_REFERENCE.md)
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
