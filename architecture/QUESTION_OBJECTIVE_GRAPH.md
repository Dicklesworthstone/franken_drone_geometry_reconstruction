# Mission, Objective, Question, and Evidence-Deficit Graph

This graph is the cognitive center of FDGR. It connects user purpose to observations and compute without forcing an agent to understand subsystem boundaries.

## 1. Node families

```text
Mission
Objective
Question
DecisionBoundary
EvidenceDeficit
CandidateObservation
CandidateComputation
CandidatePlan
Obligation
Claim
CoverageDomain
AssetOrRegion
Episode
Surprise
LessonCandidate
PolicyCandidate
```

## 2. Core edges

```text
Mission CONTAINS Objective
Objective DEPENDS_ON Objective
Objective CONFLICTS_WITH Objective
Objective RAISES Question
Question AFFECTS Objective
Question REQUIRES EvidenceDeficit
EvidenceDeficit ABOUT AssetOrRegion
EvidenceDeficit COVERED_BY CoverageDomain
CandidateObservation REDUCES EvidenceDeficit
CandidateComputation REDUCES EvidenceDeficit
CandidatePlan CONTAINS observation/computation/effect steps
CandidatePlan PREDICTS Claim or objective delta
Obligation EXECUTES CandidatePlan
Obligation PRODUCES evidence or Claim
Claim ANSWERS Question
Claim SUPPORTS or CONTRADICTS Claim
Surprise COMPARES predicted and observed result
Episode CONTAINS plan, obligation, evidence, outcome, cost, surprise
LessonCandidate DERIVED_FROM Episode
PolicyCandidate EVALUATED_ON Episode set
```

Every edge is anchor- and policy-bound and has a deterministic identity.

## 3. Question states

A question can be:

```text
unopened
open
partially_answered
answered
contradicted
blocked
stale
indeterminate
retired
```

`answered` requires its registered terminal predicate, evidence class, coverage, and uncertainty threshold. A model caption alone cannot close a safety-relevant question.

## 4. Evidence deficits

An evidence deficit names what is missing, not merely that confidence is low. Examples:

- no oblique view of the north roof plane;
- scale is only estimated for the detached garage component;
- two loop-closure hypotheses remain observationally equivalent;
- possible electrical service equipment has only one low-resolution view;
- archive root has provider HEAD evidence but no independent readback;
- wall region is occluded, so absence of a spigot is uncertified.

Deficits can be reduced by capture, computation, human confirmation, waiting for an effect, or explicitly narrowing the claim scope.

## 5. Value of information

For each candidate step:

```text
VOI = probability of changing a material decision
    × reduction in expected decision loss
    + future-control reuse value
    + coverage/reliability value
    - observation/compute/token/storage/operator cost
    - delay and risk cost
```

VOI is a decision aid, not a safety authority. Hard proof requirements remain hard even when estimated VOI is negative.

## 6. Stopping rules

A campaign or objective may stop because:

- all required questions satisfy terminal evidence predicates;
- remaining uncertainty cannot alter an allowed decision;
- marginal evidence gain falls below the declared threshold;
- budget is exhausted and a partial result is explicitly accepted;
- a blocked or impossible condition is proved;
- the operator suspends or abandons the objective;
- safety, privacy, compatibility, or custody policy forbids further work.

Stopping is a positive, explainable decision. The system must not continue capturing or computing merely because resources remain.

## 7. Question bundles

Related questions are grouped by shared evidence. One flight maneuver or reconstruction job may resolve several deficits. Candidate generation therefore optimizes over a hypergraph of expected evidence gain rather than treating each question independently.

## 8. Branches and disagreement

Competing pose, scale, topology, or semantic hypotheses create branch-local answers. The question remains open at the parent until a registered merge, rejection, or scope split resolves the disagreement. Branch confidence is never averaged into false consensus.

## 9. Agent interface

`fdgr.orient` returns the objective and question frontier. `fdgr.query` expands a question. `fdgr.propose` compiles candidate steps for one or more deficits. `fdgr.compare` exposes the Pareto frontier. `fdgr.watch` reports question state changes caused by active work. `fdgr.explain` traverses all links.
