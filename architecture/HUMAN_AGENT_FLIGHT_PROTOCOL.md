# Human–Agent Flight and Capture Protocol

The initial FDGR control posture is a human-piloted aircraft with an agent acting as evidence-aware
copilot. The protocol optimizes useful evidence while preserving operator attention, physical
safety, device limits, privacy, and the distinction between recommendation and observed execution.

## 1. Four separate facts

FDGR never conflates:

1. an agent **recommended** a maneuver;
2. the operator **acknowledged** the instruction;
3. the aircraft was **observed** to execute the maneuver;
4. the resulting media **resolved** the intended evidence deficit.

Each has a separate timestamp, identity, and receipt.

## 2. Pilot card

Only one normal pilot card is foregrounded at a time. It contains:

```text
plain-language maneuver and spatial handle
why this view matters and which questions it can resolve
safe envelope and device/profile prerequisites
visual cue for successful positioning
quality conditions to hold: distance, speed, parallax, exposure, overlap
estimated battery/time/operator burden
abort and skip conditions
expected evidence gain and automatic stop condition
acknowledge / skip / cannot-comply / abort actions
```

Critical abort guidance preempts every normal card. The agent does not stream dense analytical prose
during manual flight.

## 3. Closed-loop guidance

```text
select question bundle
→ emit pilot card
→ receive operator acknowledgement or refusal
→ observe source/telemetry continuity and quality
→ detect evidence gain or failure mode
→ complete, adapt, pause, or abort
→ update questions, coverage, and next card
```

The next card depends on observed evidence, not merely elapsed time or presumed compliance.

## 4. Safety and authority

Pilot guidance is authority-free advice. Initial FDGR does not autonomously command aircraft
motion. Any later device-control surface requires a separate capability, device profile,
precondition/fence protocol, operator-presence policy, kill path, and terminal observation proof.

## 5. Cognitive-load rules

- one primary instruction and at most two short supporting cues;
- stable vocabulary and landmarks throughout a session;
- no coordinate math unless explicitly requested;
- no mid-maneuver objective switch except safety preemption;
- no repeated alert without a material state change;
- exact “why”, “done when”, and “abort when” fields;
- operator refusal is normal evidence, not an error.

## 6. Evidence-aware stopping

Capture stops when the active question bundle reaches its registered terminal predicates or the
marginal expected evidence gain falls below total flight/control cost. “More footage” is not a goal.
