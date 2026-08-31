
# Capture Playbook — Initial Hypotheses

This is a starting operating profile, not a universal flight prescription. The coverage engine
will eventually turn it into environment- and device-specific suggestions. The operator remains
responsible for lawful, safe manual flight.

## Exterior baseline

- Prefer original highest-quality video and a consistent non-digital-zoom lens state.
- Use slow, smooth motion and avoid abrupt yaw; blur and rolling-shutter stress reduce useful
  correspondences.
- Maintain substantial view overlap and vary viewpoint enough to create parallax.
- Fly at least one broad perimeter orbit for global shape and one closer oblique pass for details.
- Include corners from both sides, roof/eave transitions where lawfully visible, ground-to-wall
  contact, steps, doors, windows, and utility areas.
- Capture short stationary or very-slow segments at important assets.
- Avoid large exposure jumps where possible; retain D-Log M only if the color pipeline is recorded
  and deterministic.
- Do not rely on sky, reflective glass, water, moving foliage, repetitive siding, or featureless
  walls as strong geometry.
- Revisit loop-closure areas near the beginning/end of passes.

## Semantic inspection pass

For each target asset, obtain multiple views with context and detail:

```text
wide context → medium relation to building → close readable/detail view → alternate angle
```

Targets may include HVAC outdoor equipment, propane storage, water spigots, meters/service entry,
doors, windows, garage openings, stairs, paths, gardens, and trees. Do not infer hidden indoor
components from exterior footage.

## Evidence quality signals

The live assistant should report, rather than conceal:

- blur, clipping, low light, codec corruption;
- insufficient baseline or overlap;
- repeated texture or weak features;
- sky/water/glass/foliage fraction;
- dynamic objects;
- uncovered surface bins;
- poor incidence angle or ground sampling distance;
- missing scale/calibration evidence;
- unresolved loop closure;
- semantic target with only one view.

## Calibration and scale aids

Optional measured markers or known segments can radically improve metric trust. Their exact
geometry, placement, visibility, measurement uncertainty, and frame associations must be recorded.
A common object whose dimensions merely “seem standard” is an estimate, not a witness.

## Agent-guided evidence loop

Capture is driven by objective-bound questions rather than a generic completeness percentage:

```text
orient with profile=pilot
→ inspect the highest-value evidence deficit
→ compare a small maneuver frontier
→ operator accepts or rejects guidance
→ observe maneuver and source quality
→ acquire and seal evidence
→ update coverage/questions
→ stop when terminal evidence or marginal-value rule is reached
```

Every maneuver states the question(s) it is expected to change, desired target/framing/baseline/resolution, privacy and safety constraints, predicted information gain, cost, invalidators, and good-enough/stop/abort conditions. Guidance is not a flight-control command. Operator acknowledgment, observed movement, usable evidence, quality acceptance, and question resolution are separate states.

The agent should prefer one maneuver that resolves several correlated deficits, avoid redundant or low-value footage, preserve battery and operator attention, and explicitly recommend stopping when remaining uncertainty cannot alter an accepted decision.
