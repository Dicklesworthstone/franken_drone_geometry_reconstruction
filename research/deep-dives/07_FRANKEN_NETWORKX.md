# Deep Dive 07 — `franken_networkx`

**Import decision:** graph semantics, deterministic algorithm contracts, snapshot views, and a broad algorithm portfolio are central to FDGR
**FDGR authority:** algorithms produce certified derived claims; physical observations remain authoritative evidence

## Reconstruction is a family of graphs, not a sequence of tensors

The usual computer-vision diagram—frames → model → point cloud—hides the structures that determine
correctness. FDGR explicitly represents at least these graphs:

| Graph | Nodes | Edges / hyperedges | Core questions |
|---|---|---|---|
| packet/timeline graph | packets, access units, clocks | sequence, containment, correlation | gaps, drift, discontinuities |
| view graph | frames/keyframes | overlap, relative pose, retrieval | connectivity, loop closure, keyframe value |
| track graph | features, observations, landmarks | correspondence / membership | data association, ambiguity, persistence |
| pose-factor graph | camera states, landmarks, calibration, scale | residual factors | globally consistent geometry |
| scale-witness graph | coordinate frames and measurements | ratios/transforms with uncertainty | metric authority and conflicts |
| surface graph | patches/planes/mesh components | adjacency, seam, coplanarity | topology, walls, openings, rooms |
| room/portal graph | spaces, doors, windows, stairs | traversability / containment | navigation and topology |
| utility hypothesis graph | meters, panels, pipes, tanks, fixtures | likely physical connection | utility tracing and missing links |
| visibility graph | viewpoints, rays, surfaces | sees/occludes/detects | coverage and next-best-view |
| semantic evidence graph | observations, hypotheses, entities | supports, contradicts, same-as | resolution and explanation |
| change graph | cross-session entities/patches | correspondence and edit | persistence versus change |
| archive object graph | roots, manifests, objects, symbols | contains/repairs/replicates | closure, retrievability, repair |
| work/obligation DAG | jobs and effects | dependency, ownership, fencing | scheduling, critical path, cancellation |
| provenance graph | claims, code, models, inputs, receipts | derived-from | replay and trust |

`franken_networkx` matters because it treats graph representation, iteration order, tie-breaks,
numeric behavior, mutation/view semantics, complexity, and failure as part of the API contract. FDGR
needs that discipline as much as it needs individual algorithms.

## Constitutional graph semantics

Every algorithm invocation declares:

```text
algorithm_id and version
input graph projection and anchor
node/edge identity and ordering
simple / multi / directed / undirected semantics
weight, capacity, sign, missing-value, and parallel-edge policy
numeric type, overflow, NaN, tolerance, and reduction policy
tie-break and canonical-choice policy
resource budget and cancellation points
expected complexity class
stale/mutation behavior
output order and canonical encoding
```

Mathematically equivalent answers are not operationally equivalent. Equal-cost shortest paths,
multiple minimum cuts, alternative matchings, and nonunique spanning trees must select by a named
canonical policy. Hash-table iteration is never a policy.

Each planning-relevant invocation emits an `AlgorithmCertificate`:

```text
input_anchor
projection_digest
n, m and relevant structural statistics
algorithm / policy / numeric identities
observed operation counters
budget consumed and stop reason
decision-path digest
ordered output digest
supporting evidence roots
```

Instrumentation can fail without changing a mathematically selected answer, but a result required
for effect planning is not publishable without the minimum certificate fields.

## Zero-copy immutable snapshot views

Algorithms operate over pinned immutable graph views backed by scene/Atlas generations. Cloning a
view increments ownership metadata; it does not deep-copy adjacency. Filtered, reversed,
subgraph, temporal, capability-scoped, and branch views compose lazily while retaining the base
anchor and policy.

A live fail-fast iterator, when intentionally exposed, checks the revision and returns a typed
mutation error. It never yields a mixture. Most authoritative computation uses immutable
snapshots, allowing safe parallel traversals and deterministic replay.

## Algorithm family 1 — connected components and dynamic connectivity

### FDGR uses

- detect disconnected view/pose components before claiming one coherent twin;
- maintain free-space and room connectivity as spatial bricks arrive or retract;
- identify disconnected utility hypotheses;
- track archive object-graph closure;
- detect whether a flight-plan waypoint graph remains connected under constraints;
- maintain component lineage across capture sessions.

A union-find reference handles insert-only phases. Deletions/retractions require recomputation or a
qualified dynamic-connectivity structure. FDGR does not label an insert-only data structure
“dynamic” and silently ignore deleted loop closures or surfaces.

Certificates name component ordering and representative-selection policy. Canonical
representatives use stable node identity rather than tree-shape accident.

## Algorithm family 2 — articulation points, bridges, biconnectivity, and block-cut trees

### FDGR uses

- find a single doorway, stair, corridor, gate, or exterior path whose uncertainty disconnects
  room topology;
- identify a single view-graph edge whose rejection splits the reconstruction;
- detect fragile utility-network hypotheses;
- prioritize capture around topological bottlenecks;
- explain which evidence edge holds two map components together.

The block-cut tree provides a compact explanation of structural fragility. In the view graph, an
articulation keyframe or bridge constraint receives higher evidence-review priority. In a
room/portal graph, a bridge can be a true chokepoint or an artifact of missing observations; the
algorithm result is advisory until supported by geometry/coverage.

## Algorithm family 3 — strongly connected components, condensation DAGs, and cycle diagnosis

### FDGR uses

- command/obligation dependency cycles;
- incremental pipeline and derived-view dependency cycles;
- directed utility-flow hypotheses;
- visibility/reachability with one-way constraints;
- protocol state-machine inference;
- semantic derivation dependency analysis.

SCC condensation creates a DAG for scheduling and explanation. Cycles in a control/effect graph
are not resolved by arbitrary edge deletion; they produce a typed deadlock/unsatisfied-dependency
finding or invoke a registered cycle-breaking policy with a certificate.

## Algorithm family 4 — topological order, critical path, antichains, and scheduling

### FDGR uses

- order reconstruction obligations and data dependencies;
- find the critical path in live/offline convergence;
- expose parallelizable antichains;
- schedule archive replication and model jobs under budgets;
- plan construction of a derived export sibling set;
- validate that crate and schema dependencies form DAGs.

Stable topological order uses declared identity tie-breaks. Critical-path costs are measured or
registered estimates with uncertainty; the result guides scheduling, not correctness.

## Algorithm family 5 — shortest, widest, minimax, and k-shortest paths

### FDGR uses

- safe viewpoint/waypoint routes under flight constraints;
- shortest walkable routes through the reconstructed home;
- likely utility routes between components;
- best evidence path from claim to original observation;
- robust network path for ATP transfer;
- alternative routes when one corridor/view/link is uncertain.

Different path semirings are explicit:

- additive distance/time/energy;
- widest path for minimum link quality or clearance;
- minimax for worst uncertainty/risk;
- lexicographic cost for hard-before-soft objectives;
- multiobjective Pareto front under bounded output.

K-shortest paths are valuable for alternatives but can explode. Output and deviation budgets are
structural, and canonical ordering is pinned.

## Algorithm family 6 — dominators and post-dominators

### FDGR uses

- determine which doorway/stair/room dominates every route to a target asset;
- identify which evidence or transformation dominates every provenance path to a claim;
- find mandatory processing stages for an export;
- locate utility components through which every plausible route passes;
- diagnose control-flow and protocol-state transitions.

Dominance often communicates “must pass through” more directly than centrality. A panel that
post-dominates every hypothesized electrical-service path is a strong review target, not proof of
physical connection. Dominator trees retain graph anchor and edge semantics.

## Algorithm family 7 — minimum spanning forests and tree backbones

### FDGR uses

- initialize pose-graph or track components from high-confidence constraints;
- select a low-cost connected viewpoint backbone;
- derive sparse explanatory skeletons from dense evidence graphs;
- connect archive transfer peers or remote workers efficiently;
- propose minimal utility/room connectivity hypotheses.

The maximum-confidence spanning tree is often a better pose initializer than arbitrary traversal,
but it discards cycles needed for consistency checks. FDGR retains non-tree constraints and uses
fundamental cycles as evidence. Nonunique forests have stable tie-break policy.

## Algorithm family 8 — cycle bases, minimum cycle bases, and consistency

### FDGR uses

- pose-graph cycle-consistency diagnostics;
- room/wall loop extraction;
- floor-plan topology;
- utility-loop hypotheses;
- protocol state-machine cycles;
- archive dependency-cycle auditing.

For a transform cycle, FDGR computes closure residual under a named Lie-group/numeric policy and
attributes inconsistency to participating constraints. A graph-theoretic minimum cycle basis can
reduce diagnostic redundancy. It does not by itself choose which geometric edge is wrong; that
requires residuals, provenance, robust estimation, and branch comparison.

## Algorithm family 9 — max flow, min cut, Gomory–Hu trees, and cut structure

### FDGR uses

- graph-cut segmentation of surfaces or rooms using geometry/semantic affinities;
- identify the smallest evidence set separating competing scene hypotheses;
- reason about corridor, free-space, network, or data-transfer capacity;
- locate weak cuts in view/pose connectivity;
- infer likely structural/utility separations;
- compute all-pairs cut summaries via a Gomory–Hu tree on appropriate undirected projections.

Capacities are typed and nonnegative under explicit numeric policy. Infinite/hard constraints use a
safe constructed bound or symbolic representation, not a magic huge float. Cut ties are
canonical. A cut is an optimization result over the chosen model, never direct physical proof.

## Algorithm family 10 — min-cost flow and circulation

### FDGR uses

- track stitching across frames with birth/death and ambiguity costs;
- multi-view object-instance association;
- assign compute/network capacity to jobs;
- route archive chunks across donors/providers;
- match observation demand to candidate viewpoints;
- infer plausible directed utility flow under constraints.

The graph construction is as important as the solver. Every cost term and hard capacity cites its
source/policy. Integer/fixed-point costs are preferred where they preserve semantics and
reproducibility. Floating costs require a numeric profile and stable tie handling.

## Algorithm family 11 — bipartite matching and assignment

### FDGR uses

- feature correspondence and landmark association;
- frame-to-frame or session-to-session object matching;
- semantic proposal-to-entity resolution;
- room/patch correspondence across time;
- viewpoint-to-coverage-target assignment;
- worker/job scheduling.

Maximum-cardinality, maximum-weight, bottleneck, and stable matching are different contracts. FDGR
never substitutes greedy nearest-neighbor matching without declaring the semantic downgrade.
Ambiguous near-ties remain visible as hypotheses when collapsing them would destroy evidence.

## Algorithm family 12 — clique, independent set, coloring, and association graphs

### FDGR uses

- maximum-consensus sets of mutually compatible correspondences;
- conflict-free selection of loop closures, semantic claims, or viewpoints;
- schedule jobs sharing scarce resources;
- select non-overlapping spatial updates;
- analyze mutually contradictory evidence.

Many forms are NP-hard. FDGR separates exact bounded instances, certified approximations, and
heuristics. A heuristic output carries a lower/upper bound or honest unknown optimality. “Maximum”
is never written when only a maximal set was found.

## Algorithm family 13 — k-core, k-truss, degeneracy, and dense substructure

### FDGR uses

- identify robustly supported view/track subgraphs;
- detect semantic entities supported by multiple mutually consistent observations;
- rank pose constraints by local redundancy;
- isolate fragile one-edge/one-view hypotheses;
- select stable cores for global optimization.

Core/truss membership is a structural support signal, not a calibrated probability. Dynamic
maintenance is validated against full recomputation after edge insertions and retractions.

## Algorithm family 14 — centrality, PageRank, HITS, PPR, and attention

### FDGR uses

- prioritize keyframes/evidence for review;
- identify high-impact rooms, portals, surfaces, utilities, and provenance nodes;
- retrieve context around a seed claim;
- select representative observations;
- rank failures or coverage deficits.

Centrality is advisory. It cannot authorize an effect or certify importance in the owner's actual
objective. Numeric convergence criteria, dangling-node policy, personalization vector, and tie
order are certificate fields.

Personalized PageRank over the evidence graph is particularly useful for bounded explanation packs
that stay near a queried asset while including diverse provenance.

## Algorithm family 15 — community detection, cuts, and room/scene partition hypotheses

### FDGR uses

- propose room clusters from portal/surface/visibility relationships;
- cluster repeated object/utility observations;
- partition massive graphs for processing and storage;
- discover capture subregions.

Community methods can be nondeterministic and resolution-sensitive. FDGR uses deterministic seeds,
canonical update order, explicit resolution parameters, and stability analysis across nearby
policies. Communities are hypotheses, not canonical rooms, until resolved with geometric and
semantic evidence.

## Algorithm family 16 — spectral algorithms and early-warning monitors

### FDGR uses

- graph connectivity/conditioning diagnostics;
- detect a view graph approaching disconnection;
- spectral clustering candidates;
- estimate mixing and expansion;
- monitor wait-for or obligation graphs;
- prioritize weakly constrained geometry.

Eigenvalue/eigenvector results need numeric policy, convergence residual, sign/rotation
canonicalization, and multiplicity handling. Near-repeated eigenvalues make vector identity
unstable; invariant subspaces or scalar certificates are preferred. Spectral warning signals are
advisory and backed by deterministic structural checks.

## Algorithm family 17 — isomorphism, subgraph matching, and repeated motifs

### FDGR uses

- recognize repeated window/door/stair patterns;
- compare room/utility motifs across captures;
- detect duplicate protocol message schemas;
- match local scene graphs during loop closure;
- verify graph serialization/canonicalization.

VF2-like search and Weisfeiler–Lehman refinements are bounded by graph size, labels, and search
budget. A timeout yields unknown, not false. Approximate motif retrieval is kept distinct from
exact isomorphism.

## Algorithm family 18 — edit distance, maximum common subgraphs, and change graphs

### FDGR uses

- explain what changed between two home captures;
- align evolving room/utility graphs;
- distinguish moved objects from structural changes;
- compare protocol profiles across firmware versions;
- compare reconstruction branches.

Exact graph edit distance is expensive. FDGR uses typed edit costs, lower/upper bounds, and
budgeted search; approximate results disclose optimality status. Changes trace back to physical
observations and correspondence evidence.

## Algorithm family 19 — tree decompositions, factor graphs, and elimination order

### FDGR uses

- choose variable elimination order in bundle adjustment/factor inference;
- preserve factorized query intermediates;
- exploit low-treewidth substructures in room/utility graphs;
- schedule local marginalization and Schur complement operations.

Graph algorithms propose orderings; the numeric solver validates fill, conditioning, and cost. An
ordering heuristic cannot alter the mathematical objective. Deterministic tie-breaks and observed
fill statistics are recorded.

## Algorithm family 20 — geometric and visibility graphs

FDGR builds domain-specific graphs using native geometry kernels:

- view-overlap and epipolar compatibility graphs;
- line-of-sight/occlusion graphs;
- navigation graphs and medial/Voronoi-like skeletons;
- surface adjacency and crease graphs;
- planar arrangement/room-boundary graphs;
- viewpoint conflict and coverage graphs.

Construction predicates use robust/adaptive numeric policies and provenance. The graph library
operates on the resulting typed view; it does not hide geometric degeneracy behind generic floats.

## Algorithm family 21 — submodular selection and diversity

Although not always exposed as a classic NetworkX family, graph-aware submodular objectives are
crucial for:

- keyframe selection;
- next-best-view batches;
- context/evidence pack construction;
- representative model/query benchmark subsets;
- archive scrub sampling.

Objectives may combine coverage, log-determinant information, facility location, novelty, and
cost. Greedy algorithms report the assumptions under which approximation bounds hold. Constraints
such as matroids/knapsacks are explicit. The objective never substitutes for hard safety or
custody requirements.

## Algorithm family 22 — temporal graphs and persistence

Every graph edge can carry validity and observation intervals. FDGR supports:

- `AS OF` scene topology;
- temporal reachability;
- persistence of semantic assets;
- first/last support and contradiction;
- dynamic-object tracks;
- changing room/utility relationships;
- evolution of protocol and archive state.

A temporal query never approximates by reading current adjacency unless requested. Interval
semantics and boundary inclusivity are registered.

## Canonical graph serialization and fingerprints

Graph identities derive from canonical ordered node/edge records, graph type, attribute schema,
and policy. Multi-edge keys and direction are preserved. Floats use canonical bit/NaN policy.
Graph views fingerprint their base root plus filter/projection expression rather than materializing
ambiguous ad hoc copies.

Fingerprints support cache keys, branch comparison, algorithm replay, and qualification. They do
not replace evidence roots for physical claims.

## Complexity witnesses and algorithmic performance

For each family, FDGR records relevant counters, such as:

- node/edge examinations;
- heap operations;
- union/find operations;
- augmenting paths;
- residual relaxations;
- recursion/search states;
- factorized tuple operations;
- fill-in and matrix nonzeros;
- convergence iterations;
- bytes read/allocated.

Counters catch accidental quadratic behavior and policy drift. Adversarial families include paths,
stars, cliques, grids, lollipops, barbells, multigraphs, disconnected components, high-degree hubs,
near-tied weights, zero capacities, negative-cycle cases where permitted, and dynamic churn.

Performance comes from algorithm choice, compact representations, zero-copy views, stable arenas,
SoA edge blocks, portable SIMD where applicable, and incremental updates—not from nondeterministic
shortcuts or foreign C libraries.

## Reference and optimized lanes

Every imported algorithm begins with a small deterministic reference implementation, often using
ordered maps/vectors and straightforward asymptotics. Optimized FrankenNetworkX crates are admitted
per algorithm family and profile after:

- exact output differential tests;
- exception/error semantic parity;
- tie-break fixtures;
- adversarial graph families;
- cancellation and budget tests;
- snapshot/view invalidation tests;
- incremental/full equivalence where applicable;
- complexity witness envelopes;
- same-binary performance receipts.

The production process never imports Python, NetworkX, PyO3, or a C graph library. Python NetworkX
may be an external qualification oracle over bounded canonical fixtures.

## Superficial imports rejected

- using a graph database but no graph algorithms;
- cloning whole graphs for each query;
- nondeterministic iteration and “any valid answer” semantics;
- confusing maximal with maximum;
- using centrality as physical truth;
- graph-cut labels treated as observed objects;
- approximate matching reported as exact;
- insert-only connectivity after retractions;
- no numeric policy for spectral/weighted algorithms;
- no provenance from graph edges to evidence;
- algorithm names without complexity and adversarial tests;
- importing Python/PyO3 into the shipping process.

## FDGR admission gate

1. Graph type, identity, ordering, weight, and numeric semantics are frozen.
2. Snapshot views are zero-copy, immutable, and anchor-carrying.
3. Each admitted family passes reference differential and adversarial fixtures.
4. Nonunique outputs obey stable canonical-choice policies.
5. Dynamic algorithms match full recomputation under insertions and retractions.
6. Budget exhaustion yields typed partial/unknown outcomes, never false exactness.
7. Algorithm certificates reproduce outputs and decision paths.
8. Capability-scoped graph views prove noninterference.
9. Complexity witnesses guard expected asymptotics.
10. Physical/semantic conclusions remain separately resolved from algorithm outputs.

---

## Agent-native synthesis

### Decision algorithms for the agent

Graph algorithms power view connectivity, loop fragility, room portals, utility topology, evidence cuts, obligation critical paths, semantic reservations, route/orienteering, set cover, and submodular context selection. Every nonunique answer has a canonical tie break and complexity witness.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
