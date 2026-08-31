# Deep Dive 02 — `frankensqlite`

**Import decision:** semantic architecture is foundational; crate dependency is gated on exact API qualification
**FDGR authority:** evidence ledger, multi-version generations, witnessed publication, and conflict control

## The important transfer is not “use a database”

FDGR receives evidence from independently progressing producers: packet readers, media importers,
clock estimators, calibration solvers, feature trackers, loop-closure workers, depth estimators,
semantic models, human annotations, and archive verifiers. They produce overlapping updates over a
world that may also be captured again months later. A single mutable project file would make
queries mix generations, lose causal basis, and turn retries into ambiguous rewrites.

The deep FrankenSQLite import is a versioned transactional semantics for evidence and derived
claims. The system can permit high concurrency while retaining a short deterministic publication
point, explicit conflict witnesses, crash recovery, historical explanation, and conservative
rejection when uncertainty prevents proving disjointness.

## Mechanism 1 — multi-version evidence, not one mutable twin

Every authoritative or derived publication creates an immutable version. A reconstruction anchor
is conceptually:

```text
PropertyLineageId
CaptureEpoch
EvidenceSequence
ClockEpoch
DeviceCompatibilityEpoch
CalibrationEpoch
SchemaEpoch
GeometryPolicyEpoch
SemanticPolicyEpoch
ModelRegistryEpoch
RootDigest
```

A query pins one anchor or an explicit temporal interval. It never reads camera poses from one
version, voxels from a second, and semantic labels from a third merely because each is “latest.”
Derived generations declare exact input roots and high-water marks.

Restoring an older project, importing a foreign bundle, changing a device compatibility profile,
or discovering a clock discontinuity creates an epoch transition. Old readers remain coherent;
old prepared mutations cannot silently target the new epoch.

## Mechanism 2 — spatial and evidentiary MVCC

FDGR applies MVCC at several granularities rather than forcing every write through a project-wide
lock:

| Domain | Version unit |
|---|---|
| original custody | immutable object/chunk and manifest root |
| observation history | capsule run and sequence range |
| frame metadata | frame or bounded frame block |
| tracks | track shard and observation range |
| pose graph | node/edge block plus graph root |
| depth | image tile or depth pyramid tile |
| fused geometry | spatial brick at level-of-detail |
| occupancy/free space | spatial brick and sensor-model epoch |
| mesh/topology | patch/component and adjacency generation |
| semantic state | entity claim, relation family, and ontology epoch |
| coverage | visibility cell/surface patch and detectability policy |
| archive state | provider/object receipt and custody generation |

Readers pin a root and access immutable chains. Writers stage new versions for disjoint blocks in
parallel. Publication validates their basis and installs a new root. Reclamation is epoch-aware:
objects remain live while pinned readers, branches, exports, or evidence references can reach them.

A spatial block is not chosen merely for implementation convenience. Its size is part of a
registered policy and same-binary workload experiments because coarse blocks reduce metadata but
increase false conflicts and write amplification, while tiny blocks increase lookup and manifest
cost.

## Mechanism 3 — read, write, and negative witnesses

A prepared operation records what made its conclusion valid.

### Positive read witnesses

- exact frame/object digest and decode recipe;
- camera intrinsics, distortion, rolling-shutter, and stabilization generation;
- clock-map segment and residual bounds;
- keyframe revision and image-quality measurements;
- feature/track observations and descriptor policy;
- pose node/edge revisions;
- spatial brick and neighboring-halo revisions;
- semantic entity/relation revisions;
- model identity, preprocessing identity, and output-schema identity;
- archive readback receipt and provider semantics profile.

### Negative witnesses

Negative evidence is not absence from a cache. Examples include:

- no loop-closure candidate above a registered retrieval floor in the searched domain;
- no occupied evidence in a visibility-certified free-space volume;
- no doorway hypothesis on a wall region observed at sufficient resolution and angle;
- no propane-tank candidate in a coverage domain where that object class was detectable;
- no conflicting device command in the fenced operation interval;
- no newer calibration or policy epoch at commit;
- no object already published under an idempotency identity.

Every negative witness names the complete domain, generation, algorithm/policy, limits, and
completeness status. An exhausted search can produce `UncertifiedAbsence`; it cannot produce a
false proof of absence.

### Write witnesses

Write witnesses name exact semantic domains:

- frame annotations or clock segment;
- pose node/edge set;
- track observations;
- spatial brick interiors and halo dependencies;
- mesh patches and topology relations;
- semantic claims and relation keys;
- coverage cells;
- idempotency and effect records;
- publication roots and retention state.

## Mechanism 4 — hierarchical conflict refinement

Witnesses form a sound hierarchy:

```text
property
└── reconstruction lineage
    └── domain (pose / depth / geometry / semantics / coverage / archive)
        └── generation or spatial level
            └── shard / brick / entity family
                └── field / edge / voxel mask / surface patch / time interval
```

Coarse witnesses are mandatory. Finer witnesses are optional proofs of disjointness. When two
candidates conflict coarsely, the coordinator can spend bounded work refining to exact masks,
fields, tracks, or adjacency keys. Exhaustion yields a conservative conflict and replan. It never
permits overlap merely because the refinement budget ended.

The scheduler may estimate the value of information:

```text
P(refinement proves disjointness)
× value of avoiding recomputation
− refinement CPU / memory / latency cost
```

This estimate chooses effort only. Soundness comes from the mandatory coarse witness.

## Mechanism 5 — SSI-style dangerous structures for derived claims

Some reconstruction races are not direct write/write conflicts. For example:

1. branch A reads calibration C and produces pose constraints;
2. branch B reads those poses and publishes depth;
3. a calibration update invalidates the original assumptions;
4. the publications could form a cycle in which each appeared valid against its own snapshot.

FDGR tracks read/write anti-dependencies across recently prepared and committed publications.
Dangerous structures are rejected or forced through deterministic replay against a newer anchor.
Negative witnesses participate, so a new loop closure, object observation, or free-space conflict
can invalidate a conclusion that relied on nonexistence.

The claim is scoped carefully: FDGR can provide serializable semantics over its evidence ledger and
publication protocol. It cannot make the physical world or DJI firmware transactional. Device
effects remain a separate observed-and-reconciled boundary.

## Mechanism 6 — deterministic short publication coordinator

Expensive work happens outside the commit point. Producers submit complete candidates containing:

- basis roots and epochs;
- read/write/negative witnesses;
- deterministic intent and parameter digest;
- materialized child object identities;
- expected root transition;
- resource and retention obligations;
- policy and numeric identities;
- evidence and validation receipts.

The coordinator:

1. orders ready candidates by a stable registered policy;
2. validates epoch, lease, capability, and idempotency fences;
3. selects the commit anchor;
4. checks witnesses and bounded refinements;
5. detects dangerous dependency structures;
6. validates object closure and root digest;
7. allocates a gap-free publication sequence;
8. atomically publishes the new root and durable receipt;
9. releases expensive callbacks and derived notifications after leaving the critical section.

This is not a global-lock architecture. Preparation, materialization, queries, graph work, depth,
fusion, model inference, and transfer proceed concurrently. Only the final authoritative ordering
is serialized where total order is semantically required.

## Mechanism 7 — semantic merge ladder

Last-writer-wins and raw-byte merge are forbidden. When a basis advanced, FDGR attempts:

1. **Exact deterministic replay.** Re-run the same intent and parameters against the successor
   anchor.
2. **Stable-key structural composition.** Merge disjoint fields, track observations, graph edges,
   spatial masks, or semantic relation keys in canonical order.
3. **Registered commutative merge.** Apply only when an algorithm registry proves the operation is
   associative/commutative under the same numeric and quantization policy.
4. **Confidence-preserving evidence union.** Preserve independent observations without prematurely
   collapsing them to one estimate; recompute the resolver as a derived successor.
5. **Reconcile external effect, then replan.** Used when archive or DJI outcome may already exist.
6. **Reject.** Unknown semantics, incompatible model spaces, calibration epochs, or numeric policies
   do not merge.

An accepted merge emits a certificate containing basis roots, conflict domains, chosen mechanism,
canonical normal form, decision path, and successor digest.

## Mechanism 8 — branches as reconstruction hypotheses

Branches are cheap manifests over shared immutable objects. They support:

- competing calibration hypotheses;
- alternative loop closures;
- robust-estimator and solver policies;
- native versus external-oracle comparisons;
- alternative scale witnesses;
- semantic model ensembles;
- “what would this next flight observe?” planning;
- redacted export views;
- agent-isolated investigations.

Branch merge never means copying an arbitrary mesh into the authoritative twin. It means producing
a candidate evidence/claim set, conflict report, and proof obligations for normal publication.

## Mechanism 9 — recovery and the truth hierarchy

The ledger must answer after any crash:

- which bytes were accepted;
- which objects became durable;
- which capsule sequence is complete;
- which candidates were prepared;
- which root was published;
- which callbacks/derived consumers may not have observed the root;
- which DJI or cloud effects are known, failed, or indeterminate;
- which branches and readers still pin objects;
- which repair, archive, or retention obligations remain.

The truth hierarchy is:

1. sealed original bytes and authoritative evidence capsules;
2. published roots and receipts;
3. calibration/clock and policy generations;
4. reproducible derived generations;
5. disposable caches, indexes, previews, and reports.

Derived indexes and meshes can be discarded and rebuilt. Original evidence, operation identity,
and publication history cannot.

## Mechanism 10 — VFS and storage abstraction without semantic leakage

The ledger core operates over a narrow FDGR VFS/custody trait rather than direct paths and ambient
filesystem calls. It exposes staged write, sync class, atomic/root promotion, immutable read,
listing by manifest, and corruption/fault surfaces. Storage providers cannot redefine transaction
or durability semantics.

FrankenSQLite itself is admitted as a production dependency only when the exact pinned revision
supports the required context-first API, recovery behavior, encoding/format profile, and
concurrency gates. Until then FDGR maintains:

- a deterministic in-memory reference ledger;
- a simple append-only reference file ledger;
- the same public semantic interface.

This prevents the design from declaring unfinished sibling work authoritative by association.

## Mechanism 11 — conflict and regime telemetry as advisory policy

Sequential monitors may observe conflict rate, replay success, brick hotness, witness-refinement
payoff, commit latency, and version-chain depth. They may tune batching, compaction priority, or
refinement effort. They do not weaken serializability, permit an unwitnessed write, or turn an
unknown external result into failure/success.

## Data layout and performance implications

The initial reference representation favors clarity: ordered maps, immutable vectors, explicit
version chains, canonical serialization. Optimized layouts are admitted behind identical
semantics:

- struct-of-arrays transaction/witness records;
- cache-line-padded shard metadata;
- append-only arenas with compact typed handles;
- sorted spatial/write sets and SIMD-safe comparisons;
- batched validation;
- epoch-based reclamation;
- small inline witness sets with spill blocks;
- content-addressed deduplication;
- deterministic parallel preparation.

Every optimization has a scalar/reference path and same-binary A/B receipt. A throughput claim is
invalid unless semantic output digests, crash behavior, and workload identity match.

## Superficial imports rejected

- using SQLite merely as a key/value blob bucket;
- one mutable `current_project` row;
- snapshot isolation labeled serializable without dependency tracking;
- “no row returned” treated as a negative proof;
- global writer lock retained indefinitely as an “interim” architecture;
- last-writer-wins pose, voxel, or semantic updates;
- auto-merging results from different model/calibration spaces;
- callback execution while holding the publication lock;
- optimistic performance claims based on parser/source presence rather than end-to-end recovery.

## FDGR admission gate

1. A single-threaded reference ledger defines all state transitions.
2. Candidate implementations pass deterministic differential traces.
3. Crash injection covers every reserve/materialize/publish byte boundary.
4. Negative-read phantom campaigns cover loop closures, occupancy, semantic absence, and epochs.
5. Concurrent disjoint spatial writes scale without changing root digests.
6. Same-block/overlap conflicts either use a registered proof or reject.
7. Branch replay and merge certificates reproduce.
8. Long readers remain coherent through compaction/reclamation.
9. Recovery distinguishes committed, aborted, pending, and indeterminate effects.
10. The pinned FrankenSQLite adapter earns its own integration receipt before becoming default.

---

## Agent-native synthesis

### Question and plan MVCC

The question/objective graph and four ledgers read one snapshot. Candidate plans carry positive, negative, aggregate, spatial, and generation witnesses. Commit refinement may reduce false conflicts but cannot miss a real one. Episode and surprise capsules are transactional evidence, while context/search generations remain rebuildable.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
