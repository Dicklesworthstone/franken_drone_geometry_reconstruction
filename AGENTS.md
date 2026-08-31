
# AGENTS.md — franken_drone_geometry_reconstruction

## First obligation

Read all of this file, `README.md`, `ARCHITECTURE.md`,
`COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md`,
`FRANKENSTACK_DEEP_DIVE.md`, `IMPLEMENTATION_STATUS.md`, and the relevant registries before making
substantive changes. The plan and machine registries are normative.

## Development doctrine

1. Extract and freeze semantics before implementation.
2. Build a simple deterministic reference path before an optimized or model-heavy path.
3. Preserve exact evidence; derived assets never overwrite originals.
4. Treat clocks, coordinate frames, calibration, scale, uncertainty, and coverage as first-class
   types, not comments.
5. Model outputs are proposals. Validate basis, schema, numeric domain, coordinates, identity,
   license, and evidence before use.
6. Use `reserve → materialize → verify → publish` for every coherent generation.
7. Distinguish request durability, adapter/process acceptance, observed output, verified output,
   publication, and semantic completion.
8. Never detach work. All async work belongs to an Asupersync region and receives `&Cx`.
9. Do not add Tokio, Rayon, reqwest, hyper, axum, tower, SQLx, an ORM, C/C++ FFI, or in-process
   Python.
10. Every FDGR crate begins with `#![forbid(unsafe_code)]`. Do not create exceptions.
11. Do not use `unwrap`, `expect`, `panic!`, `todo!`, `unimplemented!`, or `dbg!` in shipping code.
12. Prefer typed errors and stable IDs. Machine output uses versioned schemas.
13. Stable ordering is a contract. Hash-map iteration is not a tie-break policy.
14. Adaptivity may choose effort but cannot weaken correctness, privacy, scale, freshness,
   completeness, authority, or evidence gates.
15. Local qualification receipts, not hosted badges, determine release readiness.
16. Public machine fields and enum values use one registry-derived lower `snake_case` vocabulary; payload schema identities use `fdgr.<name>/1`.

## Rust and crate graph

Use the pinned latest-nightly toolchain and edition 2024. Keep crates focused. Split a crate only
when it establishes a dependency, authority, trust, failure, or verification boundary. Keep pure
geometry/math/codec cores free of I/O and runtime assumptions. Asupersync belongs at orchestration
seams, not inside every scalar kernel.

The dependency graph is a strict DAG. Run `python3 scripts/validate_repo.py` after changing Cargo
manifests or registries.

## External tools and models

ffmpeg and model runtimes are process sidecars. Do not “temporarily” link libav*, PyTorch, CUDA,
or vendor SDKs through FFI. Sidecars receive sealed manifests and content-addressed inputs. Network
is disabled by default. Validate outputs and drain descendants.

Never download “latest” weights during a build or test. Exact artifacts are provisioned separately,
verified, and named by digest. Every model addition updates `registries/models.toml`,
`MODEL_REGISTRY.md`, the dependency/license evidence, fixtures, and admission tests.

## DJI and device research

Operate only the user's own authorized devices and accounts. Do not bypass pairing,
authentication, encryption, access controls, or geographic restrictions. Begin read-only. Do not
expose arbitrary packet injection or flight commands. Exact aircraft/controller/app/firmware/OS/
region identity is mandatory.

## Geometry correctness

- Never emit meters when `ScaleStatus` is relative or estimated.
- Never mix OpenCV, OpenGL, three.js, ROS, geodetic, camera, or building frames implicitly.
- Reject NaN/infinity, invalid rotations, singular calibration, and unbounded coordinates.
- Keep raw observations separate from refined states.
- Foundation-model confidence is not calibrated uncertainty.
- Gaussian splats and rendered views are appearance products, not canonical geometry.
- Validate with held-out views, independent geometry metrics, and ground truth where available.

## Semantic correctness

Use the ontology. Preserve `Observation`, `Hypothesis`, `Resolved`, `Rejected`, and
`Indeterminate`. A caption cannot jump directly to resolved. Critical assets need elevated gates.
Hidden utilities are not observed facts. Absence needs a coverage/detectability witness.

## Testing

Every semantic transition needs deterministic fixtures. Fault campaigns include cancellation,
process crash, packet loss/reorder/duplication, timestamp reset/drift, malformed media, disk full,
partial publication, cloud substitution, model NaNs/huge outputs, calibration mismatch, privacy
scope violation, and stale compatibility profile.

Benchmark with one binary and runtime-selected arms. Establish semantic equality first. Retain
input roots, source commit, toolchain, model roots, hardware, policy, samples, output digests, and
statistical receipt.

## Beads

Use the `WP-*` IDs in `registries/work_packages.toml`. Preserve dependency order. Do not create
implementation issues that bypass a blocking contract or gate. When discovering a missing
prerequisite, add or update the normative work graph before coding around it.

## Documentation and status

Update `IMPLEMENTATION_STATUS.md` in the same change that earns or removes a capability. Avoid
future tense disguised as current functionality. Classify statements as fact, design, hypothesis,
or target in normative documents. Negative evidence must remain visible and cannot be summed into
a misleading aggregate readiness percentage.

## Agent-native constitutional rule

Every implementation decision must be evaluated from the operating agent's seat. The public system is the loop in `docs/AGENT_OPERATING_MODEL.md`, not a collection of crate-specific APIs. New surfaces MUST project through the Agent Turn Packet, the abstraction tower, the four ledgers, and the objective/question graph. They MUST preserve exact anchors, epistemic class, active work, uncertainty, coverage, budgets, recovery, and typed next steps.

Do not make the agent remember protocol facts the server can represent. Do not add a top-level operation merely because a subsystem has a new capability. Prefer a typed affordance compiled through `fdgr.propose`, then `fdgr.commit` and `fdgr.watch`. Recommendations, memories, counterfactuals, and model outputs are authority-free.

Before considering an agent-facing change complete, run the scenarios in `docs/AGENT_ACCEPTANCE_SCENARIOS.md` and ask whether a cold agent can understand the state, choose the minimum-cost safe next step, verify completion, and leave a sufficient handoff without reading source or replaying the transcript.
