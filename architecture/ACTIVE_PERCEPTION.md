# Active Perception and Pilot Guidance

FDGR treats manual capture as an evidence-acquisition problem over the objective/question graph, not as a generic “get more coverage” loop.

## 1. Inputs

A guidance decision binds one anchor vector and consumes:

- unresolved questions and evidence deficits;
- current camera/coverage/visibility graph;
- geometry and uncertainty fields;
- scale and calibration deficiencies;
- semantic detectability requirements;
- obstacle/no-fly/owner policy;
- operator and device capabilities;
- battery, time, bandwidth, and risk budgets;
- active capture obligations and duplicate-work state.

## 2. Candidate maneuver

A candidate is a semantic observation request, not low-level motor control:

```text
view target and desired framing
admissible position/orientation region
minimum baseline, view angle, resolution, exposure, and dwell
questions and deficits expected to change
predicted information gain interval
flight/operator/time/storage cost
risk, privacy, and accessibility constraints
invalidating conditions
stop/abort predicates
```

The initial architecture advises a human pilot. It does not send autonomous flight commands.

## 3. Objective

The reference selector maximizes expected reduction in decision loss and future control cost under hard safety and privacy constraints. It uses submodular set selection to account for overlap: one maneuver may resolve several questions, while near-duplicate views have diminishing value.

## 4. Pilot packet

The `pilot` profile returns:

- one primary maneuver and at most a small fallback set;
- a plain-language reason tied to specific questions;
- visual/spatial target handle;
- quality conditions the operator can verify;
- live indicators for blur, exposure, overlap, baseline, and occlusion;
- explicit “good enough / stop / abort” conditions;
- whether live preview is provisional and what original recording will later replace;
- no unsupported autonomous-control implication.

## 5. Closed-loop distinction

The protocol records separate states:

```text
guidance proposed
operator acknowledged
maneuver observed
usable evidence acquired
quality gate passed
questions updated
objective advanced
```

No UI button press or drone acknowledgment skips these states.

## 6. Route planning

When many views are needed, candidate viewpoints form a constrained graph. The planner may use orienteering, prize-collecting route, shortest path, set cover, min-cost flow, and matroid/submodular constraints. Deterministic tie-breaks and complexity witnesses are mandatory.

## 7. Stopping

Capture stops when hard questions are established, remaining uncertainty cannot change accepted decisions, marginal evidence gain is below policy, device/operator risk rises, or budgets expire. Continuing to collect redundant video is a failure of the planner.

## 8. Learning

Predicted and observed evidence gain, operator difficulty, flight time, image quality, and question resolution are stored in the episode capsule. Calibration can improve future guidance only through the policy promotion ladder.
