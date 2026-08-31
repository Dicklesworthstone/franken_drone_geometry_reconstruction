# FDGR Architecture

This is the compact engineering map. The comprehensive plan, semantics manifest, Agent Operating
Model, ADRs, and machine registries are normative when details differ.

## Thesis

FDGR is a safe-Rust, agent-native evidence and control substrate for reconstructing physical space
from owner-authorized drone video. Its center is not a media pipeline or a collection of geometry
components. Its center is a **question-driven closed loop** that helps an agent establish the best
possible world model at the lowest total control cost without confusing evidence, inference,
prediction, execution, or memory.

## One synthetic control loop

```text
bootstrap → orient → focus → inspect → formulate → propose → compare
          → commit → watch → verify/reconcile → learn → handoff/resume
```

Every public operation says where the agent is in this loop and what legal transition follows. No
subsystem exposes a competing lifecycle.

## Agent Turn Packet

Every success, progress event, and error converges on one packet:

```text
identity + phase + exact anchor vector + continuity
focus + world/epistemic/work/system ledgers
changes + ranked attention + affordances + recommendations
uncertainty + coverage + budget + references + continuation
```

The packet is an authority-free projection. It cannot dispatch effects or strengthen a fact.
Machine contract: [`architecture/agent_turn_contract.json`](architecture/agent_turn_contract.json).

## Abstraction tower

```text
L9  Campaign / mission / policy
L8  Objective graph
L7  Question graph, uncertainty, coverage, evidence deficits
L6  Candidate plans, counterfactuals, decision cards
L5  Obligations, effects, progress, verification, reconciliation, surprise
L4  Scene claims, assets, measurements, topology, coverage
L3  Constraint fabric: clocks, calibration, scale, tracks, poses, depth, factors
L2  Observation capsules and immutable history
L1  Content-addressed objects, custody, transfer, repair, restore
L0  External devices, processes, filesystems, providers, and operators
```

Typed handles and provenance connect every level. Higher levels compress lower levels; they never
silently strengthen epistemic status or authority.

## Question-driven cognition

```text
Mission → Objective → Question → Evidence deficit
        → Candidate observation/computation/effect
        → Candidate plan → Obligation → New evidence
        → Claim/abstention → Objective progress
        → Episode → Surprise → Lesson/policy evidence
```

A question states what evidence would resolve it, what objectives it affects, what remains missing,
and when further work should stop. Reconstruction, active perception, semantic resolution,
archive verification, and diagnostics all compete through expected decision value rather than
uncoordinated queues.

## Four synchronized ledgers

- **World:** established geometry, topology, assets, measurements, coverage, and change.
- **Epistemic:** open questions, assumptions, contradictions, staleness, uncertainty, detectability,
  and deficits.
- **Work:** candidates, prepared plans, obligations, confirmations, drains, transfers, blockers,
  and indeterminate effects.
- **System:** source/device compatibility, clock/calibration/scale, compute pressure, models,
  storage/archive, privacy, and qualification.

All four bind one compatible anchor vector. Mixed freshness is a protocol error.

## Three planes

```text
┌──────────────────────────────────────────────────────────────────────┐
│ AGENT SEMANTIC WAIST                                                 │
│ sessions · packets · objectives/questions · packs · plans · handoff │
└──────────────────────────────────────────────────────────────────────┘
                 │ reads / intents                   │ evidence
                 ▼                                   ▲
┌──────────────────────────────────────────────────────────────────────┐
│ AUTHORITATIVE EVIDENCE PLANE                                         │
│ capsules · MVCC roots · witnesses · claims · plans · obligations    │
│ publications · episodes · custody · qualification receipts           │
└──────────────────────────────────────────────────────────────────────┘
        │ pinned immutable generations            │ short-lived ticket
        ▼                                         ▼
┌──────────────────────────────────┐  ┌────────────────────────────────┐
│ COGNITION PLANE                  │  │ EFFECT PLANE                   │
│ constraints · poses · geometry   │  │ source/device adapters         │
│ scene/graph/search · coverage    │  │ media/model workers · archive  │
│ questions · packs · proposals    │  │ upload/lookup/reconcile        │
└──────────────────────────────────┘  └────────────────────────────────┘
```

Cognition has no effect-dispatch path. Effect adapters cannot redefine identity, coordinate,
scale, claim, or completion semantics.

## One version universe and anchor vector

The append unit is an immutable evidence capsule. The ordered stream drives original custody,
clocks, calibration, constraints, geometry, scene claims, coverage, questions, search/context,
branches, subscriptions, episodes, reports, archive state, and replay.

An anchor vector names:

```text
property lineage
capture epoch and evidence sequence/root
clock model generation
calibration generation
scale generation and authority
constraint/pose branch and generation
geometry generation
scene/ontology generation
coverage generation
search/context generation
archive/custody generation
schema, compatibility, privacy, and policy epochs
```

A request reads one compatible vector or explicitly reports partial, stale, gap, reset, or
indeterminate continuity.

## Publication

```text
reserve successor
→ materialize immutable children
→ verify bytes, schema, closure, basis, and terminal predicates
→ durably stage
→ atomically publish root/high-water
→ notify dependent projections
```

Readers see the prior or successor complete root, never an in-between generation.

## Epistemic separation

```text
observed
certified_derived
inferred
predicted
assumed
stale
unknown
contradicted
indeterminate
```

Confidence is orthogonal. Only observed and eligible certified-derived facts satisfy authoritative
preconditions. Model output remains a proposal. Absence requires complete authorized-domain
coverage and detectability.

## Planning, effects, and obligations

```text
objective/question
→ candidates on one anchor
→ witnesses, assumptions, costs, risk, reversibility, invalidators
→ deterministic Pareto frontier and decision card
→ commit-time capability/policy/idempotency/lease/fence revalidation
→ owned obligations and fenced effects
→ authoritative observation
→ terminal predicate proof | failure | cancellation | indeterminate
→ reconciliation and episode/surprise publication
```

A transport acknowledgment, drone response, child exit, upload receipt, or root reservation is not
completion.

## Context packs

Context packs are deterministic, focus-bound, privacy-bound, token-budgeted read models. Pack DNA
records mandatory items, marginal gain, redundancy, omissions, coverage, and continuation.
Profiles are `pulse`, `briefing`, `tactical`, `pilot`, `forensic`, and registered `custom`.

## Active perception

Manual-pilot guidance is generated from question bundles, visibility/coverage graphs, geometry and
semantic deficits, operator/device constraints, and value of information. Guidance proposed,
operator acknowledgment, observed maneuver, usable evidence, and question resolution are distinct.
Autonomous flight remains outside the initial system.

## Accretion

Every completed attempt yields an immutable episode capsule connecting context, alternatives,
selected plan, predicted and observed deltas, actual cost, evidence, surprises, and lesson
candidates. Policy changes require independent support, applicability bounds, deterministic replay,
shadow evaluation, canary epoch, monitoring, and rollback. Eidetic memory is advisory and cannot
grant authority or satisfy evidence.

## Owned execution

Asupersync exclusively owns sessions, watchers, device reads, media/model processes, geometry jobs,
branches, publications, transfers, archive work, obligations, pack builders, and episode writers.
Every effectful function receives `&Cx` authority, deadlines, multidimensional budgets,
cancellation, pressure, trace, and replay identity.

Cancellation is request → prevent new effects → drain → reconcile/compensate → seal progress and
evidence → finalize. Long drains expose progress potentials. Unknown external outcomes terminate as
`indeterminate`, not silently cancelled.

## Narrow waist

```text
fdgr.open_session
fdgr.orient
fdgr.query
fdgr.propose
fdgr.compare
fdgr.commit
fdgr.watch
fdgr.cancel
fdgr.explain
fdgr.handoff
fdgr.doctor
```

CLI domain verbs, MCP, NDJSON, TUI, and future UI surfaces compile into these operations. Only
`fdgr-mcp` may depend on FastMCP Rust; transport types stop at the presentation seam.

## Safe-Rust and dependencies

The universe is `core`/`alloc`/`std`, the pinned nightly, exact admitted Asupersync and Franken
siblings, and rare fundamental exceptions. FDGR crates use `unsafe_code = "forbid"`. No second
runtime, C/C++ FFI, in-process Python, generic external database/graph/search engine, or linked
media/model stack enters the core. External tools are supervised untrusted processes.

## Local qualification

Doodlestein executes clean, exact-source, native/self-hosted qualification lanes. Workflow YAML is
a portable job graph, not hosted release authority. Claims are matrix rows over exact platform,
device, model, provider, privacy, recovery, and performance profiles.

## Non-bypassability

An implementation is invalid if any path can mutate without a sealed plan and current authority;
read mixed generations; emit metric claims without scale; prove absence without coverage; hide
active/indeterminate work; dispatch from recommendation or memory; truncate safety under budget;
publish roots before children; treat acceptance as completion; learn policy from one episode; let a
stale agent publish after fencing; or claim readiness without local positive evidence.

## Shared cockpit invariants

The Agent Turn Packet is the common response envelope; the Decision Frame is the common decision
object. A material decision has one frame joining objective, focal questions, admitted facts,
epistemic debt, constraints, candidate frontier, active obligations, budget, stopping, and next
steps. Attention is a stable classed interrupt system over epistemic debt, not raw event frequency.
Every public spatial reference is frame-complete and historically resolvable. Human-pilot guidance
separates recommendation, acknowledgement, observed maneuver, usable evidence, and question
resolution.

These contracts are detailed in [`architecture/DECISION_FRAME.md`](architecture/DECISION_FRAME.md),
[`architecture/ATTENTION_AND_EPISTEMIC_DEBT.md`](architecture/ATTENTION_AND_EPISTEMIC_DEBT.md),
[`architecture/SPATIAL_SEMANTIC_HANDLES.md`](architecture/SPATIAL_SEMANTIC_HANDLES.md),
[`architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md`](architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md), and
[`architecture/AGENT_METRICS.md`](architecture/AGENT_METRICS.md).
