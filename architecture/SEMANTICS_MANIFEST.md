# FDGR Semantics Manifest

This manifest is the compact constitutional map for identity, time, space, authority, epistemics, publication, and agent operation. Detailed definitions live in the comprehensive plan and machine registries.

## 1. Canonical roots

FDGR has no mutable master mesh. The authoritative lineage is an ordered stream of immutable evidence capsules and root-last generation publications. Every derived generation names exact source high-water marks.

## 2. Anchor vector

An agent-visible anchor is a vector, not a single revision number:

```text
property_lineage
capture_epoch and evidence sequence/root
clock epoch/model root
calibration generation
scale generation and authority state
pose/constraint branch and generation
geometry generation
scene/ontology generation
coverage generation
search/context generation
archive/custody generation
schema, compatibility, privacy, and policy epochs
```

A response either binds one compatible vector or declares partial, stale, gap, reset, or indeterminate continuity. Every public occurrence conforms to `schemas/anchor_vector.schema.json` and carries the same canonical `anchor_digest` (`INV-053`).

## 3. Coordinate and numeric policy

Every spatial object declares frame identity, handedness, axis order, units, origin, transform direction, and uncertainty. Metric output requires an admitted scale witness. NaN, infinity, invalid rotations, singular calibration, overflow, and unbounded coordinates are rejected or quarantined.

## 4. Time policy

PTS, DTS, arrival, decode, display, telemetry, wall, monotonic, and device clocks remain distinct. Discontinuity creates a new epoch. Temporal alignment is an evidence-bearing model, not timestamp arithmetic hidden in an adapter.

## 5. Epistemic states

```text
observed
certified_derived
inferred
predicted
assumed
stale
unknown
contradicted
indeterminate
```

Confidence is orthogonal. Only observed and eligible certified-derived facts satisfy authoritative preconditions.

## 6. Scale authority

```text
RelativeOnly → Estimated → Witnessed → Surveyed
```

Transitions require registered evidence. Contradictory witnesses remain explicit and can demote scope. Metric authority is domain-specific and does not automatically cross disconnected components.

## 7. Claim and question semantics

Claims carry basis, scope, epistemic status, uncertainty, supporting and contradicting evidence, derivation identity, and validity interval. Questions define the terminal evidence predicate needed to establish, reject, narrow, or abstain on a claim.

An absence claim additionally requires complete authorized-domain coverage and detectability evidence.

## 8. Authority and effects

Cognition, search, graph, context, recommendation, counterfactual, and memory surfaces are authority-free. Effects require a sealed plan, valid witnesses, capabilities, policy, leases/fences, idempotency, and a short-lived ticket. Acceptance is not completion.

## 9. Publication

```text
reserve → materialize children → verify closure and semantics
        → durably stage → atomically publish root → notify consumers
```

Readers see the prior or successor complete root. Cancellation before publication removes or quarantines staging; cancellation after external dispatch reconciles outcome.

## 10. Agent operating contract

All public results share the Agent Turn Packet and the single loop:

```text
bootstrap → orient → focus → inspect → formulate → propose → compare
          → commit → watch → verify/reconcile → learn → handoff/resume
```

The packet projects synchronized world, epistemic, work, and system ledgers and never requires the agent to reconstruct protocol state from unrelated outputs.

## 11. Determinism

For strict operations, identical input roots, registries, profiles, budgets, policy, and schedule produce byte-identical eligible outputs. Bounded numeric nondeterminism must publish tolerances, seeds, hardware profile, residual summaries, and equivalence digest.

## 12. Derived state

Indexes, graph projections, context packs, reports, thumbnails, embeddings, appearance assets, and advisory memories are rebuildable. Loss of derived state cannot alter canonical evidence or claim history.
