# FDGR Design Corpus Index

The project is organized around the agent control loop rather than subsystem ownership. Normative
conflicts are resolved in favor of the comprehensive plan, Agent Operating Model, semantics
manifest, ADRs, and machine registries.

## Start from the agent’s seat

1. [`docs/AGENT_QUICKSTART.md`](docs/AGENT_QUICKSTART.md) — the intended first session.
2. [`docs/AGENT_OPERATING_MODEL.md`](docs/AGENT_OPERATING_MODEL.md) — normative cognitive/control center.
3. [`architecture/AGENT_ABSTRACTION_TOWER.md`](architecture/AGENT_ABSTRACTION_TOWER.md) — the linked levels of understanding.
4. [`architecture/QUESTION_OBJECTIVE_GRAPH.md`](architecture/QUESTION_OBJECTIVE_GRAPH.md) — mission → question → evidence → action.
5. [`architecture/AGENT_NARROW_WAIST.md`](architecture/AGENT_NARROW_WAIST.md) — eleven stable semantic operations.
6. [`architecture/CONTEXT_PACKS.md`](architecture/CONTEXT_PACKS.md) — bounded context and Pack DNA.
7. [`architecture/AGENT_ACCRETION.md`](architecture/AGENT_ACCRETION.md) — episodes, surprise, learning, rollback.
8. [`docs/AGENT_ACCEPTANCE_SCENARIOS.md`](docs/AGENT_ACCEPTANCE_SCENARIOS.md) — driver-seat tests.

## System constitution

- [`README.md`](README.md) — target-state product narrative and current boundary.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — compact synthetic system map.
- [`COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md`](COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md) — normative plan and traceability.
- [`architecture/SEMANTICS_MANIFEST.md`](architecture/SEMANTICS_MANIFEST.md) — identity, anchors, time, space, epistemics, authority, publication.
- [`architecture/REGISTRY_TRACEABILITY_SUPPLEMENT.md`](architecture/REGISTRY_TRACEABILITY_SUPPLEMENT.md) — stable IDs introduced after the plan's embedded revision-0.4 appendix.
- [`FRANKENSTACK_DEEP_DIVE.md`](FRANKENSTACK_DEEP_DIVE.md) — project-by-project and compound design synthesis.
- [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md) — strict safe-Rust closed universe.
- [`LOCAL_QUALIFICATION_AND_RELEASE.md`](LOCAL_QUALIFICATION_AND_RELEASE.md) — Doodlestein-native release authority.
- [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) — earned evidence versus target state.

## Execution and physical-world reasoning

- [`architecture/PROCESS_REGION_TREE.md`](architecture/PROCESS_REGION_TREE.md)
- [`architecture/TARGET_CRATE_GRAPH.md`](architecture/TARGET_CRATE_GRAPH.md)
- [`architecture/OBJECT_GRAPH_FORMAT.md`](architecture/OBJECT_GRAPH_FORMAT.md)
- [`architecture/ALGORITHM_PORTFOLIO.md`](architecture/ALGORITHM_PORTFOLIO.md)
- [`architecture/ACTIVE_PERCEPTION.md`](architecture/ACTIVE_PERCEPTION.md)
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

## Current code and future reference contracts

Current compiled scaffold crates are `fdgr-types`, `fdgr-core`, and `fdgr-cli`. Target reference
crate contracts are documented under:

- [`crates/fdgr-evidence/README.md`](crates/fdgr-evidence/README.md)
- [`crates/fdgr-graph/README.md`](crates/fdgr-graph/README.md)
- [`crates/fdgr-transfer/README.md`](crates/fdgr-transfer/README.md)
- [`crates/fdgr-lab/README.md`](crates/fdgr-lab/README.md)
- [`crates/fdgr-agent/README.md`](crates/fdgr-agent/README.md)

These directories are design contracts, not currently compiled workspace members. They become
crates only when the corresponding semantic boundary and gate justify them.

## Shared cockpit and physical-world legibility

- [`architecture/DECISION_FRAME.md`](architecture/DECISION_FRAME.md) — the singular proof-carrying decision object.
- [`architecture/ATTENTION_AND_EPISTEMIC_DEBT.md`](architecture/ATTENTION_AND_EPISTEMIC_DEBT.md) — stable interruption and explicit cost of not knowing.
- [`architecture/SPATIAL_SEMANTIC_HANDLES.md`](architecture/SPATIAL_SEMANTIC_HANDLES.md) — frame-complete physical references and semantic zoom.
- [`architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md`](architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md) — evidence-aware human pilot cards and closed-loop confirmation.
- [`architecture/AGENT_METRICS.md`](architecture/AGENT_METRICS.md) — behavioral qualification of agent intuition, economy, and accretion.
