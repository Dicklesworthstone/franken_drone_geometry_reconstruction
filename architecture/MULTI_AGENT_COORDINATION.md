# Multi-Agent Coordination

FDGR supports many concurrent readers and speculative workers while preserving one coherent evidence universe and explicit mutation ownership.

## 1. Separation of knowledge and authority

A valid lease does not make stale knowledge true. A valid witness does not grant ownership. A recommendation does not reserve a resource. Mutation requires both current semantic validity and a capability/lease/fence path.

## 2. Shared and private state

Shared authoritative state includes capsules, roots, claims, obligations, and published policy epochs. Each agent may own private or shared counterfactual branches, context packs, and candidate plans. Branches cannot publish into the live lineage without normal plan compilation and commit-time validation.

## 3. Coordination graph

The agent packet exposes:

- agent/session identity and declared role;
- objective ownership and delegation;
- branch roots and visibility;
- active plan read/write/negative witness domains;
- lease identity, incarnation, expiry, and fence;
- likely conflicts and refinement opportunities;
- pending confirmation and handoff records;
- duplicate-work and shared-evidence opportunities.

## 4. Semantic reservations

Agents reserve semantic domains such as capture regions, geometry tiles, ontology asset families, archive generations, or qualification lanes. Raw text locks and filename conventions are not the coordination protocol.

Coarse reservations are safe but may cause false conflicts. Hierarchical refinement may prove disjointness; budget exhaustion must preserve conservative conflict.

## 5. Shared observation economy

When multiple objectives need overlapping evidence, the question graph coalesces candidate observations and computations. A single capture maneuver, model run, or archive readback can satisfy multiple deficits while preserving each objective's evidence trail.

## 6. Handoff and takeover

A handoff transfers understanding, not capability. It names active work, required next protocol step, lease disposition, branch state, unresolved questions, and exact anchor. A takeover requires a new admitted lease incarnation; stale workers cannot publish after fencing advances.

## 7. Merge ladder

Concurrent semantic work merges only through:

1. exact replay against the successor anchor;
2. stable-key structural composition of disjoint domains;
3. registered commutative composition;
4. explicit ordering with re-proof;
5. reconcile and replan;
6. reject.

Raw-byte merge and last-writer-wins are forbidden.

## 8. Swarm briefing

A bounded swarm briefing answers:

- who is doing what and why;
- which work is duplicated or conflicting;
- what evidence just arrived;
- which obligations or confirmations need an owner;
- what can be safely parallelized;
- where shared computation would reduce total control cost.

It is a projection of authoritative work and branch metadata, not chat presence.
