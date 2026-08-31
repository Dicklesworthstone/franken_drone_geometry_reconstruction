# The FDGR Decision Frame

The Decision Frame is the singular, anchor-bound object an agent uses whenever it must decide
whether to inspect, compute, capture, wait, reconcile, or commit. It prevents the agent from having
to join a mission record, question list, scene graph, candidate-plan response, budget report, and
active-work table in its own context window.

## 1. Constitutional rule

Every materially decision-bearing response contains exactly one Decision Frame. A heartbeat may
carry `null` when no decision boundary is active. Two apparently separate recommendations that
cannot be represented inside one frame are either two different decisions or evidence that the
abstraction boundary is wrong.

The frame is derived and authority-free. It cannot dispatch effects. It is a compact proof-carrying
projection over canonical state.

## 2. Contents

```text
DecisionFrame
  identity and compatible anchor vector
  active mission and objective slice
  focal questions and terminal predicates
  admitted facts and epistemic classes
  epistemic debt and decision-changing deficits
  hard constraints and forbidden states
  available affordances and current blockers
  candidate Pareto frontier and rejected alternatives
  active obligations that constrain the decision
  cost/risk/privacy/battery/operator envelopes
  recommended next protocol steps
  stopping rule and conditions for doing nothing
  invalidators, expiry, continuation, and decision digest
```

Every item is a typed handle into the same abstraction tower. Expansion never creates a parallel
identity.

## 3. Lifecycle

A frame is immutable. New observations or policy changes produce a successor frame with an explicit
delta:

```text
open → inspectable → candidate_ready → commit_ready
     → waiting → verified | blocked | stale | contradicted | indeterminate | retired
```

A frame becomes stale if any material witness, grant, lease, privacy scope, device profile, policy
epoch, or anchor component changes. Rebase recompiles the frame; it does not edit the old one.

## 4. Decision boundary

The frame states which fact could change the preferred action. This is the operational definition
of relevance. Evidence that cannot change the candidate frontier, risk class, proof requirement,
stop condition, or objective ordering is normally excluded from the default context pack.

## 5. Pareto frontier

A single opaque score is forbidden for materially different alternatives. The frame exposes the
non-dominated candidates over registered dimensions such as:

- expected question closure and objective progress;
- geometric or semantic proof improvement;
- information gain and robustness to uncertainty;
- operator attention and flight burden;
- CPU/GPU/storage/network cost;
- battery, physical, privacy, and recovery risk;
- reversibility and invalidation breadth.

The policy may recommend one candidate, but the frontier, dominated alternatives of interest, and
tie-break path remain inspectable.

## 6. “Do nothing” is a candidate

Waiting, accepting current uncertainty, deferring until a better source exists, or declaring that
the objective is not worth further cost are first-class candidates. The system does not fabricate
activity merely to appear agentic.

## 7. Use across interfaces

CLI, MCP, NDJSON, TUI, and future viewers expose the same frame identity and schema. Human-facing
cockpit cards are projections of the frame, not separately computed recommendations.
