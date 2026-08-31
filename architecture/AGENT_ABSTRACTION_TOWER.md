# Agent Abstraction Tower

This document defines the single conceptual tower through which an agent understands and drives FDGR. It is normative for naming, protocol shape, crate boundaries, reports, context packs, explanations, and future user interfaces.

## 1. Why a tower is necessary

A subsystem inventory is not an operating model. An agent should not have to remember that pose uncertainty lives in one service, archive custody in another, semantic contradictions in a third, and capture advice in a fourth. Every lower-level mechanism must be reachable through a stable higher-level question:

```text
What am I trying to establish?
What is known now?
What prevents a defensible answer?
Which observation, computation, or effect has the greatest value?
What work is active, and what would prove it complete?
What did the last attempt teach the system?
```

The tower converts heterogeneous implementation details into a small set of linked abstractions. Every object names its parent, evidence basis, lifecycle, authority class, cost, and explanation handle.

## 2. The nine levels

```text
L9  Campaign / mission / policy
L8  Objective graph
L7  Question graph, uncertainty, coverage, and evidence deficits
L6  Candidate plans, counterfactual branches, and decision cards
L5  Obligations, effects, progress, verification, reconciliation, surprise
L4  Scene claims, assets, measurements, topology, and coverage certificates
L3  Constraint fabric: poses, tracks, depth, scale, calibration, clocks, factors
L2  Observation capsules and immutable history
L1  Content-addressed objects, manifests, custody, transfer, and repair
L0  External effects: device, filesystem, process, GPU, network, cloud, operator
```

No level is allowed to invent facts supplied by a lower level. Higher levels compress and organize; they do not silently strengthen epistemic status.

## 3. Level contracts

### L9 — Campaign / mission / policy

A campaign is the durable operating context for one property or reconstruction program. A mission states why the work exists and names policy epochs for privacy, cost, risk, retention, and quality. It is not an action and grants no capability.

Required fields include mission identity, target property lineage, success conditions, forbidden outcomes, policy roots, budget envelopes, and owner/delegation scope.

### L8 — Objective graph

An objective converts mission intent into evidence-testable terminal predicates. Objectives form a typed dependency and conflict graph. Examples:

- produce an exterior metric twin under the standard profile;
- resolve every exterior opening with stated confidence;
- determine whether an exterior propane tank is visible in the authorized domain;
- prove that the remote archive can restore the current release root;
- reduce unresolved roof geometry below a named threshold.

The planner may decompose an objective but cannot silently rewrite it. Every decomposition is digest-addressed and explainable.

### L7 — Question graph

A question is the agent's primary unit of uncertainty. It binds:

- a proposition or decision boundary;
- current answer state and epistemic class;
- supporting and contradicting evidence;
- coverage and detectability requirements;
- downstream objectives affected;
- candidate observations and computations;
- expected information value and cost;
- stopping or abstention rule.

The question graph is the bridge between active perception, reconstruction, semantics, and economical agent reasoning. `What should I do next?` is answered by ranking unresolved questions and the cheapest safe actions that can materially change them.

### L6 — Candidate plans and decision cards

A plan is a sealed proposal for obtaining evidence, performing computation, publishing a generation, moving custody, or requesting an operator action. Materially different safe alternatives are returned as a bounded Pareto frontier rather than collapsed behind an opaque scalar.

A decision card records common basis, assumptions, predicted question/objective impact, cost vector, risk, reversibility, witnesses, invalidators, tie-break policy, and why dominated alternatives were omitted.

### L5 — Obligations and verification

A committed plan becomes owned work. An obligation names the terminal predicate that will prove success, not merely the process that was started. Its lifecycle is:

```text
prepared → committed → dispatching → accepted? → effect-observed?
         → verifying → stable-complete | failed | cancelled | indeterminate
```

Unknown external outcomes enter reconciliation. Prediction-versus-observation divergence produces a surprise record rather than being buried in logs.

### L4 — Scene claims

This level contains agent-meaningful facts: geometry generations, rooms, surfaces, openings, equipment, utilities, vegetation, paths, measurements, relations, coverage, and explicit unknowns. Every claim carries evidence, scope, uncertainty, validity interval, and epistemic status.

### L3 — Constraint fabric

This is the mathematical evidence layer: observations, keypoints, tracks, correspondences, camera states, factors, residuals, calibration, clocks, scale witnesses, depth proposals, uncertainty, loop closures, and branch hypotheses. It is versioned and inspectable. No optimizer is the source of truth merely because it converged.

### L2 — Observation capsules

Capsules are immutable append units for acquired evidence and accepted semantic transitions. They establish sequence, basis, clock epoch, source identity, provenance, privacy scope, and publication lineage. Derived generations name their consumed high-water marks.

### L1 — Objects and custody

All durable state is an immutable object graph with canonical identity. Manifests publish roots last. Local custody, ATP transfer, cloud replication, repair symbols, retention, and restore operate on this graph without changing semantic identity.

### L0 — External effects

Devices, DJI software, filesystems, ffmpeg, model workers, GPU runtimes, network providers, and human operators are fenced effect domains. They may return receipts and observations; they cannot define FDGR truth or completion.

## 4. Vertical links

Every object exposes typed links upward and downward:

```text
mission → objectives → questions → plans → obligations
obligation → effects → observations → factors → claims
claim → evidence capsules → immutable objects → custody receipts
surprise → episode → lesson candidate → policy evidence
```

The agent can traverse downward for proof and upward for consequence without issuing a corpus-wide search.

## 5. The four ledgers

Every orientation packet projects the tower through four compact ledgers:

| Ledger | Agent question | Principal tower levels |
|---|---|---|
| **World** | What is known about the property? | L4–L2 |
| **Epistemic** | What is uncertain, contradicted, uncovered, or stale? | L7–L3 |
| **Work** | What is active, blocked, awaiting confirmation, or indeterminate? | L6–L5 |
| **System** | Is capture, compute, storage, archive, and policy machinery healthy and affordable? | L1–L0 plus policy |

These are synchronized projections over one anchor vector. They are not independent caches.

## 6. Stable handles

Every agent-visible item has a typed stable handle such as:

```text
mission:...
objective:...
question:...
plan:...
obligation:...
claim:...
asset:...
region:...
generation:...
evidence:...
object:...
episode:...
surprise:...
continuation:...
```

A handle can be summarized, explained, expanded, compared, watched, or cited. It never embeds executable text or ambient authority.

## 7. Non-bypassability

The tower is invalid if:

- a low-level process result can appear as an L4 fact without validation;
- a recommendation can dispatch an L0 effect without a sealed L6 plan and L5 commit;
- an L7 inference satisfies a mutation precondition as though observed;
- a context pack mixes anchors across levels;
- an upper level hides unresolved lower-level contradictions;
- a handoff omits active obligations or indeterminate effects;
- a policy update skips episode, surprise, shadow, and rollback evidence.

## 8. The agent-driver test

A fresh agent receiving one briefing must be able to identify:

1. the mission and current objective frontier;
2. the exact understood anchor and continuity status;
3. the strongest established world claims;
4. the highest-value unresolved questions;
5. all active and indeterminate work;
6. the legal affordances and best next protocol steps;
7. the expected cost and evidence gain of each step;
8. the minimum safe handoff state.

If it cannot, the tower has leaked subsystem complexity upward and the interface has failed.
