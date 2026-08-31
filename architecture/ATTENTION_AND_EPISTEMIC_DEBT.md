# Attention and Epistemic Debt

FDGR must direct an agent toward the few unresolved facts that can materially change a decision,
without thrashing whenever a score changes by a small amount. Attention is therefore a versioned,
explainable interrupt system over explicit epistemic debt.

## 1. Epistemic debt

Epistemic debt is unresolved uncertainty that creates expected future decision loss or recovery
cost. Each debt item names:

```text
debt_id
question and affected objectives
spatial/temporal scope
epistemic state
coverage and detectability
possible material answers
current decision sensitivity
expected loss if ignored
expiry or review condition
candidate resolution actions and their costs
supporting, contradicting, and missing evidence
```

Debt is not a vague confidence score. It is a typed obligation to know, consciously defer, or retire
a question.

## 2. Attention classes

1. **Protocol-critical:** continuity gaps, stale plans, indeterminate effects, privacy violations,
   missing custody, or invalid authority.
2. **Physical-safety:** collision, battery, flight envelope, operator load, weather/profile, or
   dangerous-asset ambiguity.
3. **Objective-critical:** evidence deficits on the active decision boundary.
4. **Efficiency:** duplicate work, likely reflight, expensive low-value computation, or archive
   waste.
5. **Learning:** surprises, drift, calibration failures, and policy evidence.

A lower class cannot displace an unresolved higher class merely through a larger scalar score.

## 3. Stable interruption

Attention publication uses deterministic ordering, hysteresis, suppression keys, expiry, and
acknowledgement state. An item re-enters the foreground only when:

- severity or epistemic class changes materially;
- its expiry/review condition fires;
- new evidence changes a decision boundary;
- an acknowledgement becomes invalid;
- a linked obligation transitions.

Raw model or event frequency cannot create repeated interrupts.

## 4. Attention contract

Every attention item explains:

- why it is visible now;
- what consequence follows if ignored;
- what decision it can change;
- what evidence supports and contradicts it;
- the cheapest safe resolution path;
- why neighboring items were ranked lower;
- when it will disappear, recur, or escalate.

Attention can propose a typed next step, never authorize it.

## 5. Debt retirement

A debt item terminates as `resolved`, `consciously_accepted`, `blocked`, `superseded`, or
`indeterminate`. “No longer displayed” is not a terminal state. Accepted debt records who accepted
it, the objective/policy basis, expiry, and maximum tolerated consequence.

## 6. Budget-aware scheduling

The scheduler chooses among eligible debt items using value of information, critical-path impact,
shared evidence opportunities, and total control cost. It bundles questions that can be resolved by
the same pass or viewpoint, and avoids acquiring evidence after every feasible answer maps to the
same safe action.
