# Agent Accretion and Learning Architecture

FDGR should become easier and more effective to operate over time without allowing memories, anecdotes, or adaptive policies to become hidden authority.

## 1. Unit of learning: the episode capsule

Every meaningful closed-loop attempt publishes an immutable episode capsule containing:

```text
mission/objective/question basis
agent and policy identities
starting anchor and context-pack digest
candidate set and decision card
selected plan and witnesses
predicted effects, costs, uncertainty, and completion time
committed effects and obligation transitions
observed result and terminal proof
actual cost ledger
surprises, contradictions, and operator interventions
regret/counterfactual evaluation when available
reusable lesson candidates
```

An episode is evidence, not a policy update.

## 2. Surprise records

A surprise is emitted whenever observed reality materially diverges from a recorded prediction, including:

- missing or unexpected geometry change;
- loop closure rejected after deeper evidence;
- semantic hypothesis overturned;
- capture maneuver produces less information than predicted;
- runtime, memory, storage, or transfer cost exceeds its interval;
- an effect remains indeterminate longer than expected;
- a recommendation would have ranked differently with newly available evidence.

Surprise makes epistemic failure visible and provides a precise target for improvement.

## 3. Memory strata

- **Episodic:** immutable FDGR episode capsules.
- **Semantic:** repeatedly supported statements about device profiles, property-specific conditions, or algorithm behavior, with applicability bounds.
- **Procedural:** reusable objective decompositions, capture patterns, repair workflows, and diagnostic playbooks.
- **Policy:** ranking, budgeting, scheduling, and model-selection parameters with explicit epochs and rollback.
- **Negative:** failed hypotheses, dangerous shortcuts, incompatibilities, and disproven assumptions.

Eidetic Engine may index and curate these as external advisory memory. Canonical FDGR state never cites advisory memory as proof.

## 4. Promotion ladder

```text
episode evidence
→ lesson candidate
→ independent supporting episodes
→ contradiction and confound review
→ bounded applicability statement
→ deterministic offline replay
→ shadow recommendation evaluation
→ counterfactual/regret comparison
→ canary policy epoch
→ admitted and monitored policy
→ retain, revise, or roll back
```

No single successful episode promotes a production policy.

## 5. Policy safety envelope

Adaptive policy may choose:

- which eligible question to inspect next;
- candidate count and search depth;
- keyframe or model effort within declared bounds;
- cache and storage tier;
- observation cadence;
- transfer path and repair overhead;
- context-pack allocation among optional items.

It may not weaken scale, coverage, privacy, capability, freshness, publication, custody, or completion requirements.

## 6. Cost calibration

Predicted and actual cost vectors are retained by workload class:

```text
tokens
canonical reads
derived queries
graph operations
CPU/GPU time and peak memory
source/device bytes
storage and network bytes
operator attention and flight time
risk and recovery burden
```

Calibration intervals, not point estimates alone, inform future plans. Regime changes create new policy epochs rather than contaminating old evidence.

## 7. Causal caution

Outcome correlation does not prove that a recommendation caused success. Promotion distinguishes:

- randomized or naturally varied evidence;
- matched counterfactual comparisons;
- same-input same-binary experiments;
- observational associations;
- operator assertions.

The evidence class travels with the lesson.

## 8. Handoff as accretion

A handoff capsule is the immediate operational memory needed by another agent. It carries exact anchors, active work, unresolved questions, rejected options, budget/grant posture, and typed evidence references. It contains no unverifiable narrative claim and grants no authority.

## 9. Agent feedback

An agent can report that a pack item, recommendation, explanation, or affordance was helpful, harmful, redundant, missing, or misleading. Feedback is linked to the resulting episode and evaluated against observed outcomes. It cannot directly mutate production ranking.

## 10. Success criterion

The system is agent-accretive when later agents require fewer tokens, fewer redundant observations, fewer retries, and less operator effort while maintaining or improving proof quality, calibration, and recovery behavior. Mere growth in stored memory is not accretion.

## 11. Feedback through the same narrow waist

Agent feedback does not justify a twelfth top-level lifecycle. `fdgr.orient` advertises a
`feedback.record` affordance when a pack item, recommendation, explanation, pilot card, or plan can
be evaluated. The agent submits a typed feedback intent through `fdgr.propose`; `fdgr.commit`
publishes an immutable advisory feedback receipt linked to the turn, item, episode, reason, and
observed outcome. Feedback may be `helpful`, `harmful`, `redundant`, `missing`, `misleading`, or
`uncertain`. It cannot rewrite the underlying episode or directly alter ranking. Promotion still
follows independent evidence, replay, shadow, canary, monitoring, and rollback.
