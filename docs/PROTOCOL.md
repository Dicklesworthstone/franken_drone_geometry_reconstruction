# FDGR Agent Protocol Contracts

The public protocol is one coherent semantic waist. JSON Schemas in `schemas/`, operation/profile
registries, and `architecture/agent_turn_contract.json` are the machine roots.

## Operating loop

```text
bootstrap → orient → focus → inspect → formulate → propose → compare
          → commit → watch → verify/reconcile → learn → handoff/resume
```

No command family or transport defines a competing state machine.

## Operations

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

CLI verbs such as `ingest`, `reconstruct`, `coverage`, `semantic`, `archive`, and `export` compile
into these operations. Current geometry subsystem commands are reference adapters used to qualify
semantic generations; they do not add a second authority plane.

## Agent Turn Packet

Every response, progress event, and error contains:

```text
schema, operation, phase, status, error, recovery
session_id, turn_id, request_id
anchor, continuity, profile, focus, decision_frame
world/epistemic/work/system ledgers
changes, attention, affordances, recommendations
uncertainty, coverage, budget, references, continuation
```

`anchor` always conforms to `schemas/anchor_vector.schema.json`; the same shape is reused by decisions, plans, obligations, episodes, handoffs, pilot cards, and spatial handles.

`turn_id` identifies presentation. `request_id` identifies an authority-bearing semantic request
when one exists; the two are never silently aliased.

## Status

```text
complete       response contract fully satisfied
partial        useful bounded result with named omissions
accepted       durable obligation created; terminal result pending
blocked        evidence/capability/license/privacy/compatibility gate not met
failed         terminal typed failure; effect outcome known
cancelled      cancellation drained and terminal cancellation predicate proved
indeterminate  external effect or continuity cannot yet be classified
```

## Recovery

Every packet carries nullable `error` and `recovery` fields. `blocked`, `failed`, `cancelled`, and
`indeterminate` packets require a recovery object naming one class:

```text
do_not_retry_unchanged
safe_read_retry
refresh_and_retry
rebase_required
backoff
reconciliation_required
confirmation_required
operator_action_required
```

Errors preserve the last usable anchor, active work, possible effect outcome, exact safe repair,
and information needed to resolve the problem.

## Epistemic state

```text
observed · certified_derived · inferred · predicted · assumed
stale · unknown · contradicted · indeterminate
```

Confidence never substitutes for class. Mutation preconditions can use only observed and eligible
certified-derived facts.

## Reference generation discipline

Current diagnostic geometry commands emit deterministic generation objects rather than Agent Turn
Packets. Every such object still follows the same evidence rules:

- exact schema identity;
- exact immutable upstream basis digests;
- canonical input ordering;
- typed authority ceiling;
- retained rejection or diagnostic evidence;
- deterministic replay and semantic digest;
- bounded execution evidence separate from semantic identity;
- no ambient local path in machine output.

A later Agent Turn Packet may reference these generations, but it cannot strengthen them. A
`reference_source` capability is not production admission.

## Bundle preparation protocol

The optimizer-entry protocol is deliberately two-stage:

```text
bundle-problem-build
  → fdgr.bundle_problem/1
    → bundle-admission-audit
      → fdgr.bundle_admission/1
        → future bounded optimizer
```

### Structural bundle problem

`bundle-problem-build` authenticates exact raw tables for:

```text
camera bindings
landmark seed proposals
optimize and candidate-held-out observations
```

It reconstructs the complete pose-refinement basis through the shared pose/scale/global pipeline,
then derives canonical semantic table identities and compiles a fixed-point support core plus a
bipartite topology certificate.

A structural component decision has only these meanings:

```text
block             structural or upstream evidence is unusable
admit_diagnostic  topology may support diagnostic computation only
admit             topology is ready for the stronger bundle-admission audit
```

Structural `admit` is not permission to optimize. `fdgr.bundle_problem/1` does not contain exact
image dimensions or observation-level seed provenance.

### Bundle-admission audit

`bundle-admission-audit` consumes the exact structural digest and two additional authenticated raw
tables:

```text
camera-domains.tsv
  camera_node_id
  frame_digest
  effective_calibration_digest
  image_width
  image_height

seed-provenance.tsv
  landmark_id
  comma-separated optimize observation IDs, or none
```

The audit verifies:

- exactly one domain per structural camera;
- exact frame and effective-calibration identity equality;
- half-open top-left image bounds in nano-pixels;
- exactly one provenance row per structural landmark;
- every seed-support ID exists, observes the same landmark, and has immutable role `optimize`;
- no held-out observation influenced seed initialization;
- enough seed-support observations and cameras survived in the final optimize core;
- held-out evidence is counted only from cameras still active in that core when policy requires it.

Audit component decisions mean:

```text
block             upstream structure or active image-domain evidence is invalid
admit_diagnostic  seed provenance, independent held-out evidence, or upstream topology is incomplete
admit             the exact relative problem may enter bounded optimization evaluation
```

A positive audit grants only:

```text
audited_relative_bundle_problem
```

It grants no calibration-accuracy, numerical-rank, reprojection-improvement, optimized-pose,
optimized-landmark, metric, or sparse-geometry authority.

### Exact-byte inputs

Both commands use the same shared structural parser and reconstruction seam. Every raw file is
verified against its supplied SHA-256 identity before parsing. Mutation after digest computation,
symlinks, malformed headers, malformed rows, unknown enum values, oversized files, and resource
ceiling exhaustion fail closed without partial semantic output.

### Execution ceilings

Operation and graph-path ceilings are retained as cost evidence but excluded from successful
semantic identity. Two successful runs with different ceilings must emit the same generation digest.
A ceiling that interrupts completion produces a typed refusal, not a partial generation.

## Context profiles

- `pulse`: compact heartbeat and critical transitions.
- `briefing`: cold/resume orientation.
- `tactical`: one decision, region, asset, or question.
- `pilot`: immediate manual-capture guidance.
- `forensic`: bounded evidence-complete audit or reconciliation.
- `custom`: explicit registered projection set under hard bounds.

## Progress

Progress is semantic, not a fabricated percentage. It names obligation state, processed high-water,
total when known, active children, potential, resources consumed, blockers, evidence produced,
question/objective transitions, and next heartbeat. A transport task is a projection of an FDGR
obligation, not its owner.

## Continuations

A continuation seals normalized operation, focus, ordering/tie-break, anchor vector, registry roots,
privacy/grants, budget/profile, and cursor. It cannot silently cross a gap, reset, policy epoch, or
incompatible generation.

## Recommendations and affordances

Recommendations are structured next protocol steps with evidence, utility, value of information,
cost vector, risk, reversibility, prerequisites, invalidators, confirmation, and exact template.
They remain authority-free. “Wait,” “stop,” “reconcile,” “ask the operator,” and “do nothing” are
valid.

Bundle recommendations are likewise authority-free. For example,
`proceed_to_bounded_bundle_optimization` means only that all currently registered admission gates
passed for one exact audit generation. It does not authorize publication or imply that an optimizer
will improve the scene model.

## Compatibility

Semantic protocol negotiation is independent of MCP transport. Unknown enum variants never map to
a dangerous default. Breaking durable semantics require a new schema/codec epoch, migration plan,
and local qualification receipt.

The image-domain and seed-provenance checks were introduced as a new `fdgr.bundle_admission/1`
generation instead of changing the meaning of existing `fdgr.bundle_problem/1` digests. This is the
required pattern whenever stronger evidence cannot be added without changing durable semantics.

## Decision Frame and physical handles

Decision-bearing results include `decision_frame` conforming to
[`schemas/decision_frame.schema.json`](../schemas/decision_frame.schema.json); non-decision
heartbeats use `null`. Attention items, pilot cards, and spatial handles conform to their registered
schemas. A client must not infer aircraft execution from a pilot instruction or interpret a spatial
tuple without its declared frame and scale authority.

## Canonical machine vocabulary

All public fields and enum values use lower `snake_case`; payloads identify their schema as
`fdgr.<name>/1`. CLI JSON, NDJSON, MCP, receipts, manifests, model-worker messages, and examples
share registry-derived names. Compatibility aliases are ingress-only and never emitted.
