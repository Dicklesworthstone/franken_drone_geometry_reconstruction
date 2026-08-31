# Deep Dive 06 — `frankengraphdb`

**Import decision:** import the one-version-universe composition, temperature-tiered storage, factorized execution, incremental algebra, branching, and certificate discipline
**FDGR authority:** mixed; the evidence stream and publication roots are authoritative, while scene/geometry/query projections remain reproducible claims

## The most important lesson is composition

Many systems can produce a point cloud. Some can produce a mesh. Others attach labels, optimize a
pose graph, maintain a spatial index, or upload media. The leapfrog architecture comes from making
all of these one coherent versioned system rather than loosely coupled folders and services.

`frankengraphdb`'s deepest idea is that MVCC history, time travel, replication, subscriptions,
branches, derived indexes, and analytics should not each invent a separate timeline. FDGR applies
that doctrine to physical-world evidence and reconstruction.

## Bet 1 — one evidence universe

The append unit is an immutable `ObservationCapsule` or a typed successor capsule. The ordered
stream contains facts such as:

- accepted original byte ranges and object roots;
- packet/frame boundaries and integrity status;
- device and telemetry observations;
- clock correlations and discontinuities;
- calibration observations and witness measurements;
- feature/track observations;
- solver constraints and residual evidence;
- model proposals and refusals;
- human measurements/annotations;
- archive readback/scrub events;
- compatibility and policy epoch transitions;
- device-effect attempts, receipts, and observed outcomes.

The stream drives:

- current evidence views;
- historical/time-travel queries;
- frame, track, pose, depth, and geometry generations;
- scene and utility graphs;
- semantic resolution;
- coverage and active-perception views;
- search indexes and subscriptions;
- branches and experiments;
- ATP replication and disaster recovery;
- reports and exports;
- qualification and replay.

Every derived generation declares the exact capsule high-water mark and additional roots it
consumed. There is no second untraceable “latest scene database.”

## Chronicle analogue — immutable capsules and root publication

FDGR's Chronicle-like substrate stores durable things as immutable content-addressed objects.
A publication candidate contains:

```text
basis anchor
intent / derivation identity
input object and capsule ranges
read / write / negative witnesses
child object closure
algorithm and numeric policy
schema and compatibility epochs
result and evidence digests
retention and replication obligations
```

A short deterministic coordinator validates and publishes a root marker. The exact low-level
commit protocol is selected by the admitted custody backend, but the semantic rule is root-last,
gap-free, and crash-recoverable. The mutable surface is kept tiny: active-root, lease, and bounded
coordination records. Derived structures are never more authoritative than the capsule stream.

Unlike an ordinary event log, capsules are not arbitrary JSON events. They have stable type IDs,
canonical encoding, bounded payloads, domain-separated hashes, predecessor/sequence rules, and
explicit schema migration.

## Bet 2 — Atlas: a spatially structured LSM-MVCC store

GraphDB's Strata refuses a false choice between transactional updates and scan-efficient CSR.
FDGR's analogous `Atlas` store refuses a choice between live local updates and compact global
geometry.

### Temperature tiers per spatial key

For a key such as `(property, lineage, lod, morton_cell, representation)`:

1. **Inline micro-state:** tiny observations or patches embedded in the directory record.
2. **Delta blocks:** recent sorted immutable updates with per-block MVCC and local halos.
3. **Sealed runs:** compact scan-friendly arrays for stable depth, surfels, occupancy, TSDF, mesh
   patches, or semantic relations.
4. **Anchor generations:** globally converged/cold snapshots with retained deltas for time travel.
5. **Archive objects:** content-addressed cold blocks replicated through ATP.

Most spatial cells are empty or contain tiny stable state. Hot regions near current views receive
deltas. Converged cold regions should read at dense-array/CSR-like speed and storage efficiency.
One heavyweight mutable object per voxel or triangle is rejected.

### Representation-specific blocks

Atlas can host distinct but linked families:

- image/depth tiles;
- sparse track landmarks;
- surfel blocks;
- TSDF/ESDF/occupancy bricks;
- free-space rays or compressed visibility evidence;
- mesh patches and topology seams;
- plane/line/structural primitives;
- semantic instance fragments;
- coverage and uncertainty fields.

Each family owns its canonical ordering, compression, merge proofs, halo policy, and numeric
encoding. A common manifest and version layer does not force one data structure onto every
representation.

### Migration by measured temperature

Promotion/sealing decisions use observed update rate, scan rate, branch count, version depth,
compression payoff, and rebuild cost. Adaptive policy may choose when to compact; it cannot change
logical results or discard evidence required by pinned generations.

## Bet 3 — Loom: unified factorized and worst-case-aware execution

Scene queries can explode if they materialize every intermediate combination. Consider:

> Find exterior wall patches seen in fewer than two independent views, connected to a basement
> room, near a water-line hypothesis, not covered by a resolved spigot instance, and reachable by a
> safe next-view flight path.

Naive joins over frames × surfaces × rooms × utilities × coverage × viewpoints create huge
intermediates. FDGR imports the Loom principles:

- use a typed relational/graph/spatial algebra;
- preserve factorized sets rather than expanding Cartesian products;
- select binary hash/merge joins, trie-like multiway joins, or worst-case-optimal traversal from
  one operator family;
- execute vectorized batches over contiguous Atlas/graph runs;
- push capability, spatial, temporal, and generation filters before expansion;
- retain stable ordering and plan certificates;
- account memory/output budgets structurally.

The initial engine is simple ordered iterators and reference joins. Factorization and WCO
operators are admitted only through differential plans and adversarial skewed workloads.

## Bet 4 — Ripple: incremental everything

Re-running the entire reconstruction after each new observation is wasteful and obscures causal
impact. The capsule stream is already a delta stream. FDGR defines a differential multiset algebra
for derived views:

- frame quality and keyframe eligibility;
- view/track/pose graph edges;
- connected components and loop-closure candidates;
- local depth and spatial-brick updates;
- room/portal graph hypotheses;
- semantic entity tracks and relations;
- coverage debt and next-best-view candidates;
- search documents/vectors;
- archive health and obligations;
- attention queues and subscriptions.

A generation builder consumes a contiguous capsule range, updates the circuit, verifies its
successor root, and publishes atomically. Periodic full recomputation is the oracle. Incremental
and full outputs must match under canonical ordering and numeric policy.

### Retractions are first-class

New evidence can retract a false loop closure, dynamic-object surface, utility hypothesis, or
semantic label. The algebra supports positive and negative deltas rather than append-only
accumulation of mistakes. Retraction preserves provenance: history records why a claim changed.

### Recursion and fixed points

Room connectivity, reachability, utility propagation, and transitive provenance may be recursive.
Fixed-point evaluation uses bounded iteration, monotone strata where possible, explicit
nonmonotone barriers, and deterministic convergence criteria. Approximate/early results carry
completeness status.

## Bet 5 — Beacon: one family of indexes over one truth

Indexes include:

- typed B-tree/hash identities;
- spatial Morton/Hilbert and bounding-volume indexes;
- inverted lexical indexes;
- exact and approximate vector indexes;
- frame/view/track adjacency;
- room/portal navigation indexes;
- temporal intervals;
- path/reachability summaries;
- visibility and coverage summaries.

All are derived from named roots. Each index generation can be discarded and rebuilt. Query plans
cite exact index identities and fallbacks. There is no external vector database or search server
with an independent mutable truth.

## Bet 6 — Prism: graph algorithms over zero-copy snapshot views

Graph algorithms consume immutable views over the pinned scene/evidence generation. They do not
copy the whole graph and cannot observe a half-published update. Algorithms emit:

- output digest and stable order;
- projection/anchor;
- numeric and tie-break policy;
- observed complexity counts;
- budget and stop reason;
- decision-path digest;
- evidence references.

Detailed algorithm transfers are specified in the `franken_networkx` deep dive. The key GraphDB
import is architectural: transactional scene state and analytics share a versioned store without
letting derived graph answers become canonical evidence.

## Bet 7 — branches for hypotheses, agents, and experiments

A branch is a manifest over shared immutable objects plus typed deltas. Branch uses include:

- alternative clock or calibration solutions;
- competing loop closures and robust-loss policies;
- different depth/fusion algorithms;
- structural-prior variants;
- semantic model ensembles;
- scale-witness conflict resolution;
- next-flight simulations;
- agent investigations;
- release/performance experiments.

Branch creation is cheap and zero-copy at the object level. Branch query and reconstruction use the
same APIs as trunk with a different root. Merge means produce a candidate intent/evidence set,
conflict report, and replay certificate. It never means last-writer-wins copying of mesh bytes.

A branch cannot cite fabricated simulation output as physical observation. Hypothetical deltas are
typed and visibly separate from observed capsules.

## Bet 8 — deterministic plan and result certificates

Every eligible strict operation satisfies:

```text
same source roots
+ same query/intent
+ same policy/model/numeric identities
+ same capability projection
= byte-identical ordered semantic result and certificate
```

Parallelism is deterministic through stable partitioning, fixed reduction trees, canonical tie
breaks, and explicit floating-point policy. Where bit identity is not physically portable across
an admitted numeric profile, the contract names the equivalence relation and error bounds; it is
never left as “close enough.”

Certificates include plan operators, cardinality/statistics roots, adaptive decision cards,
resource observations, fallback/degradation, output digest, and replay command.

## Bet 9 — planner-enforced capability security

Authorization is not a post-filter. Capability caveats compile into source scans, spatial
partitions, relation domains, time ranges, semantic classes, export redactions, and maximum
resolution before expansion. This prevents leakage through:

- neighbor degree;
- result counts;
- nearest-neighbor distances;
- absence claims;
- reachability/path length;
- aggregate geometry;
- vector similarity gaps;
- historical versions;
- branch names or shared object identities.

A query certificate names the authorized projection. Internal global object IDs are not exposed
when that would reveal hidden state.

## Bet 10 — replication and remote reconstruction from immutable roots

An Aegis-like layer separates:

- small ordered root/lease metadata;
- bulk immutable object movement through ATP/RaptorQ;
- derived-index rebuild;
- read replica activation;
- point-in-time root selection;
- multi-donor seeding;
- scrub and repair.

Replicas verify object identities and root closure before activation. They do not replay arbitrary
side effects. A remote worker can materialize a branch, run a deterministic job, and return a
sealed candidate root/certificate; trunk publication occurs locally after validation.

This supports the user's fleet of local machines without requiring a cloud platform or GitHub
runner. Work is scheduled by object roots and receipts rather than mutable shared directories.

## Bet 11 — hybrid retrieval and GraphRAG inside the scene planner

Text, image/vector, spatial, temporal, and graph retrieval are one planned operation. An agent
asking about “the likely electrical service route” can:

1. retrieve semantic/visual seeds;
2. constrain to exterior/service-entry regions;
3. expand along conduit, wall, meter, panel, and provenance relations;
4. consult historical captures;
5. score geometry/coverage/contradiction;
6. return factorized hypotheses with evidence paths.

The result is transactional and time-travelable because every component is pinned to one version
universe. It is not a separate RAG cache whose answer cannot be reproduced.

## Bet 12 — one process posture, multiple products

The same core supports:

- embedded Rust library;
- CLI;
- local daemon;
- MCP server;
- browser/WASM read-only viewer;
- remote deterministic worker;
- virtual filesystem projection.

Presentation layers do not define semantics. They compile to the same query, intent, publication,
and evidence contracts.

## Storage/query crate topology inspired by GraphDB

```text
fdgr-types / fdgr-error / fdgr-digest / fdgr-codec
                ↓
fdgr-object / fdgr-custody / fdgr-ledger / fdgr-capsule
                ↓
fdgr-atlas-types
├── fdgr-atlas-image
├── fdgr-atlas-track
├── fdgr-atlas-spatial
├── fdgr-atlas-mesh
└── fdgr-atlas-semantic
                ↓
fdgr-scene-graph / fdgr-index / fdgr-delta
                ↓
fdgr-query-algebra / fdgr-query-plan / fdgr-query-exec
                ↓
fdgr-graph-algorithms / fdgr-search / fdgr-coverage
                ↓
fdgr-agent / fdgr-mcp / fdgr-cli / fdgr-server
```

Dependencies form a strict DAG. Atlas cannot call the query planner. Algorithms cannot dispatch a
DJI effect. The MCP crate cannot reach storage except through the semantic service facade.

## Reference-first implementation sequence

1. canonical capsule and object encodings;
2. append-only in-memory/file reference history;
3. immutable root snapshots and time travel;
4. simple ordered scene graph and spatial maps;
5. reference query algebra and deterministic graph oracle;
6. branches and replay certificates;
7. incremental/full-equivalence circuits;
8. Atlas delta/sealed tiers;
9. factorized and WCO execution;
10. optimized native indexes;
11. ATP replicas and remote workers;
12. adaptive policies only after static workload gates.

This prevents a sophisticated store from accelerating the wrong semantics.

## Superficial imports rejected

- putting geometry blobs in a generic graph database and calling it graph-native;
- separate timelines for media, mesh, semantics, and archive;
- a mutable current scene as the only truth;
- `HashMap<Entity, Vec<Edge>>` presented as the final graph store;
- full intermediate materialization for multiway scene queries;
- incremental views without full-recompute equivalence;
- branch bytes merged directly into authoritative state;
- graph results used as observations without evidence;
- security filters after graph/vector expansion;
- nondeterministic hash iteration as a tie-break policy;
- remote workers publishing trunk directly;
- a performance README written in future tense without receipts.

## FDGR admission gate

1. Capsule history is sufficient to rebuild every canonical projection.
2. Every derived generation names contiguous consumed high-water marks.
3. Time-travel queries never mix roots.
4. Atlas reference and tiered layouts are differential-equivalent.
5. Incremental circuits match full recomputation under insertions and retractions.
6. Factorized/WCO plans match the simple join oracle on adversarial skew.
7. Graph algorithms use pinned zero-copy views and deterministic certificates.
8. Branches are isolated, cheap, and merge only through typed candidate intents.
9. Capability noninterference covers graph, vector, spatial, temporal, count, and absence leakage.
10. Remote replicas/workers verify root closure and cannot publish authoritative effects.
11. Same-input strict results replay with the registered deterministic/numeric contract.
12. All performance claims are gates tied to local qualification artifacts.

---

## Agent-native synthesis

### The linked cognitive graph

One version universe now includes objectives, questions, deficits, plans, obligations, episodes, and provenance. Branch-per-hypothesis becomes branch-per-agent/candidate. Factorized graph execution prevents context and planning queries from materializing explosive intermediate paths.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
