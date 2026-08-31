# Frankenstack Deep Dive for `franken_drone_geometry_reconstruction`

**Status:** normative design input
**Revision:** 4 — agent-native synthetic-system integration
**Date:** 2026-08-31
**Source inventory:** [`research/source-inventory/source_manifest.json`](research/source-inventory/source_manifest.json)
**Per-project analyses:** [`research/deep-dives/`](research/deep-dives/)

> This revision replaces the initial feature survey with a mechanism-level investigation. Every
> imported idea now has an FDGR owner, invariant, rejected superficial imitation, deterministic
> reference model, failure boundary, admission gate, and rollback path.

---

## 1. What “go deeper” changes

The first plan correctly identified immutable evidence, three planes, metric proof, plural geometry,
DJI profile research, local-first custody, and agent-native interfaces. It did not yet exploit the
full compositional power of the Franken stack.

The deeper pass changes the center of gravity:

1. FDGR is not a video-to-mesh pipeline. It is a **versioned evidence and constraint operating
   substrate** whose meshes are projections.
2. There is not one reconstruction state. There are independently certified, incrementally
   maintained hypotheses for clocks, calibration, tracks, pose, scale, depth, surfaces, topology,
   semantics, coverage, and change.
3. All authoritative and derived state shares **one append-only evidence universe**. Every
   generation declares consumed high-water marks and roots.
4. Spatial state uses **multi-version, temperature-tiered blocks** rather than a mutable global
   point cloud.
5. Graphs are not documentation diagrams. View, track, pose, scale, surface, room, utility,
   visibility, provenance, archive, and obligation graphs are first-class query and algorithm
   substrates with deterministic certificates.
6. Geometry is maintained incrementally from evidence deltas, including retractions. Full
   recomputation remains the oracle.
7. Capture is an owned, pressure-aware region tree. Cancellation is a reconciliation protocol, not
   task dropping.
8. ATP and RaptorQ form the immutable object-graph movement and repair plane, not merely a cloud
   uploader.
9. External native/Python/model/codec tools are temporary reference or bootstrap processes. The
   shipping trust domain is closed-world, pure Rust, and `unsafe_code = "forbid"`.
10. Local Doodlestein qualification is the release authority. Hosted GitHub Actions has no
    correctness or promotion authority.

---

## 2. The thirty-two constitutional decisions

These decisions override weaker language elsewhere until the comprehensive plan is trued up.

### CD-01 — Asupersync is the only runtime

No Tokio, async-std, smol, Rayon, detached background thread, or second cancellation model enters
the production process. All owned work lives in an asupersync region and all effectful APIs receive
an explicit context.

### CD-02 — The shipping trust domain is pure Rust

Workspace crates use the latest pinned nightly, edition 2024, and `unsafe_code = "forbid"`.
External executables may act as supervised qualification/reference/bootstrap oracles, never as
silent permanent semantic foundations. No C/C++ FFI, in-process Python, OpenCV, libavcodec,
COLMAP, Ceres, g2o, RocksDB, or C SQLite enters the authoritative production process.

### CD-03 — The dependency universe is closed and exact

Allowed dependencies are `core`/`alloc`/`std`, `asupersync`, admitted owned Franken projects, and
rare basic crates named in a machine allowlist with exact versions/features/transitive closure.
Convenience is not an admission argument. Every dependency has a semantic owner, reference/fallback
story, trust boundary, source identity, and qualification receipt.

### CD-04 — Original evidence is immutable and highest authority

Accepted original packets/media, exact metadata bytes, owner measurements, and effect receipts are
never overwritten by derived output. Truncation, corruption, or gaps are durable facts. A preview,
model output, mesh, or report cannot outrank original evidence.

### CD-05 — There is one evidence universe

Observation capsules are the ordered delta stream for current views, history, reconstruction,
scene graphs, indexes, branches, subscriptions, replicas, archive, reports, and replay. A derived
root without exact input roots/high-water marks is invalid.

### CD-06 — Readers pin coherent versions

A query sees one complete anchor or an explicit temporal/branch relation. “Latest” may choose an
anchor once; it cannot mix the latest pose, mesh, labels, and index independently.

### CD-07 — Geometry is a constraint system, not a monolithic artifact

Camera, track, pose, scale, depth, surface, topology, and semantic constraints retain identity,
provenance, uncertainty, residuals, and active/retracted state. Point clouds and meshes are
materialized views of selected hypothesis generations.

### CD-08 — Multiple hypotheses remain first-class

Ambiguous loop closures, scale sources, calibrations, object associations, room boundaries, and
utility routes live on branches or in explicit hypothesis sets. FDGR never averages away a
contradiction to produce one deceptively clean twin.

### CD-09 — Metric scale is a proof obligation

`RelativeOnly`, `Estimated`, `Witnessed`, and `Surveyed` are distinct authority levels. Measurements
in meters cite a scale-witness graph, uncertainty, coordinate-frame transform, and calibration
basis. A monocular model cannot silently promote itself to metric authority.

### CD-10 — Negative reads are witnessed

“No doorway,” “no obstacle,” “no propane tank,” “no loop closure,” and “no newer calibration” are
predicates over named complete domains and policies. Absence from top-k, a cache, an ANN index, or
an incomplete view is not negative evidence.

### CD-11 — Publication is reserve → materialize → validate → publish root last

Readers never see a root naming incomplete children. Staged, visible, and durable epochs are
separate. Cancellation before commit aborts/quarantines; after commit it publishes a successor or
executes registered reconciliation.

### CD-12 — Derived generations are rebuildable

Tracks, poses, spatial fusion, meshes, scene graphs, search indexes, embeddings, reports, and
previews can be discarded and rebuilt from named authoritative roots. Caches never become truth by
surviving longer than their source.

### CD-13 — Reconstruction is incremental with a full oracle

Observation deltas incrementally update views, constraints, graphs, coverage, and indexes,
including retractions. Periodic full recomputation must produce the same canonical result under the
same policy.

### CD-14 — Graph results are certificate-bearing projections

Algorithms declare graph semantics, anchor, numeric policy, tie-breaks, budget, complexity witness,
decision path, and output digest. They produce derived claims and candidate intents, not physical
observations.

### CD-15 — Nonunique choices are deterministic

Equal-cost paths, matching ties, minimum cuts, spanning forests, loop-closure candidates, parallel
reductions, and query order follow registered canonical policies. Hash iteration and scheduler
accident are forbidden decision rules.

### CD-16 — Device/cloud effects are not ledger commits

Local plan commitment, dispatch attempt, transport acceptance, device/provider acceptance,
observed effect, terminal proof, and obligation discharge are distinct states. Unknown outcomes
remain `Indeterminate` until lookup and observation resolve them.

### CD-17 — Every consequential effect is witnessed, idempotent, and fenced

A sealed plan, capability, device/profile epoch, idempotency key, lease incarnation/fence, final
precondition, dispatch record, and terminal predicate are mandatory for admitted control effects.
Manual guidance remains a lower-authority plan type.

### CD-18 — The cognition plane has no command path

Models, search, graph algorithms, optimization, attention ranking, and agent memory can propose
hypotheses and plans. Only the effect coordinator can mint a short-lived command ticket after all
hard checks.

### CD-19 — ATP moves immutable object graphs, never command authority

Capture roots, spatial generations, models, branches, exports, crashpacks, and qualification
bundles move through verified resumable ATP/RaptorQ graphs. DJI commands, credentials, leases, and
mutable authority use a separate fenced protocol.

### CD-20 — Adaptive policy is subordinate to invariants

Bandits, e-processes, conformal monitors, learned caches, and workload adaptation may choose effort,
priority, sampling, or compression. They cannot weaken custody, freshness, scale, capability,
witness, completeness, or terminal-proof requirements. Decisions emit replayable cards.

### CD-21 — Models are untrusted proposal producers

Every checkpoint/preprocessing/numeric/runtime identity is explicit. Outputs are bounded and schema-
validated. Model claims require cross-view, geometric, topological, temporal, and/or human evidence.
Changing model space creates a new generation, never a silent in-place upgrade.

### CD-22 — Reference before optimization

Every semantic subsystem begins with the simplest deterministic implementation. Optimized Franken
adapters, SIMD, compressed layouts, approximate indexes, sparse solvers, and dynamic algorithms
enter only behind differential, adversarial, crash/cancellation, and same-binary performance gates.

### CD-23 — Agent memory is advisory and one-way

Eidetic memory may cite FDGR evidence. FDGR canonical state never cites memory as proof or authority.
Current evidence always wins over remembered context.

### CD-24 — Local qualification is release authority

A clean source snapshot, exact sibling closure, pinned toolchain, native local lane receipts, and a
sealed artifact manifest determine release truth. Workflow YAML is a portable Doodlestein job graph.
GitHub-hosted runner status is optional information with zero promotion authority.

---

### CD-25 — One agent operating loop

Every public operation is a view or transition in one loop: bootstrap, orient, focus, inspect,
formulate, propose, compare, commit, watch, verify/reconcile, learn, and handoff/resume. Subsystems
cannot invent a competing lifecycle.

### CD-26 — Questions are first-class proof obligations

Mission objectives raise questions; questions name evidence deficits, decision consequences,
terminal predicates, and stopping. Capture, reconstruction, semantics, archive verification, and
diagnostics compete through the expected value of resolving questions.

### CD-27 — Every result carries one Agent Turn Packet

Successes, progress records, and errors expose exact anchor/continuity, synchronized world,
epistemic, work, and system ledgers, attention, affordances, recommendations, uncertainty,
coverage, budget, references, and continuation.

### CD-28 — Context compression must explain itself

Token-budgeted context packs retain mandatory safety/continuity state and publish Pack DNA for
selection, redundancy, omissions, coverage, and marginal budget value.

### CD-29 — Planning exposes alternatives, not an opaque answer

Materially different safe candidates share one basis and appear as a deterministic Pareto frontier
with assumptions, witnesses, predicted effects, costs, risks, reversibility, and invalidators.

### CD-30 — Surprise is the unit of accretion

Every material prediction/observation divergence creates an episode-linked surprise. Learning
advances through replay, shadow, canary, monitoring, and rollback rather than direct memory-driven
policy mutation.

### CD-31 — Handoff preserves understanding, not authority

A sealed handoff capsule is sufficient for a fresh agent to resume the minimum safe next step. It
does not confer capabilities, leases, or device/archive authority.

### CD-32 — Self-description is qualification-backed

Operation manifests, schemas, help, robot docs, affordances, compatibility, and maturity must agree
with current registries and local qualification evidence. First-try discoverability is tested.

## 3. Deep-dive index

| Project | Detailed analysis | Most accretive transfer |
|---|---|---|
| `asupersync` | [`01_ASUPERSYNC.md`](research/deep-dives/01_ASUPERSYNC.md) | owned regions, `Cx`, cancellation/reconciliation, deterministic lab, ATP |
| `frankensqlite` | [`02_FRANKENSQLITE.md`](research/deep-dives/02_FRANKENSQLITE.md) | MVCC evidence, witnesses, SSI-style conflicts, safe merge, recovery |
| `frankenfs` | [`03_FRANKENFS.md`](research/deep-dives/03_FRANKENFS.md) | staged/visible/durable custody, root-last publication, repair, path caps |
| `frankensearch` | [`04_FRANKENSEARCH.md`](research/deep-dives/04_FRANKENSEARCH.md) | progressive hybrid retrieval, immutable generations, score/absence proofs |
| `franken_markdown` | [`05_FRANKEN_MARKDOWN.md`](research/deep-dives/05_FRANKEN_MARKDOWN.md) | exact spans, bounded clean-room parsing, deterministic sibling output |
| `frankengraphdb` | [`06_FRANKENGRAPHDB.md`](research/deep-dives/06_FRANKENGRAPHDB.md) | one version universe, Atlas/Strata, Loom, Ripple, branches, certificates |
| `franken_networkx` | [`07_FRANKEN_NETWORKX.md`](research/deep-dives/07_FRANKEN_NETWORKX.md) | graph semantics, broad deterministic algorithms, views, complexity witnesses |
| `dwarf_fortress_mcp` | [`08_DWARF_FORTRESS_MCP.md`](research/deep-dives/08_DWARF_FORTRESS_MCP.md) | semantic narrow waist, witnessed plans, obligations, fenced external effects |
| `fastmcp_rust` | [`09_FASTMCP_RUST.md`](research/deep-dives/09_FASTMCP_RUST.md) | thin replaceable transport, budgets, cancellation/task projection |
| `eidetic_engine_cli` | [`10_EIDETIC_ENGINE.md`](research/deep-dives/10_EIDETIC_ENGINE.md) | advisory memory, explainable context packs, outcome/decay |
| `doodlestein_self_releaser` | [`11_DOODLESTEIN_SELF_RELEASER.md`](research/deep-dives/11_DOODLESTEIN_SELF_RELEASER.md) | clean local qualification, sibling closure, resumable release receipts |

The generated source inventory records exact commits and high-signal files. The transfer matrix is
[`research/TRANSFER_MATRIX.md`](research/TRANSFER_MATRIX.md).

---

## 4. The twelve leapfrog bets

No individual technique makes FDGR extraordinary. The composition does.

### B1 — One Evidence Universe (`Chronicle`)

Original bytes, observations, constraints, effects, archive receipts, and policy transitions form
one immutable capsule stream. History, replication, branches, subscriptions, reconstruction, and
reports are consequences of the same stream.

### B2 — Geometry as a Versioned Constraint Fabric (`Parallax`)

Tracks, poses, calibration, scale, depth, surfaces, topology, and semantics remain typed constraints
with provenance and uncertainty. Solvers select and optimize a hypothesis generation; they do not
destroy alternative evidence.

### B3 — Spatial LSM-MVCC (`Atlas`)

Spatial state migrates from tiny inline records to sorted deltas, sealed scan-efficient runs, and
cold anchors. Hot regions update cheaply; cold geometry approaches dense-array scan and compression
efficiency. Branches share immutable blocks.

### B4 — Incremental Everything (`Ripple`)

The capsule stream incrementally maintains view/pose/scene graphs, spatial fusion, coverage,
semantic resolution, indexes, subscriptions, and attention. Retractions are first-class. Full
recompute is the oracle.

### B5 — Graph-Native Physical Cognition (`Lattice`)

View, track, pose, scale, surface, room, utility, visibility, semantic, change, provenance, archive,
and obligation graphs use deterministic FrankenNetworkX algorithms and certificate-bearing query
plans.

### B6 — Metric and Uncertainty Proof (`Surveyor`)

Units, coordinate frames, clock maps, calibration, scale witnesses, covariance/intervals,
detectability, and contradiction are first-class. Measurements and absence claims carry proof
levels rather than one confidence float.

### B7 — Active Perception (`Lantern`)

Coverage is an evidentiary field. FDGR computes what is unknown, why it matters, candidate
viewpoints, expected information gain, and safe/manual or assisted routes. It optimizes the next
observation, not merely the current mesh.

### B8 — Verified Self-Healing Custody (`Vault`)

Original and derived object graphs are content-addressed, root-published, ATP-movable, multi-donor,
RaptorQ-repairable, scrubbed, and restore-qualified across local, B2, R2, peers, and removable
media.

### B9 — Agent-Native Witnessed Operations (`Warden`)

A small semantic API provides observe/query/plan/commit/wait/cancel/explain/doctor. Device effects
are capability-scoped obligations with idempotency, fences, final preconditions, and observed
terminal proofs.

### B10 — Closed-World Pure-Rust Local Forge (`Foundry`)

The production trust domain owns its hot paths in safe Rust, uses exact admitted sibling sources,
and qualifies on the user's local fleet through Doodlestein. External tools remain replaceable
oracles with retirement gates.

---

### B11 — Question-Driven Agent Kernel (`HELM`)

The question/objective graph, four ledgers, context packs, affordances, recommendations, candidate
frontiers, obligations, and handoffs form one cognitive kernel. An agent spends reasoning on the
property and mission instead of reconstructing protocol state or subsystem relationships.

### B12 — Evidence-Gated Operational Flywheel (`FLYWHEEL`)

Episode capsules connect context, alternatives, predictions, effects, evidence, outcome, actual
cost, surprise, and regret. Carefully promoted lessons improve capture, context, scheduling, model
selection, and budgeting while hard invariants remain immutable.

## 5. The three planes and two external rings

```text
┌────────────────────────────────────────────────────────────────────────────┐
│ AGENT / HUMAN PRESENTATION                                                 │
│ MCP · CLI · TUI · viewer · reports · bounded queries · continuations      │
└────────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌────────────────────────────────────────────────────────────────────────────┐
│ AUTHORITATIVE EVIDENCE PLANE                                               │
│ original bytes · capsules · clocks · calibration · scale witnesses        │
│ anchors · plans · capabilities · leases/fences · effects · obligations     │
│ object roots · custody/repair/restore receipts · qualification references │
└────────────────────────────────────────────────────────────────────────────┘
            │ pinned immutable inputs                         ▲ evidence
            ▼                                                 │
┌──────────────────────────────────────────┐   ┌─────────────────────────────┐
│ RECONSTRUCTION / COGNITION PLANE         │   │ DEVICE / EFFECT PLANE       │
│ tracks · poses · depth · Atlas fusion    │   │ DJI/profile adapters        │
│ scene/utility/visibility graphs          │   │ exact media import          │
│ search · models · semantics · coverage   │   │ stream/telemetry/control    │
│ branches · active perception · exports   │   │ cloud/local effect adapters │
└──────────────────────────────────────────┘   └─────────────────────────────┘

External advisory ring: Eidetic campaign/project memory (one-way evidence pointers)
External qualification ring: Doodlestein local clean-source build/test/release authority
```

The cognition plane has no handle capable of dispatching a device effect. The effect plane cannot
define canonical geometry, semantics, identity, units, or completion. Memory is outside all three
planes. Release evidence names their exact code/data identities but cannot alter runtime truth.

---

## 6. One version universe

### 6.1 Observation anchor

```text
(property_lineage,
 capture_epoch,
 evidence_sequence,
 device_profile_epoch,
 clock_epoch,
 calibration_epoch,
 schema_epoch,
 geometry_policy_epoch,
 semantic_policy_epoch,
 model_registry_epoch,
 root_digest)
```

Every exact request reads one complete anchor. Temporal requests name ranges/branches. A derived
root names source anchors and additional producer identities.

### 6.2 Generation DAG

```text
EvidenceGeneration
├── DecodeGeneration
├── ClockGeneration
└── CalibrationGeneration
      ↓
TrackGeneration
      ↓
PoseGeneration ─────────────┐
      ↓                     │
DepthGeneration             │
      ↓                     │
SpatialGeneration ← ScaleGeneration
      ↓
TopologyGeneration
├── SemanticGeneration
├── CoverageGeneration
├── SearchGeneration
└── ExportGeneration
```

This is a dependency DAG, not a requirement that every generation be linear or singular. Branches
and competing hypotheses are normal. Each node has basis roots, producer identity, policy,
validation, completeness, and output digest.

### 6.3 Root publication

```text
reserve successor
→ materialize immutable children
→ validate format, bounds, provenance, numeric policy, and closure
→ check witnesses / epochs / leases / conflicts
→ publish root and sequence atomically
→ notify incremental consumers
```

Notifications are accelerators. A missed notification does not lose truth; consumers resume from
high-water marks.

---

## 7. Geometry as constraints and claims

### 7.1 Constraint classes

- temporal/clock correlation;
- camera intrinsic/distortion/rolling-shutter/stabilization;
- feature/line/plane observations;
- track membership and association alternatives;
- relative/absolute pose;
- IMU/telemetry priors where trustworthy;
- scale ratios and metric witnesses;
- depth/disparity observations;
- surface/occupancy/free-space evidence;
- structural priors and topology;
- semantic observations and relations;
- human survey/control points.

Every constraint has source, coordinate frames, units, uncertainty, producer, validity interval,
residual policy, and active/retracted disposition.

### 7.2 Claim ladder

```text
Proposal
→ Corroborated
→ ResolvedRelative
→ ResolvedMetricEstimated
→ ResolvedMetricWitnessed
→ Surveyed
```

Orthogonal states include `Contradicted`, `Stale`, `Occluded`, `CoverageInsufficient`,
`ModelIncompatible`, `Retracted`, and `Indeterminate`.

### 7.3 Numeric honesty

Solvers declare precision, deterministic reduction, robust loss, stopping rules, conditioning,
gauge fixing, and error representation. Covariance, intervals, conformal sets, residual
quantiles, and discrete ambiguity can coexist; no universal confidence scalar is invented.

---

## 8. Graph-native reconstruction and scene understanding

The detailed algorithm map is in the FrankenNetworkX deep dive. The most accretive compositions are:

- maximum-confidence spanning forests initialize pose components;
- cycle bases expose transform inconsistency efficiently;
- biconnectivity/articulation identifies fragile reconstruction bridges;
- bipartite matching and min-cost flow handle feature/object association;
- cliques/independent sets select mutually compatible loop closures;
- k-core/truss finds redundantly supported evidence;
- max-flow/min-cut and Gomory–Hu summarize segmentation/weak cuts;
- dominators reveal mandatory portals, utility components, and provenance nodes;
- shortest/k-shortest/minimax paths power route and evidence explanations;
- SCC/condensation/topological/critical-path algorithms structure obligations;
- PPR and factorized graph retrieval build compact explanation/context packs;
- graph isomorphism/edit methods support repeated motifs and change detection;
- spectral warnings detect weak connectivity/conditioning without becoming proof;
- submodular selection chooses diverse keyframes, views, context, and scrub samples.

Every result is anchored and certificate-bearing. Physical-world resolution is a separate evidence
step.

---

## 9. Pure-Rust media and model strategy

### 9.1 Media

FDGR preserves original camera bytes immediately, even before native decode support. The path is:

1. owned bounded parsers for containers/NAL structure and timestamps;
2. reference external process adapter for FFmpeg in qualification/bootstrap only;
3. native safe-Rust H.264/H.265 decode profiles required for production maturity;
4. optional hardware decode only through an admitted safe process/profile with bitstream and
   decoded-frame differential evidence;
5. exact decode recipe and color pipeline identity on every frame generation.

FFmpeg command strings are never agent-controlled. The external adapter receives typed manifests
and returns untrusted bounded artifacts.

### 9.2 Models

Open-weight models are data artifacts behind a worker protocol. Initial research may use Python/
PyTorch sidecars. The target production path ports admitted operators/checkpoints into owned
safe-Rust tensor/inference infrastructure (for example an admitted Franken tensor stack) with:

- exact tokenizer/preprocessing/vision transform;
- operator and numeric conformance;
- bounded memory and cancellation;
- scalar reference kernels and safe portable SIMD;
- model-space identity;
- differential fixtures against the source implementation;
- license/use-profile registry.

No model output is authoritative merely because native inference matches.

---


## 9A. The agent synthetic kernel

The deepest cross-project composition is not another component. It is a linked abstraction tower:

```text
mission/policy
→ objective graph
→ question and evidence-deficit graph
→ candidate plans and counterfactual branches
→ obligations/effects/progress/reconciliation
→ claims and physical scene graph
→ constraints and observation capsules
→ immutable objects and custody
```

Each turn projects this tower through synchronized world, epistemic, work, and system ledgers.
Frankensearch and graph algorithms select a proof-carrying context pack; FrankenGraphDB-style
branches preserve competing hypotheses; FrankenSQLite-style witnesses protect plans against stale
or negative reads; Asupersync owns all work and cancellation; FrankenFS/ATP protect publication and
movement; Eidetic indexes advisory episodes; Doodlestein qualifies the exact behavior locally.

This composition makes several new capabilities possible:

- one next-step ranking across capture, compute, verification, and operator action;
- shared observations that resolve several objective questions at once;
- deterministic Pareto candidate comparison rather than hidden scalar optimization;
- active-work continuity across context windows and agents;
- errors that preserve state and exact recovery rather than forcing restart;
- measurable decision quality per total control cost;
- accretion based on surprise and outcome evidence instead of transcript accumulation.

The agent packet remains a derived projection. It cannot publish truth or authority, and memory
cannot satisfy evidence.


### 9A.8 The Decision Frame is the cockpit object

The Agent Turn Packet prevents response-shape fragmentation, but it is not enough: the agent should
not mentally join objectives, questions, evidence debt, candidates, and obligations. FDGR therefore
adds one immutable Decision Frame per material decision. This imports FrankenGraphDB branch and
certificate discipline, FrankenSQLite witnessed validity, Frankensearch bounded selection, and
Dwarf Fortress MCP's control-loop semantics into one proof-carrying cockpit object.

### 9A.9 Attention is scheduled epistemic debt

Attention is not a sorted alert list. Each item is an unresolved question whose possible answers
can change a decision or expected loss. Class priority, hysteresis, acknowledgement, suppression,
and expiry prevent event/model frequency from owning the agent's context. Shared evidence
opportunities let one observation retire several debts.

### 9A.10 Spatial handles make the physical world agent-legible

Coordinates, frames, mesh IDs, and semantic objects share one frame-complete handle family with
semantic zoom and historical correspondence. This is the physical-world counterpart of stable
source spans in Franken Markdown and stable entity/version identity in FrankenGraphDB.

### 9A.11 The human pilot is part of the loop, not an opaque actuator

Pilot cards preserve separate facts for recommendation, acknowledgement, observed motion, acquired
evidence, and question closure. The interface spends the scarce resource of operator attention
explicitly and can learn from refusal, inability, abort, and poor evidence gain without treating
them as generic failures.

## 10. Performance doctrine

FDGR targets world-class performance through semantics and layout:

- do not decode/process frames that add no information;
- submodular keyframe and next-view selection;
- incremental updates rather than global reruns;
- sparse/factorized computation rather than materialized Cartesian products;
- Atlas temperature tiers and multiresolution blocks;
- SoA arrays, compact typed handles, arenas, cache-line-aware shards;
- portable SIMD in safe Rust;
- deterministic parallel partitions and reduction trees;
- local Schur/block sparsity and graph-guided elimination;
- zero-copy immutable views;
- content-addressed deduplication;
- progressive fast/refined products;
- pressure-aware QoS lanes;
- ATP multi-path/multi-donor transfer and repair.

No benchmark mode may skip durability, validation, provenance, or semantic work unless it is labeled
and excluded from product comparisons. Same-binary A/A and A/B receipts establish equivalence
before timing.

---

## 11. Target crate graph

The target decomposition is intentionally more granular than the initial three-crate scaffold. A
crate is split only for a dependency, trust, format, or verification boundary.

```text
Foundation
  fdgr-types
  fdgr-error
  fdgr-digest
  fdgr-canonical-codec
  fdgr-numeric
  fdgr-geometry-primitives

Evidence and custody
  fdgr-object
  fdgr-custody
  fdgr-ledger
  fdgr-capsule
  fdgr-branch
  fdgr-transfer

Device and media
  fdgr-device-profile
  fdgr-dji-lab
  fdgr-dji-adapter
  fdgr-media-container
  fdgr-bitstream
  fdgr-decode
  fdgr-telemetry
  fdgr-clock
  fdgr-calibration

Reconstruction
  fdgr-keyframe
  fdgr-feature
  fdgr-track
  fdgr-pose
  fdgr-scale
  fdgr-depth
  fdgr-atlas
  fdgr-fusion
  fdgr-mesh
  fdgr-topology
  fdgr-change

Cognition
  fdgr-scene-graph
  fdgr-graph-algorithms
  fdgr-delta
  fdgr-index
  fdgr-search
  fdgr-semantics
  fdgr-coverage
  fdgr-objectives
  fdgr-context
  fdgr-planner
  fdgr-agent
  fdgr-explain

Interfaces and qualification
  fdgr-service
  fdgr-mcp
  fdgr-cli
  fdgr-viewer-wasm
  fdgr-lab
  fdgr-harness
  fdgr-release
```

The dependency graph is a strict DAG. `fdgr-dji-adapter` cannot depend on semantics or MCP.
`fdgr-semantics` cannot obtain a device command capability. `fdgr-mcp` is presentation-only.

---

## 12. Correct integration order

1. Freeze canonical identities, encodings, anchors, outcome lattice, publication state machine,
   agent abstraction tower, question graph, four ledgers, packet, profiles, and authority rules.
2. Build deterministic in-memory/file evidence and custody oracles plus an in-memory agent packet
   and cold-arrival scenario oracle.
3. Build synthetic camera/device/property generators with exact ground truth.
4. Introduce asupersync regions, contexts, budgets, and laboratory execution.
5. Implement exact media import and byte-preserving live spool before ambitious decode/control.
6. Implement clock/calibration/track/pose reference constraints and branches.
7. Implement graph views and reference algorithm certificates.
8. Implement Atlas reference spatial blocks and root publication.
9. Add full-recompute depth/fusion/topology, then incremental equivalents.
10. Add progressive search, context packs/Pack DNA, and semantic proposal/resolution.
11. Add objective/question-driven coverage and next-best-view planning; manual guidance first.
12. Qualify passive DJI live ingest and telemetry profile by profile.
13. Add one reversible camera/gimbal effect family with full obligation semantics.
14. Admit FrankenSQLite/FS/Search/Graph/NetworkX adapters one at a time behind reference semantics.
15. Add candidate frontiers, semantic watch/reconcile, handoff/resume, and multi-agent branches.
16. Add episode/surprise capsules and shadow/canary accretion before adaptive production policy.
17. Add native codec and inference paths; retain external differential oracles.
18. Add ATP/RaptorQ multi-provider custody and restore campaigns.
19. Add assisted flight only after simulator, physical, capability, and human-preemption gates.
20. Optimize only after local workload and total-control-cost receipts identify a bottleneck.
21. Release only through clean Doodlestein source-closure and agent-scenario qualification.

---

## 13. Explicit rejections

- a monolithic `process_video()` pipeline;
- one mutable point cloud/mesh as truth;
- frame/model/mesh directories with no shared version universe;
- model confidence used as metric or semantic proof;
- negative top-k interpreted as physical absence;
- raw DJI command or arbitrary packet injection tools;
- transport ACK treated as completed effect;
- blind retry after unknown command/upload outcome;
- separate Tokio/model/HTTP runtimes;
- in-process Python or C/C++ computer-vision/database/codec dependencies;
- permanent FFmpeg/COLMAP/Ceres/OpenCV foundations;
- graph storage without deterministic graph algorithms and provenance;
- last-writer-wins geometry/semantics;
- approximate index result called complete without a certificate;
- adaptive policy allowed to weaken safety/custody/evidence;
- deep graph copies for every query;
- branch artifacts copied directly into trunk;
- release claims based on source presence, badges, or hosted runner status;
- performance numbers without same-binary semantic receipts;
- memory or model output allowed to grant authority;
- one giant state dump that forces the agent to discover salience itself;
- component-specific progress protocols or hidden background work;
- recommendations without evidence, alternatives, cost, risk, invalidators, and stopping;
- token savings that hide continuity, counterevidence, active work, or uncertainty;
- a context pack with no explanation of inclusion and omission;
- a handoff that depends on replaying the transcript;
- policy adaptation from one episode, user rating, or unverified model rationale;
- autonomous activity merely to appear helpful when waiting or stopping dominates.

---

## 14. Admission philosophy

A feature is not implemented because code exists. It is implemented only when:

1. stable semantics, identities, failures, limits, and migration exist;
2. a deterministic reference implementation exists;
3. positive and negative evidence are retained separately;
4. differential, metamorphic, adversarial, cancellation, and crash tests pass;
5. capability and non-bypassability checks pass;
6. local native compatibility evidence exists where relevant;
7. performance gates pass without changing semantic digests;
8. documentation/registries/schemas/code agree;
9. a clean source-closure receipt can reproduce the result.

That is the standard by which the rest of the comprehensive plan must be interpreted.

### 9A.12 One vocabulary across the tower

The combined Franken imports would still be agent-hostile if storage, graph, MCP, model-worker,
archive, and cockpit surfaces renamed the same facts. FDGR therefore freezes one registry-derived
lower `snake_case` vocabulary and `fdgr.<name>/1` payload identity convention. Compatibility
parsing is a bounded ingress adapter, never a second emitted dialect. This turns names themselves
into stable semantic handles and eliminates translation state from agent context.
