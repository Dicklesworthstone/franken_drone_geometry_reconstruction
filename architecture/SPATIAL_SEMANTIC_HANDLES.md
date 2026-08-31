# Spatial and Semantic Handles

A physical-world system becomes hostile to agents when every tool invents its own coordinates,
frame indices, bounding boxes, object IDs, and scene names. FDGR uses one family of frame-complete,
anchor-bound handles across capture, geometry, semantics, questions, plans, reports, and viewers.

## 1. Handle contract

Every spatial handle declares:

```text
stable handle identity and kind
anchor vector and branch
authoritative coordinate-frame identity
handedness, axis order, units, origin, and transform direction
scale authority and uncertainty
spatial support: point, ray, frustum, mask, polygon, volume, surface, path, or relation
semantic aliases and human landmarks
time/capture interval when relevant
coverage and occlusion context
expansion affordances and neighboring abstractions
```

A naked `[x,y,z]`, pixel rectangle, or point-cloud index is never public protocol.

## 2. Human-legible landmarks

The system maintains anchor-bound aliases such as:

```text
north garage wall
rear-left HVAC unit
path from driveway gate to propane tank
second-floor east window opening
uncovered strip behind the maple tree
```

Aliases are derived references with provenance and ambiguity state. They never replace canonical
geometry identities, but they let an agent and human discuss the same place economically.

## 3. Semantic zoom

A handle can expand upward or downward without a new search:

```text
pixel/mask ↔ ray/frustum ↔ track ↔ surface patch ↔ asset hypothesis
            ↔ room/portal/utility relation ↔ question ↔ objective
```

Expansion carries exact anchor compatibility and omission/coverage semantics.

## 4. Stable references across refinement

A successor geometry generation may split, merge, or move patches. FDGR records correspondence and
supersession edges instead of silently reusing IDs. An old handle remains historically resolvable
and either maps to a certified successor set or reports ambiguity.

## 5. Privacy and capability

A handle carries disclosure scope. Expansion is authorized before graph traversal or evidence
materialization so counts, neighborhood structure, and absence do not leak restricted regions.
