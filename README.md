# franken_drone_geometry_reconstruction (`fdgr`)

[![License: MIT+Rider](https://img.shields.io/badge/License-MIT%2BOpenAI%2FAnthropic%20Rider-blue.svg)](./LICENSE)

**An agent-native, evidence-grade operating substrate for turning owner-authorized drone video into metrically honest semantic digital twins.**

> **Current status — 2026-08-31:** normative architecture, project-by-project Franken deep dives,
> machine registries, JSON Schemas, and a dependency-free safe-Rust scaffold. The repository does
> **not** yet claim a live DJI stream adapter, production reconstruction, cloud archival, or
> semantic asset recognition. Every such claim is gated by retained positive evidence.

FDGR is designed for a person who manually flies a compact drone such as the DJI Flip around a
home while an agent helps decide what to observe, what to compute, what can be trusted, what
remains unknown, and what should happen next. The eventual system preserves original media,
reconstructs geometry, resolves visible property assets, measures what can honestly be measured,
identifies gaps, guides additional capture, archives evidence, and explains every consequential
claim.

The result is not “a mesh from a video.” It is a **multi-version evidence and constraint database
for physical reality**, operated through one coherent cognitive control loop.

## The system from the driver’s seat

A competent agent should never have to mentally join capture status, pose graphs, semantic model
outputs, cloud uploads, and background jobs. After every success, progress event, or error, FDGR
must let the agent answer:

1. What is currently known to be true?
2. What changed since the anchor I understood?
3. What matters now, and why?
4. What is uncertain, contradicted, stale, or insufficiently observed?
5. What work is active or indeterminate?
6. What can I legally and safely do from here?
7. Which next step has the highest expected value per unit of total control cost?
8. What would prove that step complete?
9. What did prior attempts teach, and how trustworthy is that lesson?

All agent work follows one loop:

```text
bootstrap → orient → focus → inspect → formulate → propose → compare
          → commit → watch → verify/reconcile → learn → handoff/resume
```

Every operation returns the same **Agent Turn Packet**: semantic status, nullable stable error and
typed recovery; exact anchor and continuity; profile and focus; one Decision Frame when a material
decision exists; synchronized world, epistemic, work, and system ledgers; changes; attention;
affordances; recommendations; uncertainty; coverage; budget; evidence references; and continuation.

See [`docs/AGENT_OPERATING_MODEL.md`](docs/AGENT_OPERATING_MODEL.md) and
[`architecture/agent_turn_contract.json`](architecture/agent_turn_contract.json).

## The abstraction tower

```text
L9  Campaign / mission / policy
L8  Objective graph
L7  Question graph, uncertainty, coverage, and evidence deficits
L6  Candidate plans, counterfactual branches, and decision cards
L5  Obligations, effects, progress, verification, reconciliation, surprise
L4  Scene claims, assets, measurements, topology, coverage certificates
L3  Constraint fabric: poses, tracks, depth, scale, calibration, clocks
L2  Observation capsules and immutable history
L1  Content-addressed objects, custody, transfer, repair, restore
L0  Fenced external effects: DJI, files, ffmpeg, models, GPU, cloud, operator
```

The tower makes every subsystem legible through the same questions. Higher levels compress and
organize lower-level evidence; they never silently strengthen it. An agent can traverse from a
mission down to raw evidence or from a surprising frame up to affected objectives through typed
handles.

## The twelve leapfrog bets

| Bet | Design choice |
|---|---|
| **B1 · One Evidence Universe** | Capture, clocks, calibration, geometry, semantics, archive state, branches, replay, questions, episodes, and reports derive from one ordered immutable capsule stream. |
| **B2 · Scale Is a Proof Obligation** | Monocular output remains relative until a registered witness establishes metric scale for a declared domain and uncertainty. |
| **B3 · Draft Live, Converge Offline** | Fast provisional live products and deeper original-media reconstruction are successor generations in one lineage, never silently mixed. |
| **B4 · Models Propose, Geometry Adjudicates** | **Model outputs are proposals**. Classical constraints, held-out observations, independent evidence, uncertainty, and registry gates decide admission. |
| **B5 · Semantics Are an Evidence Graph** | Objects move through observation, hypothesis, resolution, rejection, contradiction, and indeterminate states; captions do not become facts. |
| **B6 · Coverage Makes Ignorance Explicit** | The twin records what was seen, how well, from where, and with what detectability; absence requires a complete-domain witness. |
| **B7 · Content-Addressed Economic Custody** | Originals and all derived generations are immutable objects with deduplication, verified transfer, repair, retention, and provider-independent restore. |
| **B8 · Determinism Is a Product Feature** | Stable ordering, canonical codecs, exact profiles, decision certificates, lab schedules, and replay turn failures into reproducible evidence. |
| **B9 · Agent-Native Is Structural** | A single response spine, typed affordances, semantic progress, continuations, explanations, and a narrow waist minimize token and retry cost. |
| **B10 · Privacy Scope Travels with Evidence** | Home geometry, coordinates, images, critical assets, model access, archives, reports, and context packs inherit explicit privacy scope. |
| **B11 · Questions Are the Cognitive Control Plane** | Missions raise objectives; objectives raise questions; questions expose evidence deficits; plans are ranked by the value of resolving them. |
| **B12 · Accretion Must Be Evidence-Gated** | Episodes, surprises, actual costs, shadow evaluation, canary policy epochs, monitoring, and rollback let the system improve without memory becoming authority. |

## One question-driven graph

```text
Mission
  → Objective
    → Question
      → Evidence deficit
        → Candidate observation / computation / operator request
          → Candidate plan and counterfactual branch
            → Committed obligation
              → New evidence
                → Claim or explicit abstention
                  → Objective progress
                    → Episode, surprise, lesson candidate
```

This graph is the synthetic center. Reconstruction, active perception, semantic resolution,
archive verification, and diagnostics all compete in one vocabulary: **which unresolved question
can materially change a decision, at what cost and risk?**

## Three planes, one center

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ AGENT SEMANTIC WAIST                                                   │
│ sessions · Agent Turn Packet · objectives · questions · context packs  │
│ affordances · recommendations · candidates · handoff · explanations    │
└─────────────────────────────────────────────────────────────────────────┘
                    │ reads / intents                    │ evidence
                    ▼                                    ▲
┌─────────────────────────────────────────────────────────────────────────┐
│ AUTHORITATIVE EVIDENCE PLANE                                            │
│ capsules · anchor vector · claims · witnesses · plans · obligations     │
│ publication roots · episodes · custody and qualification receipts       │
└─────────────────────────────────────────────────────────────────────────┘
        │ pinned immutable generations                │ fenced ticket
        ▼                                             ▼
┌──────────────────────────────────┐  ┌───────────────────────────────────┐
│ RECONSTRUCTION / COGNITION       │  │ DEVICE / EFFECT                  │
│ constraints · poses · depth      │  │ source adapters · DJI lab        │
│ geometry · scene/graph/search    │  │ ffmpeg/model workers · archive   │
│ coverage · questions · proposals │  │ uploads · lookup · reconciliation│
└──────────────────────────────────┘  └───────────────────────────────────┘
```

Cognition can propose but cannot dispatch. Effects can produce receipts and observations but
cannot define identity, scale, geometry, semantics, or completion. The Agent Turn Packet is a
derived orientation spine, not a second source of truth.

## Four synchronized ledgers

| Ledger | What it tells the agent |
|---|---|
| **World** | Established property geometry, topology, assets, measurements, coverage, and change. |
| **Epistemic** | Open questions, assumptions, contradictions, uncertainty, detectability, staleness, and evidence deficits. |
| **Work** | Candidate plans, active obligations, confirmations, drains, checkpoints, transfers, blockers, and indeterminate effects. |
| **System** | Device/source compatibility, clock/calibration/scale health, compute pressure, model status, storage, archive, privacy, and qualification. |

They are projections of one canonical `anchor_vector` and `anchor_digest`, not independently fresh dashboards.

## Agent-facing narrow waist

The target logical protocol contains eleven operations:

```text
fdgr.open_session   negotiate lineage, grants, profiles, budgets, continuity
fdgr.orient         obtain the smallest sufficient four-ledger briefing
fdgr.query          answer a bounded question or expand typed handles
fdgr.propose        compile objectives or next steps into sealed candidates
fdgr.compare        compare candidates/branches and expose a Pareto frontier
fdgr.commit         revalidate and create owned obligations
fdgr.watch          receive semantic progress and terminal transitions
fdgr.cancel         request drain, compensation, and reconciliation
fdgr.explain        traverse decisions, witnesses, scores, omissions, evidence
fdgr.handoff        create or resume a sealed authority-free continuity capsule
fdgr.doctor         diagnose compatibility, custody, compute, and qualification
```

Human-friendly verbs such as `ingest`, `reconstruct`, `coverage`, `semantic`, `archive`, and
`export` compile into this same lifecycle. There is no giant tool per model, ffmpeg flag, DJI
packet, or database table.

## Context packs and Pack DNA

FDGR sends the smallest **decision-sufficient** context, not the smallest text. A context pack is
anchor-bound, focus-bound, privacy-bound, and token-budgeted. Mandatory safety and continuity
items cannot be displaced by optional relevance.

Every pack includes **Pack DNA** explaining:

- what was mandatory;
- what was selected and its marginal value;
- which items were redundant;
- which high-scoring items were omitted and why;
- what coverage was gained;
- what remains unresolved;
- what an additional token budget would buy.

Profiles are `pulse`, `briefing`, `tactical`, `pilot`, `forensic`, and explicit `custom`.

## Planning and execution

Consequential work follows:

```text
objective/question
→ sealed candidate set on one anchor
→ inspect assumptions, witnesses, costs, risk, reversibility, invalidators
→ deterministic Pareto comparison
→ commit-time witness/capability/policy/lease/fence validation
→ owned obligations
→ semantic progress
→ authoritative observation
→ terminal predicate proof or reconciliation
→ episode and surprise publication
```

Transport acknowledgment, subprocess exit, queue insertion, drone response, or cloud HTTP success
is never semantic completion. A recommendation may validly say **wait**, **stop**, **reconcile**,
**ask the operator**, or **do nothing**.
Optional helpful/harmful/missing/misleading feedback is a typed `feedback.record` intent through `fdgr.propose → fdgr.commit`, not a separate authority path.

## Active perception for a manual pilot

The `pilot` profile suggests a small set of semantic maneuvers. Each identifies the exact questions
and evidence deficits it should change, desired framing/baseline/resolution, live quality checks,
risk and privacy constraints, and good-enough/stop/abort rules. Guidance proposed, operator
acknowledged, maneuver observed, usable evidence acquired, and question resolved are distinct
states. Autonomous flight is outside the initial architecture.

## Geometry and semantic authority

FDGR does not crown one visual representation as “the twin.” Sparse cameras/tracks provide pose
evidence; depth/point maps provide per-view proposals; surfels/TSDF/occupancy provide fused
surface/free-space candidates; meshes provide topology and interchange; appearance models provide
rendering; and the scene graph carries assets, relations, claims, counterevidence, provenance, and
uncertainty.

A photorealistic Gaussian or NeRF output is never metric proof. A confident multimodal label is
never a resolved critical asset. Every measurement declares frame, units, scale authority, and
uncertainty.

## DJI integration without architectural contamination

The DJI Flip is the motivating profile, not the data model. FDGR admits sources through a ladder:

1. exact original-media import from microSD or an explicit export;
2. controller/phone recording and bounded display/HDMI/USB capture;
3. documented SDK, UVC, RTMP, RTSP, or vendor export for exact supported profiles;
4. owner-authorized, read-only-first protocol research against the operator’s own paired system.

Profiles include aircraft, controller, firmware, app, phone OS, region, radio mode, pairing/account
state, endpoint transcript, and capabilities. The project does not bypass pairing, authentication,
encryption, account controls, geofencing, or another operator’s equipment.

See [`DJI_ADAPTER_RESEARCH.md`](DJI_ADAPTER_RESEARCH.md).

## Safe-Rust and dependency doctrine

The production trust domain is strict Rust on the pinned latest nightly, with
`#![forbid(unsafe_code)]`, Asupersync as the only runtime, exact admitted Franken-suite revisions,
and only rare fundamental crates such as Serde after explicit review. No Tokio, Rayon, C/C++ FFI,
in-process Python, linked FFmpeg, OpenCV, COLMAP, Ceres, generic graph/database/search engine, or
unpinned Git dependency enters the core.

Unavoidable media, model, GPU, vendor, and research stacks run as supervised external processes
with sealed manifests, no-network defaults where possible, resource budgets, descendant cleanup,
and untrusted output validation.

See [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md).

## Local qualification is release authority

GitHub workflow YAML is a portable job graph for local/self-hosted execution, not a hosted trust
root. Doodlestein runs clean snapshots with exact sibling closure, pinned nightly/toolchain,
platform/device/model lanes, retained logs, resumable target artifacts, and sealed qualification
receipts. Partial builds may be retained but cannot be blessed as a complete release.

See [`LOCAL_QUALIFICATION_AND_RELEASE.md`](LOCAL_QUALIFICATION_AND_RELEASE.md).

## The shared agent cockpit

Every material decision is projected into one immutable **Decision Frame**. It joins the active
objective, focal questions, admitted facts, epistemic debt, constraints, available affordances,
candidate Pareto frontier, active obligations, budgets, stop conditions, and ranked next protocol
steps. CLI, MCP, NDJSON, TUI, and future viewers render the same frame rather than recomputing their
own advice.

Attention is a stable interrupt system over explicit epistemic debt. It uses safety-first classes,
hysteresis, acknowledgement, suppression, expiry, and material-change re-entry. Spatial and
semantic references use frame-complete handles with human landmarks and semantic zoom, so “inspect
the uncovered strip behind the maple tree” resolves to exact rays, surfaces, coverage, questions,
and evidence without leaking coordinate-system trivia.

During manual capture, the agent emits one bounded pilot card at a time. Recommendation, operator
acknowledgement, observed maneuver, usable evidence, and question resolution are separate facts.
The system stops when proof obligations are satisfied or marginal evidence gain is no longer worth
battery, attention, risk, and privacy cost—not when it has accumulated an arbitrary amount of
footage.

See [`architecture/DECISION_FRAME.md`](architecture/DECISION_FRAME.md),
[`architecture/ATTENTION_AND_EPISTEMIC_DEBT.md`](architecture/ATTENTION_AND_EPISTEMIC_DEBT.md),
[`architecture/SPATIAL_SEMANTIC_HANDLES.md`](architecture/SPATIAL_SEMANTIC_HANDLES.md), and
[`architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md`](architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md).

## Current executable scaffold

This repository does **not** yet claim a live DJI ingest, reconstruction, semantic-twin, archive, or agent-control implementation. The executable code is a deliberately small semantic scaffold.


Only truthful diagnostic/reference surfaces exist today:

```bash
cargo run -p fdgr-cli -- capabilities --format json
cargo run -p fdgr-cli -- doctor --format json
cargo run -p fdgr-cli -- plan-summary --format json
cargo run -p fdgr-cli -- validate-id \
  0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
```

The source tree is intentionally ahead of implementation. Read
[`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) before interpreting target-state prose as a
current feature claim.

## Documentation map

Start with:

1. [`docs/AGENT_OPERATING_MODEL.md`](docs/AGENT_OPERATING_MODEL.md)
2. [`architecture/AGENT_ABSTRACTION_TOWER.md`](architecture/AGENT_ABSTRACTION_TOWER.md)
3. [`architecture/AGENT_NARROW_WAIST.md`](architecture/AGENT_NARROW_WAIST.md)
4. [`architecture/QUESTION_OBJECTIVE_GRAPH.md`](architecture/QUESTION_OBJECTIVE_GRAPH.md)
5. [`architecture/CONTEXT_PACKS.md`](architecture/CONTEXT_PACKS.md)
6. [`COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md`](COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md)
7. [`FRANKENSTACK_DEEP_DIVE.md`](FRANKENSTACK_DEEP_DIVE.md)
8. [`DESIGN_INDEX.md`](DESIGN_INDEX.md)

## Constitutional refusal points

An implementation is invalid if any path can rewrite original evidence; mix generations; emit
metric claims without scale witnesses; infer absence without coverage; promote model output
straight to truth; treat acceptance as completion; dispatch from recommendation/memory/search;
hide active or indeterminate work; truncate away safety state; publish a root before children;
operate a device without owner authority; learn a production policy from one anecdote; or claim
readiness from source presence, negative tests, or hosted CI alone.

## License

MIT License (with OpenAI/Anthropic Rider). See [`LICENSE`](./LICENSE).

