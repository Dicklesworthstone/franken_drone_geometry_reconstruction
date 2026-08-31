# Deep Dive 04 — `frankensearch`

**Import decision:** progressive retrieval architecture and immutable-generation discipline are directly accretive
**FDGR authority:** derived cognition only; retrieval can propose evidence and intents but never manufacture truth

## Search in FDGR is not a text box

A complete home twin may contain millions of frame observations, tracks, spatial bricks, surface
patches, semantic hypotheses, graph relations, protocol traces, model outputs, archive receipts,
and historical versions. Agents and humans must answer questions such as:

- Where is the exterior electrical service likely to enter the house?
- Which claims support the location of the propane tank, and what contradicts them?
- Which exterior wall surfaces have never been seen obliquely enough to detect a spigot?
- What changed between the spring and autumn captures?
- Which loop closures explain this room alignment?
- Which protocol experiments established the current DJI stream profile?
- What should the next 90 seconds of flight observe to reduce the most important uncertainty?

The Frankensearch lesson is to treat retrieval as a progressive, evidence-bearing decision
process over immutable generations rather than a monolithic nearest-neighbor call.

## Mechanism 1 — staged fast then refined answers

FDGR's default query ladder is:

1. exact typed identifiers, epochs, spatial bounds, and capability filters;
2. canonical lexical retrieval over names, observations, diagnostics, protocol fields, and notes;
3. structured spatial and temporal filtering;
4. graph expansion from high-confidence seeds;
5. fast local embeddings over bounded candidates;
6. geometry-aware and topology-aware scoring;
7. quality semantic reranking using an admitted model generation;
8. contradiction, coverage, and evidence sufficiency checks;
9. token-budgeted explanation shaping.

Each stage returns a valid partial result with:

- pinned evidence and index generation;
- completeness class;
- candidate count and pruning path;
- component scores and normalization policy;
- evidence references;
- degradation list;
- stop reason and suggested continuation.

An agent can act on a high-confidence exact result immediately while refinement continues. Budget
exhaustion returns the best certified stage rather than an empty timeout or an overconfident final
answer.

## Mechanism 2 — immutable search generations

Index build and activation follow:

```text
Build
→ SealSegments
→ VerifyDocumentAndVectorCounts
→ VerifyProducerIdentity
→ CheckSourceContinuity
→ PublishGenerationRoot
→ Activate
→ RetirePriorWhenUnpinned
```

A request pins one generation. It cannot observe a lexical segment from one state and a semantic
vector set from another. The generation names:

- source evidence/scene high-water mark;
- schema and analyzer policy;
- ontology generation;
- embedding and reranker producer identities;
- dimensions, quantization, and normalization policy;
- spatial and temporal index identities;
- document/entity counts;
- capability projection;
- build and verification receipts.

A model with the same output dimension but a different checkpoint or preprocessing recipe defines
a different vector space. Mixing vectors fails closed and requires complete backfill before
activation.

## Mechanism 3 — hybrid retrieval without score-scale confusion

FDGR keeps native scores distinct:

- lexical BM25 or exact-match strength;
- vector cosine/dot similarity under named normalization;
- spatial distance/overlap;
- temporal proximity or persistence;
- graph proximity and path semantics;
- evidence confidence;
- coverage/detectability;
- contradiction penalty;
- novelty and actionability;
- model calibration confidence.

Fusion uses ranks or explicitly calibrated projections rather than comparing raw BM25, cosine,
meters, and probabilities as though they shared a scale. Reciprocal-rank fusion is a robust
baseline. Later learned or adaptive fusion remains bounded by explainability, deterministic
policy epochs, and a reference implementation.

Every result includes the score ledger so `fdgr explain` can answer why it ranked, which source
contributed, and what counterfactual would change the order.

## Mechanism 4 — exact lexical engine as a first-class capability

A pure-Rust native lexical index is essential even when multimodal models are available. Exact
terms matter for:

- DJI firmware, endpoint, packet-field, and error identities;
- model/checkpoint/license names;
- room/utility labels supplied by the owner;
- object IDs, digests, schema fields, and coordinates;
- diagnostic text and qualification receipts;
- historical annotations.

The Frankensearch Quill pattern—an owned, conformance-tested lexical engine with immutable
segments and a stronger external/reference oracle during development—is directly applicable.
Production FDGR cannot permanently depend on Tantivy or another opaque engine under the closed
universe. A simple reference inverted index precedes optimized segment encoding, SIMD scoring,
and block-max pruning.

## Mechanism 5 — vector storage and producer identity

FDGR needs several vector families:

- image/keyframe embeddings;
- cropped object/region embeddings;
- text and multimodal semantic embeddings;
- geometric descriptors;
- room/scene embeddings;
- protocol-trace or diagnostic embeddings;
- agent-memory embeddings outside canonical state.

Each family has a frozen producer identity:

```text
model artifact root
code revision
preprocessing graph
input color/resize/crop policy
output layer
normalization
vector dimension
quantization
numeric policy
```

Portable contiguous storage, scalar/SIMD exact search, and optional native ANN are separate layers.
Approximate search reports its recall qualification and never supports a certified absence claim
without an exact/coverage fallback.

Quantization is generation-specific. A quantized vector result retains the unquantized producer
identity and quantizer receipt. Re-quantization produces a successor generation.

## Mechanism 6 — graph, spatial, lexical, and semantic retrieval in one planner

A query such as “show every plausible water shutoff near the basement entry and explain the route
from the street” is not solved by vector search alone. The planner combines:

- ontology/type filters;
- 3D bounding volumes and spatial indexes;
- room/portal/utility graph traversal;
- lexical owner annotations;
- multimodal candidates;
- temporal persistence across captures;
- coverage and visibility evidence;
- path and relation constraints;
- negative/contradictory observations.

Authorization applies before candidate generation and graph expansion. A caller scoped to a
redacted export cannot infer hidden assets from degree, count, absence, nearest-neighbor distance,
or vector-result gaps.

The query plan is deterministic and certificate-bearing. Adaptive effort selection can change
candidate budgets, not visibility or semantics.

## Mechanism 7 — attention ranking as an operational product

FDGR derives an attention queue for live capture and offline review. Candidate deficits include:

- high-value surface with poor angle/resolution;
- disconnected pose component;
- weakly constrained scale region;
- contradictory utility identification;
- opening or stair topology uncertainty;
- archive custody below policy;
- calibration drift warning;
- unexplained geometry change;
- semantic claim lacking independent views;
- search/model index lag.

Scores combine severity, decision impact, uncertainty, expected information gain, cost, freshness,
and owner policy. The queue is explainable and pinned. It proposes next observations or
investigations; it does not authorize flight or device effects.

## Mechanism 8 — active-perception retrieval

“Search” can retrieve observations that do not yet exist. For a target claim or coverage deficit,
FDGR:

1. identifies the evidence gap;
2. retrieves similar resolved cases and required view conditions;
3. enumerates visible candidate viewpoints;
4. estimates detectability and information gain;
5. selects a bounded, diverse viewpoint set;
6. routes it under flight and capability constraints;
7. emits a witnessed scan-plan proposal.

This connects Frankensearch's staged candidate/refinement architecture to graph algorithms and
geometry. Candidate viewpoint generation is cheap and conservative; expensive visibility and
model-based value estimates refine only the most promising set.

## Mechanism 9 — absence and recall certificates

FDGR sharply separates:

```text
NoMatchInReturnedTopK
NoMatchAboveThresholdInSearchedGeneration
NoMatchInCoverageCertifiedAuthorizedDomain
PhysicalAbsenceResolved
```

Only the last is a semantic conclusion, and it requires coverage/detectability evidence plus
registered resolver policy. ANN top-k never proves absence. A lexical/vector index that is still
backfilling reports incomplete. A capability-limited query cannot make a global absence claim.

Coverage certificates name:

- authorized spatial/entity domain;
- pinned search and scene generations;
- exact or approximate algorithms;
- recall qualification;
- model/detector identity;
- visibility and resolution conditions;
- excluded or stale sources;
- resource limits and stop reason.

## Mechanism 10 — progressive streaming for agents

The machine contract emits stable events:

```text
QueryAccepted
InitialResults
RefinementStarted
ResultAdded
ResultRemovedWithReason
RankingUpdated
CoverageStatus
DegradedCapability
RefinementCompleted | RefinementFailed | Cancelled
```

Events carry monotonic sequence, query identity, pinned generation, and compact deltas. Final
results are stable-order JSON/JSONL/TOON-like structures. Human tables are projections, never the
only representation.

A result ID is stable within its generation and explanation ledger. Pagination uses continuation
seals over query/generation/policy, not mutable offsets into a changing index.

## Mechanism 11 — rebuildability and activation rollback

Search is derived. The source of truth is evidence, claims, and scene generations. If an index is
lost or suspected:

- deactivate it;
- fall back to exact typed/lexical/reference paths;
- rebuild from a pinned source range;
- verify counts, vectors, and source closure;
- activate the new root atomically.

Rollback activates the prior complete generation. No request sees a half-built backfill.

## Mechanism 12 — performance architecture

Target optimizations include:

- contiguous postings and vector blocks;
- immutable segment mmap-like reads through safe owned storage abstractions;
- block-max and impact-ordered lexical skipping;
- cache-friendly product/ scalar quantization researched behind exact oracles;
- portable SIMD dot products;
- bounded candidate heaps with deterministic tie-breaks;
- graph expansion over compact snapshot views;
- incremental indexing from observation capsules;
- two-tier embeddings so live queries avoid expensive model work when unnecessary;
- score-component materialization shared across explanations and ranking.

No optimization may change candidate eligibility, tie-break order, provenance, or completeness
classification without a policy-epoch change.

## Superficial imports rejected

- “vector database” as a separate mutable source of truth;
- mixing embeddings because dimensions match;
- ANN top-k interpreted as absence;
- raw score addition across incompatible scales;
- search results without source anchors and evidence spans;
- model fallback that silently substitutes hash embeddings while claiming semantic search;
- mutable index updates visible mid-request;
- capability filtering after retrieval;
- an opaque third-party search engine permanently inside the production trust domain;
- attention ranking allowed to dispatch a drone command.

## FDGR admission gate

1. Reference lexical and exact-vector engines define semantics.
2. Optimized native engines pass differential and metamorphic corpora.
3. Generation activation/rollback is root-atomic under kill injection.
4. Producer-space mismatch always fails closed.
5. Top-k and tie-break order replay byte-identically.
6. Progressive stages preserve provenance and honest completeness.
7. Capability noninterference covers counts, absence, graph expansion, and vector distance.
8. Score ledgers reconstruct every ranking decision.
9. Index loss degrades to functional reference paths and rebuilds from canonical roots.
10. Performance claims include query sets, index roots, model identities, host manifests, and
    same-binary correctness digests.

---

## Agent-native synthesis

### Context packs as progressive retrieval

Frankensearch supplies candidate generation and refinement, but Pack DNA and mandatory safety classes are FDGR semantics. Retrieval is focus- and anchor-bound, graph/spatial/temporal-aware, deterministic, privacy-filtered before ranking, and explicit about top-k versus complete-domain coverage.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
