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
into these operations.

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

## Compatibility

Semantic protocol negotiation is independent of MCP transport. Unknown enum variants never map to
a dangerous default. Breaking durable semantics require a new schema/codec epoch, migration plan,
and local qualification receipt.

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
