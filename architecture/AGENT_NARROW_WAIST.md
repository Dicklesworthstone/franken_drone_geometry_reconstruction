# Agent Narrow Waist

FDGR exposes one coherent semantic protocol. Domain-specific commands and UIs compile into this waist; no subsystem is allowed to create a competing lifecycle.

## 1. Operations

| Operation | Purpose | Typical phase |
|---|---|---|
| `fdgr.open_session` | Negotiate lineage, protocol, grants, profiles, budgets, and continuity basis. | bootstrap |
| `fdgr.orient` | Return the smallest sufficient four-ledger briefing and ranked attention. | orient |
| `fdgr.query` | Answer a bounded question or expand typed handles with coverage and provenance. | focus/inspect |
| `fdgr.propose` | Compile an objective or next-step template into a sealed candidate set. | formulate/propose |
| `fdgr.compare` | Compare candidates or branches on one basis and return a Pareto frontier. | compare |
| `fdgr.commit` | Revalidate witnesses, authority, fences, policy, and budget; create owned obligations. | commit |
| `fdgr.watch` | Return semantic progress transitions, not repetitive polling dumps. | watch/verify |
| `fdgr.cancel` | Request cancellation and report drain, compensation, reconciliation, and terminal state. | reconcile |
| `fdgr.explain` | Traverse decisions, claims, scores, witnesses, omissions, and evidence. | any |
| `fdgr.handoff` | Create or resume a compact, sealed, authority-free continuity capsule. | handoff/resume |
| `fdgr.doctor` | Diagnose compatibility, custody, compute, model, policy, and qualification posture. | any |

The logical names remain stable across CLI, MCP, NDJSON, TUI, and future web surfaces.

## 2. One response spine

Every success, progress event, and error includes an `agent_turn` packet. Tool-specific data is additive. The spine always preserves:

```text
identity + phase + semantic status + stable error/recovery + exact anchor vector + continuity
focus + world/epistemic/work/system ledgers
changes + attention + affordances + recommendations
uncertainty + coverage + budget + references + continuation
```

An error cannot erase state the agent still needs. A heartbeat cannot restate the whole twin. Budget exhaustion cannot drop continuity or safety findings.

## 3. Domain verbs are compiler sugar

Commands such as:

```text
fdgr ingest ...
fdgr reconstruct ...
fdgr coverage ...
fdgr semantic ...
fdgr archive ...
fdgr export ...
```

compile to typed `query`, `propose`, `commit`, and `watch` transitions. Their help text and schemas are generated from the same operation, affordance, effect, and error registries. A flag cannot unlock behavior unavailable through the semantic waist.

## 4. Affordances, not command guessing

The orientation packet advertises currently expressible action templates. Each affordance names:

- family and parameter schema;
- current enabled/degraded/blocked status;
- grants and compatibility requirements;
- known and unresolved preconditions;
- risk, confirmation, checkpoint, and reversibility class;
- estimated control-cost vector;
- evidence or objective impact;
- a structured `fdgr.propose` template.

The agent therefore discovers what can be done from state, not documentation folklore.

## 5. Status and recovery

Semantic status values are:

```text
complete
partial
accepted
blocked
failed
cancelled
indeterminate
```

Every non-complete response carries a recovery class:

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

A blind retry is never recommended after an indeterminate effect.

## 6. Continuations

Continuations bind operation, normalized arguments, sort and tie-break policy, anchor vector, registry roots, privacy scope, and cursor. They are sealed and opaque. A stale or incompatible continuation fails with exact recovery guidance instead of silently restarting against a different world.

## 7. Protocol state machine

```text
open_session
  → orient
  → query* / explain*
  → propose
  → compare*
  → commit
  → watch*
  → complete | failed | cancelled | indeterminate
                     ↘ reconcile through query/watch/cancel ↗
  → handoff
```

`query`, `explain`, and `orient` are authority-free reads. `propose` is authority-free planning. Only `commit` can create an effect ticket, and only after the authoritative plane validates the sealed plan.

## 8. Self-description

The protocol exposes machine-readable:

- capability manifest;
- operation manifest;
- public schema registry;
- enum and status dictionary;
- error and recovery registry;
- profile and field manifest;
- compatibility matrix;
- maturity/readiness matrix;
- example requests and minimal safe workflows.

Human help, robot docs, MCP schemas, and CLI parsing are generated or parity-tested against these roots.

## 9. Deterministic presentation

Given the same anchor, profile, focus, grants, policy, budget, and seed, the response is stable in:

- field order;
- item order;
- attention and recommendation ranking;
- candidate and Pareto-frontier order;
- omission priority;
- continuation boundary;
- explanation and repair path.

This makes agent caching, replay, comparison, and regression testing reliable.

## 10. Learning is an intent, not a competing lifecycle

Terminal obligations automatically seal episodes and surprises. Optional agent feedback is a
registered `feedback.record` intent compiled through `fdgr.propose → fdgr.commit`, producing an
immutable advisory receipt. This preserves one public waist while still allowing agents to report
helpful, harmful, redundant, missing, misleading, or uncertain context with exact provenance.
