# Comprehensive Plan for the Design and Implementation of
# `franken_drone_geometry_reconstruction` (`fdgr`)

**Document class:** normative architecture, research, and execution plan
**Initial issue date:** 2026-08-30
**Status:** Draft 0.4 — agent-native synthetic-system revision for public iteration
**Repository:** `Dicklesworthstone/franken_drone_geometry_reconstruction`
**Primary audience:** implementers, autonomous coding agents, computer-vision researchers,
photogrammetrists, robotics engineers, storage/reliability engineers, security reviewers, drone
integration researchers, and operators
**Normative companions:** `registries/*.toml`, `schemas/*.json`, `ARCHITECTURE.md`,
`FRANKENSTACK_DEEP_DIVE.md`, `DEPENDENCY_POLICY.md`, `MODEL_REGISTRY.md`,
`DJI_ADAPTER_RESEARCH.md`, `docs/AGENT_OPERATING_MODEL.md`,
`architecture/AGENT_ABSTRACTION_TOWER.md`, `architecture/SEMANTICS_MANIFEST.md`, `SECURITY.md`,
`PRIVACY.md`, `IMPLEMENTATION_STATUS.md`, and accepted ADRs


## Revision 0.4 companion corpus

The exact per-project research, cross-project composition analysis, agent operating model, abstraction tower, question/objective graph, context-pack contract, accretion architecture, semantic manifest, target crate graph, process region tree, owned object-graph format, algorithm portfolio, active-perception design, dependency policy, and local qualification contract are indexed in [`DESIGN_INDEX.md`](DESIGN_INDEX.md). These companion documents are normative where this plan delegates a registry, format, source identity, or operational procedure to them.

---

# Document control

## Why this plan is unusually demanding

Drone video to 3D is easy to demo and hard to trust. A modern model can turn a few images into a
visually compelling point cloud in minutes. That output may still have an arbitrary or unstable
scale, a distorted camera model, broken topology, silently omitted surfaces, transient objects
fused into walls, hallucinated detail, uncalibrated confidence, and no durable connection to the
source frames. A language model can caption an exterior condenser or electrical meter. It may also
confuse a vent, infer a hidden indoor component, or confidently assert that an asset is absent from
an area the camera never saw.

The acquisition side is equally deceptive. A controller may display a live image without exposing
a supported SDK stream. A screen recording may omit frames, add overlays, change crop, and run on a
different clock from the aircraft original. A successful ffmpeg exit does not prove timestamp
fidelity. A successful cloud response does not prove that an object can be restored. A cancelled
job may leave a child process, upload, or staged publication alive. A digital twin that ignores
these distinctions will appear magical in a video and become unreliable precisely when an agent or
human begins to depend on it.

This plan therefore specifies, before large-scale implementation:

- the semantic truth model and authority boundaries;
- exact identity, version, clock, coordinate, calibration, scale, and uncertainty contracts;
- source acquisition and compatibility behavior;
- raw evidence custody and multi-artifact publication;
- online and offline reconstruction lanes;
- model-worker isolation and per-artifact admission;
- semantic observation, resolution, counterevidence, and absence rules;
- coverage and next-best-view reasoning;
- local and cloud archive, repair, retention, and restore;
- agent, CLI, MCP, report, search, and memory surfaces;
- privacy, capability, threat, and failure domains;
- deterministic replay, fault campaigns, ground-truth benchmarks, and economic metrics;
- work-package dependencies and acceptance gates;
- negative evidence required before public claims are permitted.

A requirement is not implemented because a type, command, model wrapper, or code path exists. It
is implemented only when its contract, success/failure behavior, migration, deterministic tests,
positive evidence, negative evidence, and documentation agree.

## Normative language

The terms **MUST**, **MUST NOT**, **REQUIRED**, **SHALL**, **SHALL NOT**, **SHOULD**, **SHOULD
NOT**, **RECOMMENDED**, **MAY**, and **OPTIONAL** are used in their ordinary RFC 2119 sense.

The machine registries are authoritative for published stable IDs. The prose explains intent and
may contain more detail. If a registry and prose disagree, implementation MUST stop until the
conflict is resolved by an ADR and synchronized change.

## Evidence labels

This plan distinguishes:

- **FACT:** established by a primary source or checked repository at the snapshot date.
- **DESIGN:** a normative FDGR choice.
- **HYPOTHESIS:** plausible but requires measurement.
- **TARGET:** an acceptance objective, not a measured current result.
- **NEGATIVE EVIDENCE:** a tested absence, rejection, failure, incompatibility, or counterexample.
- **UNKNOWN:** an unresolved fact that cannot safely be inferred.

Future model cards, vendor pages, papers, or repository claims remain source claims until FDGR
reproduces them under an admitted profile. “Open source,” “open weights,” and “Apache repository”
are not interchangeable licensing facts.

## Stable identifiers

| Prefix | Meaning |
|---|---|
| `INV-` | constitutional invariant |
| `BET-` | leapfrog design bet |
| `GOAL-` | primary goal |
| `NONGOAL-` | explicit non-goal |
| `CAP-` | capability or authority class |
| `EFFECT-` | external effect class |
| `CLAIM-` | claim family and terminal predicate |
| `ERR-` | stable error |
| `SCHEMA-` | versioned public schema |
| `ADR-` | architecture decision |
| `WP-` | dependency-ordered work package |
| `GATE-` | acceptance gate |
| `TEST-` | required test family |
| `SLO-` | measurable service objective |
| `RISK-` | tracked risk |
| `OPEN-` | unresolved design question |
| `MODEL-` | exact model artifact/profile family |
| `OP-` | operation-cost registry entry |

Published identifiers MUST NOT be renumbered. Superseded entries remain as tombstones with a
replacement reference.

---

# Preface: the opportunity

A reliable, inexpensive way to create a detailed digital twin from a small manually piloted drone
would be broadly useful:

- homeowners could document buildings and utilities before maintenance or renovation;
- inspectors and contractors could reason from a common evidence base;
- emergency and disaster teams could compare before/after state;
- preservation projects could capture vulnerable structures;
- accessibility and robotics systems could understand paths, stairs, entrances, and obstacles;
- energy and resilience analysis could use geometry and visible equipment;
- agents could monitor change, retrieve evidence, plan additional capture, and produce reports
  without repeatedly rewatching hours of video.

The system becomes more valuable when it is not tied to one vendor, one model, one cloud, one file
format, or one moment in the model frontier. Original evidence should remain reinterpretable ten
years later. A new geometry model should be able to produce a branch without destroying the old
one. A calibration correction should make every affected claim traceable. A semantic label should
show the frames, masks, rays, surfaces, dimensions, and counterevidence that support it. A cloud
provider outage should be a replica problem, not a truth problem.

The project thesis is:

> The best drone-to-digital-twin system is an evidence database with reconstruction capabilities,
> not a reconstruction script with some metadata. It should preserve exact observations, make
> coordinate and scale assumptions explicit, separate neural proposals from admitted claims,
> publish coherent immutable generations, explain uncertainty and missing coverage, survive
> cancellation and crashes, and expose a narrow agent-native interface over the whole lifecycle.

The radical leap is the composition. Feed-forward 3D models, classical geometry, semantic VLMs,
content-addressed custody, structured concurrency, MVCC, graph reasoning, progressive search,
repair coding, deterministic reports, and agent memory exist separately. FDGR is designed so they
operate inside one truth and evidence model.

---

# Part I — Discovery and ecosystem forensics

## 1. Research questions

The initial design investigation asks:

1. What source bytes and live signals can be acquired from a DJI Flip or similar consumer drone
   without assuming unsupported SDK access?
2. What distinctions are necessary between live preview, controller recording, application
   export, and original aircraft media?
3. Which current open-weight geometry and multimodal models are promising, and what license,
   reproducibility, and domain-shift constraints matter?
4. What classical reconstruction and validation remains necessary even with strong feed-forward
   priors?
5. How can a semantic twin represent visible assets, hidden unknowns, counterevidence, and absence
   without allowing a VLM to become truth?
6. How should large video and geometry object graphs be retained locally and replicated cheaply to
   B2/R2 while remaining recoverable and provider-independent?
7. Which mechanisms from the existing Franken projects materially improve correctness,
   performance, reliability, agent efficiency, and verification?
8. What architecture remains useful if a live DJI adapter, a specific model, a GPU, a cloud
   provider, or a derived index is unavailable?

## 2. DJI Flip and acquisition findings

### 2.1 Product facts

**FACT (snapshot 2026-08-30):** DJI documents the Flip as a sub-249 g aircraft with a 1/1.3-inch
camera, 24 mm-equivalent fixed-focus lens, mechanical gimbal, MP4 recording up to 4K/100 fps and
150 Mbps, D-Log M, O4 transmission, controller live view up to 1080p/60, approximately 120 ms
minimum advertised latency, and Wi-Fi 5 media download. It has forward infrared and downward
visual/infrared sensing, but those sensing products are not assumed to be available as a general
mapping depth stream.

**FACT:** DJI's current Mobile SDK V5 product support table does not list the Flip. Absence from the
table does not prove that no integration path exists; it does make it architecturally unsound to
assume a supported SDK video feed.

### 2.2 Design consequence: acquisition ladder

FDGR MUST support useful reconstruction through progressively less certain paths:

1. **Tier A — exact original media:** microSD, internal storage, or explicit original export.
2. **Tier B — documented recorded preview:** controller/app recording, OS screen recording, or
   HDMI/USB capture where available.
3. **Tier C — supported live interface:** official SDK, UVC, RTMP/RTSP, local API, or vendor export
   for an exact admitted profile.
4. **Tier D — owner-authorized protocol research:** characterization of the operator's own paired
   devices and sessions, read-only first, with no bypass of authentication, encryption, pairing,
   access control, account restrictions, or geographic controls.

The project MUST NOT depend on Tier D for its first useful release. Original-media import provides
higher image quality and a stable foundation; a live preview is an accelerator for immediate
feedback and capture guidance.

### 2.3 Source equivalence is forbidden

Live preview and original media may differ in:

- resolution, frame rate, crop, aspect, field of view, and digital zoom;
- rolling-shutter exposure timing and stabilization transforms;
- codec profile, GOP structure, bitrate, sharpening, denoising, tone/color transform, and overlays;
- dropped/duplicated frames and variable frame rate;
- latency and timestamp origin;
- controller/app/OS resampling;
- calibration applicability.

They MUST have separate source, clock, calibration, and compatibility identities. An explicit
fusion/alignment policy may connect them. No path may substitute one for the other because their
filenames or visible scene are similar.

## 3. Geometry model findings

### 3.1 MapAnything

**FACT:** Meta's Apache-licensed MapAnything checkpoint describes a unified feed-forward metric 3D
model covering multi-image SfM, multi-view stereo, monocular metric depth, registration, depth
completion, and related tasks. Its interface can produce camera, depth, point, ray, and confidence
products from multiple modality combinations.

**DESIGN:** The exact Apache checkpoint is the initial default geometry candidate because its
license posture and unified interface make it the cleanest baseline. It remains a candidate prior,
not a metric or correctness authority. UAV/home-domain results, crop/coordinate behavior,
uncertainty, deterministic profile, and classical refinement compatibility must be measured.

### 3.2 Depth Anything 3 Streaming and stateful models

**FACT:** Depth Anything 3 publishes a streaming mode for ultra-long video under a bounded-memory
sliding-window design. CUT3R and related recurrent/stateful systems explore continuous online 3D
state.

**HYPOTHESIS:** Stateful or sliding-window models may provide the best online draft geometry and
coverage feedback. They also create hidden-state, reset, boundary, drift, identity, and replay
problems. An admitted worker must externalize state checkpoints and frame high-water marks; an
opaque GPU object cannot be a durable generation.

### 3.3 VGGT-Ω

**FACT:** The released VGGT-Ω materials use a FAIR Noncommercial Research License. The maintainers
posted an August 18, 2026 notice that possible benchmark contamination in an ancestor checkpoint
may inflate reported 1B benchmark results.

**DESIGN:** VGGT-Ω is a research-only challenger. It may participate in noncommercial comparative
experiments under its terms. It cannot be the default distributable/commercial lane, and the
reported affected benchmark numbers do not count as FDGR evidence.

### 3.4 Classical geometry remains necessary

No foundation model eliminates the need for:

- explicit camera/crop/distortion reconstruction;
- deterministic keyframe policy;
- correspondence and track diagnostics;
- robust pose graph and loop closure;
- bundle or local refinement;
- held-out reprojection;
- scale-witness fusion;
- depth consistency and cross-view checks;
- dynamic-region rejection;
- free-space and topology validation;
- calibrated uncertainty and failure classification.

The exact balance between learned and classical components is an empirical question. The authority
model is not.

## 4. Semantic model findings

### 4.1 Qwen3.8-27B

**FACT:** Qwen3.8-27B is an Apache-2.0 dense vision-language model with native image/video
understanding and long context. It is a plausible local semantic reasoner for selected frames,
video segments, geometry renders, structured scene evidence, and ontology questions.

**DESIGN:** Qwen may produce observations, hypotheses, relations, search text, inspection
questions, and counterevidence. It MUST NOT define geometry, scale, coverage, absence, authority,
or completion. It MUST NOT turn an unseen indoor component into an observed fact.

### 4.2 Segmentation and tracking

**FACT:** SAM 3.1 advertises more efficient multi-object video tracking through object multiplexing.

**DESIGN:** Segmentation/tracking supplies masks and temporal identity proposals. The exact
checkpoint and terms require artifact admission. Masks are imperfect observations, especially for
fine wires, foliage, transparent/reflective surfaces, repetitive textures, occlusion, and
ambiguous prompts. Geometry and multi-view consistency remain necessary.

## 5. Cloud archive findings

**FACT:** Cloudflare R2 and Backblaze B2 expose S3-compatible APIs with provider-specific
capabilities and limitations. Multipart operations, checksums, object lock, lifecycle, and
consistency behavior differ by provider and operation.

**DESIGN:** FDGR owns provider-neutral object identity and complete-object digests. ETag is not
identity. Content-addressed immutable keys avoid same-key writer ambiguity. An upload response
creates a pending replica. Independent `HEAD`, size/metadata validation, and sampled or full
readback promote it to verified. Restore drills, not API compatibility tables, prove recovery.

## 6. Frankenstack findings

The detailed project-by-project analysis is normative in `FRANKENSTACK_DEEP_DIVE.md`. The synthesis
is:

```text
Asupersync          owned execution, capability context, cancellation, replay, ATP/RaptorQ
FrankenSQLite       transactional multi-version evidence and claim history
FrankenFS           custody, staged publication, crash matrices, repair planning
Frankensearch       progressive explainable retrieval and attention
Franken Markdown    deterministic evidence reports and exact citations
FrankenGraphDB      versioned scene/provenance graph, branches, algorithms, plan discipline
Dwarf Fortress MCP three-plane authority, witnessed intents, obligations, compatibility epochs
FastMCP Rust        replaceable agent presentation plane
Eidetic Engine      advisory operational memory with one-way provenance
```

The common architecture is a deterministic semantic core, explicit capability-scoped effects,
crash-safe ledger, immutable publication, rebuildable cognition, replayable failures, and evidence-
gated public claims.

---

# Part II — Mission, bets, goals, non-goals, and baselines

## 7. Mission

Build the world's most trustworthy, economical, performant, and agent-native open system for
turning owner-authorized drone video into durable, metrically honest, uncertainty-bearing,
semantically queryable digital twins, while preserving exact evidence and making every important
claim explainable, replayable, and revisable. The system SHALL present itself to an operating agent
as one coherent question-driven control substrate rather than a collection of media, geometry,
semantic, storage, and device components.

## 8. Leapfrog bets

### BET-001 — One evidence universe

Capture history, clocks, calibration, geometry, semantics, archive state, subscriptions, branches,
replay, and reports are projections of one ordered immutable evidence-capsule stream. There is no
mutable “master mesh” or separate truth per subsystem.

### BET-002 — Metric scale is a proof obligation

Geometry can be useful while relative. Metric units require an admitted scale witness and
uncertainty. This single rule prevents an enormous class of plausible but false digital twins.

### BET-003 — Live draft and offline convergence are one lineage

A bounded online lane publishes fast provisional geometry and coverage feedback. A deeper offline
lane consumes original media, closes loops, refines poses/depth, and publishes a successor. Both
cite the same evidence lineage; neither overwrites the other.

### BET-004 — Models propose, geometry adjudicates

Foundation models provide strong priors. Classical constraints, independent evidence, held-out
views, multi-model correlation policy, and uncertainty gates decide admission. The best model can
change without changing the truth architecture.

### BET-005 — Semantics are resolved from an evidence graph

Objects and utilities pass through explicit observation, hypothesis, resolution, rejection, and
indeterminate states. Language and segmentation are contributors. Geometry, dimensions, topology,
multi-view persistence, counterevidence, ontology-specific rules, and human confirmation can all
matter.

### BET-006 — Coverage makes missing knowledge explicit

The twin contains not just reconstructed surfaces but a certificate of what was seen, at what
quality, from which views, and with what detectability. This supports honest absence claims and
next-best-view capture planning.

### BET-007 — Immutable content addressing makes large evidence economical

Original video, analysis media, geometry tiles, reports, and repair symbols are immutable objects
with deduplication, temperature tiers, resumable replication, and provider-independent restore.
Derived assets can be rebuilt; irreplaceable originals receive stronger custody.

### BET-008 — Determinism is a product feature

Stable ordering, canonical codecs, exact model profiles, decision cards, replay identity, virtual
time, and fault schedules make failures reproducible and results auditable. Nondeterminism is
classified and bounded rather than ignored.

### BET-009 — Agent-native is an architectural constraint

Compact anchored observations, stable schemas, progressive results, explanations, idempotent
plans, obligations, continuations, and narrow MCP tools minimize token and retry cost. Agents never
receive hidden omniscience or arbitrary low-level command authority.

### BET-010 — Privacy scope travels with evidence

Detailed home geometry is sensitive. Local-first execution, least-privilege capture, private
coordinates, redaction, remote-model prohibition by default, archive controls, and publication
receipts are part of the model rather than UI settings.

### BET-011 — Questions are the cognitive control plane

Mission intent becomes objectives; objectives raise questions; questions expose evidence deficits;
and capture, computation, verification, publication, or operator steps compete by their expected
ability to change those questions. A subsystem cannot claim the agent's attention merely because
it has work available.

### BET-012 — One Agent Turn Packet

Every success, progress event, and error carries one anchor-bound orientation spine projecting
semantic status, nullable stable error details, typed minimum-safe recovery, the Decision Frame,
and the world, epistemic, work, and system ledgers (`INV-052`). The agent does not reconstruct
protocol state or retry safety from scattered commands, transport acknowledgements, logs, handles,
or dashboards.

## 9. Primary goals

### GOAL-001 — Source-agnostic exact ingest

Accept original files, recorded preview, and admitted live streams while preserving exact bytes,
source profiles, complete range accounting, timestamps, and gaps.

### GOAL-002 — Fast useful feedback

Provide bounded-latency preview, image-quality, provisional geometry, and coverage suggestions
without waiting for a converged offline reconstruction.

### GOAL-003 — High-quality converged geometry

Use original evidence, multiple priors, robust refinement, fusion, topology, and held-out
validation to produce accurate visible-surface geometry and useful exports.

### GOAL-004 — Honest metric scale

Represent relative, estimated, witnessed, and surveyed scale explicitly. Prevent metric outputs
when evidence is insufficient and propagate uncertainty into measurements.

### GOAL-005 — Semantic digital twin

Resolve a practical home/property ontology including visible structure, openings, access, terrain,
vegetation, and selected utility assets with provenance and counterevidence.

### GOAL-006 — Explicit incompleteness

Quantify coverage, detectability, occlusion, dynamic content, image quality, geometry uncertainty,
and unresolved semantic regions. Suggest additional capture rather than inventing certainty.

### GOAL-007 — Durable and economical custody

Retain exact originals locally, replicate selected classes to B2/R2 or compatible stores, resume
transfers, verify readback, use repair material where valuable, and prove restore.

### GOAL-008 — Crash and cancellation correctness

After device, process, host, disk, database, network, or cloud failure, distinguish completed,
failed, cancelled, pending, and indeterminate work without duplicating effects or corrupting roots.

### GOAL-009 — Agent efficiency

Let an agent inspect, search, explain, reconstruct, measure, archive, export, and request more
capture with bounded responses and few round trips.

### GOAL-010 — Deterministic diagnosis

Given a compatible build, artifact set, and doctor bundle, reproduce core state transitions,
policies, plans, and eligible outputs or precisely classify bounded numeric nondeterminism.

### GOAL-011 — Closed, safe, auditable Rust core

Keep the production semantic trust domain in safe Rust under one runtime and a narrowly admitted
dependency universe. Isolate unavoidable media/model/GPU/vendor stacks as processes.

### GOAL-012 — Continual reinterpretation

Allow old captures to benefit from new calibration, geometry, semantic, and compression methods
through branches and successor generations without losing history or provenance.

### GOAL-013 — First-try agent operation

Capabilities, profiles, schemas, legal affordances, maturity boundaries, valid values, errors, and
repairs are self-describing. A capable agent should reach the correct read/propose path on its first
attempt without reading implementation source or deliberately causing failures.

### GOAL-014 — Evidence-gated accretion

Repeated operation should reduce tokens, redundant capture, compute, retries, and operator effort
without weakening proof quality. Episodes, surprises, actual-versus-predicted costs, shadow
comparison, canary policy epochs, monitoring, and rollback make improvement auditable.

## 10. Explicit non-goals

### NONGOAL-001 — Autonomous flight in the initial architecture

FDGR initially observes media and advises a manual pilot. Sending flight-control commands requires
a separate flight-safety architecture, regulatory analysis, device certification, geofencing,
collision-risk model, human override, and effect qualification.

### NONGOAL-002 — Bypassing vendor controls

The project will not bypass pairing, authentication, encryption, account access, DRM, regulatory
region restrictions, or another person's system. Owner authorization does not erase protocol or
safety boundaries.

### NONGOAL-003 — Survey/legal certainty by default

A camera-based twin is not automatically a land survey, legal boundary record, code inspection,
or engineering certification. Surveyed status is limited to an explicit domain and process.

### NONGOAL-004 — Inferring hidden utilities as observed facts

Exterior imagery cannot establish unseen pipe, wire, duct, tank, panel, or interior equipment
routes. The scene graph preserves unknown and inferred states.

### NONGOAL-005 — One-model monoculture

No model family, checkpoint, vendor, or framework becomes the permanent architecture. Models are
replaceable proposal producers.

### NONGOAL-006 — One-vendor capture lock-in

DJI Flip is the motivating profile, not the data model. Generic files and cameras are first-class;
other drones and capture systems can implement the same source contracts.

### NONGOAL-007 — Cloud requirement

Every essential capture, reconstruction, query, report, and restore function works local-first.
Cloud is an optional replica/collaboration tier.

### NONGOAL-008 — Photorealism as geometry proof

A compelling render, Gaussian splat, texture, or novel view is not evidence of metric accuracy,
surface completeness, or topology.

### NONGOAL-009 — Hidden omniscience for agents

Agents see compact evidence-backed projections. They do not receive fabricated complete state,
arbitrary filesystem/network/device access, or implicit authority from text.

### NONGOAL-010 — General-purpose media/robotics platform

FDGR owns the drone-video-to-digital-twin workflow. It is not a general video editor, SLAM
middleware framework, drone fleet controller, GIS server, or generic graph database.

## 11. Baseline comparison

| Baseline | Useful strength | Missing for FDGR's mission |
|---|---|---|
| Photogrammetry desktop tool | Mature offline reconstruction and export | Live/source adapter, immutable evidence lineage, agent protocol, semantic claims, archive proof |
| Foundation-model demo | Very fast dense geometry | Stable metric scale, long-sequence state, refinement, evidence custody, failure/recovery, licensing |
| SLAM/robotics stack | Online pose and map | Consumer-video ingestion, original archive, high-quality offline convergence, home ontology, agent economics |
| Drone mapping SaaS | Convenient upload and reports | Local-first privacy, open weights, provider independence, exact provenance, source/model branches |
| VLM video analysis | Rich labels and summaries | Geometric identity, multi-view resolution, metric location, coverage, absence proof |
| Point-cloud/mesh file | Portable geometry | Source evidence, clocks/calibration/scale, uncertainty, semantics, history, repair, replay |

FDGR does not need to beat every specialist immediately. It must compose their strongest ideas
without inheriting their hidden authority gaps.

---

# Part III — Constitutional semantics

## 12. Invariant registry

`registries/invariants.toml` is normative. The initial invariants are:

| ID | Rule |
|---|---|
| INV-001 | Every claim is anchored to immutable evidence and policy. |
| INV-002 | Original evidence is append-only. |
| INV-003 | All history derives from one version universe. |
| INV-004 | Requests never observe mixed generations. |
| INV-005 | Metric claims require scale witnesses. |
| INV-006 | Model output is a proposal until validated. |
| INV-007 | Cognition cannot dispatch effects. |
| INV-008 | Acknowledgement is not completion. |
| INV-009 | Publications reserve, materialize, verify, then publish. |
| INV-010 | Cancellation drains and reconciles. |
| INV-011 | No work is orphaned. |
| INV-012 | Deterministic decisions replay. |
| INV-013 | Negative/absence claims require witnesses. |
| INV-014 | The last verified original is not discarded for a derivative. |
| INV-015 | External executables are untrusted sidecars. |
| INV-016 | Secrets never enter evidence. |
| INV-017 | Local qualification is release authority. |
| INV-018 | FDGR Rust crates forbid unsafe code. |
| INV-019 | The dependency universe is closed. |
| INV-020 | Adaptivity cannot weaken proof gates. |
| INV-021 | Cloud is not authoritative before verification. |
| INV-022 | Appearance products are not canonical geometry. |
| INV-023 | Agent memory is advisory. |
| INV-024 | Critical assets require elevated evidence. |
| INV-025 | Device sessions require owner authorization. |
| INV-026 | Coordinate conventions are explicit. |
| INV-027 | Clock discontinuities create epochs. |
| INV-028 | License admission is per artifact. |
| INV-029 | Published numeric domains are finite and bounded. |
| INV-030 | Authority narrows across seams. |

A feature that cannot satisfy an invariant is rejected or moved behind a non-authoritative research
boundary. The invariant is not weakened to accommodate the feature.

## 13. One version universe

### 13.1 Evidence capsule

The append unit is an immutable `EvidenceCapsule`. A conceptual capsule contains:

```text
capsule_id
parent_capsule_id or lineage root
capture_lineage
capture_epoch
sequence
source_profile_root
clock_epoch
calibration_epoch
policy_root
privacy_scope_root
payload kind and root
wall/monotonic observation metadata
producer identity
validation receipt root
```

Payload kinds include source-object admission, byte range, packet batch, decoded-frame batch,
telemetry batch, clock update, calibration, model proposal, geometric constraint, geometry
publication, semantic observation, claim transition, archive receipt, repair event, human
confirmation, and qualification receipt.

Capsules are immutable and totally ordered within a lineage. Cross-lineage relationships are
explicit edges, not timestamp guesses.

### 13.2 Root publication

The current lineage root is a small atomically published pointer to an immutable manifest. The root
names the complete reachable evidence history or checkpoint plus successor chain. Derived
subsystems publish their source high-water mark. A reader pins one root and cannot silently advance
mid-request.

### 13.3 Branches

A branch is an O(1) manifest over shared immutable evidence plus branch-local proposal/claim
capsules. Uses include:

- compare geometry models or calibration choices;
- run a human-corrected semantic interpretation;
- test a new fusion or uncertainty policy;
- build a redacted/shareable twin;
- try a repair or migration;
- reproduce a historical release.

Branch merge is semantic. Exact replay, stable-key composition, registered commutativity, explicit
ordering with re-proof, or rejection are allowed. Raw bytes and last-writer-wins are forbidden.

## 14. Identity and canonical encoding

### 14.1 Identity

Durable objects use domain-separated SHA-256 initially:

```text
ObjectId = SHA256("fdgr/<domain>/v<codec>\0" || canonical_bytes)
```

Domains distinguish raw chunks, manifests, frames, calibration, clocks, poses, depth, geometry,
uncertainty, semantics, reports, policies, models, receipts, and roots. The domain string prevents
cross-type substitution even when bytes happen to match.

SHA-256 is chosen initially for broad independent verification and stable tooling. A future hash
change requires a multi-hash migration, not reinterpretation of existing IDs.

### 14.2 Canonical codec

The durable codec MUST specify:

- byte order and integer widths;
- length and nesting limits;
- map/key ordering;
- enum tags and tombstones;
- string normalization policy;
- float width, NaN/infinity rejection, negative zero policy, and canonical quaternion sign;
- matrix/vector layout;
- coordinate and unit metadata;
- unknown-field compatibility;
- schema version and migration;
- checksum boundaries;
- test vectors in multiple independent implementations.

JSON is a public interchange and diagnostic format, not the content-addressed canonical codec.
Framework-specific tensors, pickle, safetensors metadata ordering, protobuf unknown behavior, or
Rust `Debug` output cannot define durable identity.

### 14.3 Chunking

Large objects are deterministic chunk graphs. Chunk boundaries derive from a registered fixed or
content-defined policy. Manifests contain ordered child IDs, logical offsets, lengths, media/time
ranges, and repair metadata. A different chunk policy creates a different representation root but
may reference the same logical object identity through a verified equivalence receipt.

## 15. Anchor and epoch semantics

A `ReconstructionAnchor` contains:

```text
capture_root
capture_epoch
frame_high_water
calibration_epoch
clock_epoch
policy_root
model_registry_root
evidence_root
```

The production anchor will also carry ontology, coordinate-registry, privacy-scope, compatibility,
and schema epochs as required. The compact scaffold is not the final schema.

A new capture epoch is required when continuity cannot be proved, including:

- source/application/controller restart with ambiguous session continuity;
- timestamp reset or wrap without a verified mapping;
- source stream replacement or unknown crop/format change;
- restore or replay into a different lineage;
- packet gap beyond policy that prevents exact successor interpretation;
- incompatible adapter profile;
- secret/session transition whose effects on source identity are unknown.

A new calibration or clock generation may occur within the same capture epoch when the transition
is explicit and old frames remain interpretable. Jobs pin exact generations.

## 16. Coordinate frames and transforms

### 16.1 Frame registry

At minimum FDGR distinguishes:

- encoded image pixel frame;
- decoded/cropped image pixel frame;
- camera optical frame;
- gimbal frame;
- aircraft body frame;
- local reconstruction/world frame;
- building frame;
- geodetic frame where authorized;
- export frames such as glTF/OpenGL, three.js, ROS, or GIS.

Every transform states parent, child, direction, handedness, axes, units, timestamp/validity domain,
and uncertainty. Names like `pose` or `extrinsics` without direction are forbidden in durable
schemas.

### 16.2 Canonical rotation and numeric policy

The coordinate registry freezes:

- matrix storage order;
- quaternion component order and canonical sign;
- active versus passive interpretation;
- right/left multiplication;
- camera forward/up conventions;
- pixel center convention;
- normalized coordinate definition;
- distortion model and coefficient order;
- rolling-shutter row timing;
- finite range and normalization tolerances.

Inputs that cannot be converted without ambiguity are quarantined. Convenient framework defaults
never leak across the boundary implicitly.

## 17. Clock and timestamp semantics

### 17.1 Clock domains

FDGR may observe:

- container PTS and DTS;
- packet arrival monotonic time;
- decoder emission time;
- display/screen-capture time;
- aircraft telemetry time;
- controller time;
- mobile OS monotonic and wall clocks;
- host monotonic and wall clocks;
- file modification/creation times as weak metadata.

Each timestamp names a clock domain. Wall time is never used as a substitute for ordering when a
monotonic source exists.

### 17.2 Clock model

A clock mapping is a piecewise model with:

```text
source clock
reference clock
valid source/reference intervals
offset and drift parameters
residual distribution
outlier policy
basis observations
confidence / uncertainty
model generation
```

Clock updates do not rewrite old events. Discontinuity creates a new segment or epoch. The frame
ledger records missing, duplicate, reordered, decoded-but-rejected, and synthesized frames. A
clean sequential frame index is derived, never assumed.

### 17.3 Time correctness claim

`CLAIM-TIME-001` requires complete packet/decode accounting and bounded clock residual for the
claimed domain. A video that plays smoothly may still fail the claim if frames or timestamps were
silently repaired.

## 18. Calibration semantics

Calibration includes:

- focal length/principal point and image dimensions;
- distortion model and coefficients;
- crop, stabilization, digital zoom, rotation, and resampling transforms;
- rolling-shutter readout and exposure timing where relevant;
- camera-to-gimbal and gimbal-to-body transforms;
- temperature/focus/firmware/profile validity scope;
- calibration method, fixtures, residuals, and uncertainty.

Vendor specifications are priors, not complete calibration. Model-predicted intrinsics are
observations. EXIF is evidence with known failure modes. Calibration may be refined jointly with
poses, but the published result names its basis and gauge constraints.

A live preview and original recording generally require separate calibration profiles even when
they originate from the same physical camera.

## 19. Metric scale witness semantics

### 19.1 Scale states

```text
RelativeOnly  projective/relative geometry; metric output prohibited
Estimated     a weak/model/metadata prior proposes scale
Witnessed     independent registered evidence establishes a metric transform
Surveyed      a survey-grade process establishes scale/frame for a named domain
```

State is scoped. A measured facade marker may witness one connected exterior reconstruction but not
an unconnected interior branch. A geodetic translation can be uncertain while local metric scale
is strong.

### 19.2 Witness families

Candidate witness types:

- measured fiducial board or coded target;
- manually measured segment with endpoints resolved in geometry;
- measured camera baseline or controlled motion fixture;
- admitted aircraft/controller telemetry with an explicit error model;
- cross-sensor registration to LiDAR/depth/survey data;
- known calibration object with a narrow dimension distribution;
- survey control points;
- multi-witness fusion under independence/correlation policy.

A “standard door,” “typical propane tank,” model-predicted metric depth, GNSS altitude, and
barometer may provide estimates but are not automatically witnessed. Their scope and uncertainty
must be justified empirically.

### 19.3 Conflict and demotion

Witnesses can disagree. The scale resolver records residuals, dependence class, robust fit,
rejected observations, and counterevidence. A later calibration correction or contradictory survey
can invalidate a previously published transform and every dependent metric claim. History remains;
a successor claim is published with the reason.

## 20. Claim model

### 20.1 Claim lifecycle

```text
Observation
→ Hypothesis
→ Resolved
↘ Rejected
↘ Indeterminate
```

Resolution is per claim family. A frame can be a valid observation while its interpretation is
rejected. A geometry surface can be admitted relative but blocked from metric measurement. A
remote object can exist but remain unverified as a replica.

### 20.2 Claim components

A durable claim contains:

```text
claim_id and type
subject and spatial/temporal scope
basis anchor
supporting evidence roots
counterevidence roots
policy and registry roots
producer and method
uncertainty / confidence with semantics
current disposition
terminal predicate or blocking reasons
supersession links
human confirmation where required
```

Confidence is not a universal probability. A detector score, geometric residual, uncertainty
interval, human rating, and archive checksum are different quantities and remain typed.

### 20.3 Major claim families

The initial registry includes:

- exact original retained;
- frame timeline continuous for a declared domain;
- calibration admitted;
- pose generation admitted;
- metric scale witnessed;
- surface geometry admitted;
- semantic asset resolved;
- asset absent from a covered domain;
- cloud replica retrievable;
- digital twin release qualified.

Each has a distinct terminal predicate. They cannot be averaged into one readiness number.

### 20.4 Negative evidence

Negative evidence records what failed or was ruled out:

- unsupported DJI profile;
- packet parser rejection;
- insufficient overlap;
- unstable calibration;
- failed loop closure;
- metric scale conflict;
- model nondeterminism outside envelope;
- surface accuracy gate failure;
- semantic confusion or counterexample;
- insufficient coverage for absence;
- cloud readback mismatch;
- cancellation leak;
- privacy or license block.

Negative evidence remains visible after later success. It defines qualification scope and prevents
future agents from repeating invalid assumptions.

---

# Part IV — Trust, authority, intents, effects, and obligations

## 21. Three-plane architecture

### 21.1 Authoritative evidence plane

This plane owns:

- canonical identities and codecs;
- evidence capsules, roots, anchors, epochs, and lineage;
- exact raw object custody and source range accounting;
- clock, calibration, coordinate, scale, ontology, model, policy, privacy, and compatibility
  registries;
- intent, plan, witness, idempotency, capability, lease, and fencing state;
- claim transitions and counterevidence;
- publication, archive, retention, repair, restore, and deletion obligations;
- completion receipts, doctor bundles, and qualification evidence.

It does not run a neural model because truth is not an inference runtime. It records model requests,
proposal outputs, validation, and admitted claim consequences.

### 21.2 Reconstruction and cognition plane

This plane owns pinned projections and computations:

- image quality, dynamic masks, keyframes, features, tracks, correspondences;
- pose/depth/point/ray proposals;
- pose graphs, loop candidates, bundle refinements;
- surfel, TSDF, occupancy, point, mesh, topology, texture, and appearance branches;
- coverage/detectability maps and next-best-view candidates;
- semantic observations, grouping, relations, and scene-graph projections;
- search, ranking, reports, explanations, analytics, and counterfactuals.

It can submit `CandidateClaim` or `PublicationIntent`. It has no path to device I/O, archive write,
delete, authoritative root publication, capability grant, or policy mutation.

### 21.3 Device and effect plane

This plane owns narrowly scoped external interactions:

- source file and block reads;
- controller/app/network/capture-device reads;
- process spawn, pipes, signals, wait, descendant cleanup;
- filesystem staging and fsync;
- cloud upload/download/list/head/delete attempts;
- OS keychain/secret capability use;
- operation lookup, reconciliation, compensation, and cleanup.

It cannot define canonical identity or decide semantic success. It receives a short-lived effect
ticket naming exact request, basis, authority, destination, budget, idempotency key, and terminal
predicate.

## 22. Trust domains

### 22.1 Safe-Rust semantic domain

FDGR crates use `#![forbid(unsafe_code)]`. This domain includes the pure semantic core, canonical
codec, ledger interfaces, geometry validation, policies, claims, scene model, agent domain APIs,
archive manifests, and deterministic lab adapters.

### 22.2 External process domain

ffmpeg, ffprobe, Python/PyTorch, CUDA/Metal libraries, model-specific code, vendor utilities, and
GPU drivers are untrusted. They run out of process with bounded manifests. A process crash is an
expected failure mode. A process exit status is an observation.

### 22.3 Device/vendor domain

Aircraft, controllers, mobile applications, firmware, vendor clouds, radio links, pairing, and
account sessions are external. FDGR uses documented or owner-authorized observation paths. It does
not link vendor native code into the semantic process by default.

### 22.4 Archive/provider domain

Object providers, HTTP/TLS stacks, networks, credentials, bucket policy, lifecycle rules, and
remote metadata are external. Provider receipts are evidence requiring independent verification.

### 22.5 Untrusted content domain

Video bytes, containers, codec metadata, filenames, EXIF, telemetry, packet frames, OCR, captions,
model outputs, report source, MCP arguments, and documentation are untrusted data. Text cannot
become executable authority.

### 22.6 Human domain

A human may grant capabilities and confirm an observation, measurement, or semantic claim. Human
input is versioned evidence with identity and scope. It is not exempt from contradiction,
migration, privacy, or provenance requirements.

## 23. Capability system

### 23.1 Capability classes

The initial authority lattice includes:

| Capability | Permitted authority |
|---|---|
| `CAP-CAPTURE-READ` | read named source files/streams within a sealed scope |
| `CAP-CAPTURE-LIVE-OBSERVE` | open a read-only live source session for an admitted profile |
| `CAP-DEVICE-OBSERVE` | run bounded compatibility/read probes |
| `CAP-DEVICE-CONTROL` | reserved; not admitted in the initial architecture |
| `CAP-PROCESS-MEDIA` | spawn exact admitted media profiles |
| `CAP-PROCESS-MODEL` | spawn exact admitted model profiles |
| `CAP-EVIDENCE-APPEND` | append validated capsule types to a lineage |
| `CAP-GENERATION-PUBLISH` | publish a verified immutable root |
| `CAP-ARCHIVE-READ` | retrieve named immutable objects |
| `CAP-ARCHIVE-WRITE` | create named immutable remote objects |
| `CAP-ARCHIVE-DELETE` | apply an independently sealed deletion plan |
| `CAP-PRIVACY-EXPORT` | produce a named lower-sensitivity derivative |
| `CAP-HUMAN-CONFIRM` | attach a scoped human confirmation receipt |
| `CAP-ADMIN-POLICY` | modify registries/policies through a separately audited path |

Capabilities are typed, attenuable, time/budget/scope-limited, and carried through `Cx`. A read
capability cannot be cast to write. A model worker receives no publication or archive capability.
Transport identity grants no domain capability.

### 23.2 Caveats

Capabilities can carry caveats such as:

- capture lineage and source path/device;
- read-only method set;
- compatibility profile;
- spatial region and time interval;
- privacy class;
- object prefix and maximum bytes;
- model ID and artifact root;
- output schema and destination staging root;
- deadline and CPU/GPU/memory/network/storage budgets;
- human confirmation requirement;
- no-network/no-delete/no-geolocation restrictions.

The planner MUST enforce caveats before expansion and effect dispatch, not after results exist.

## 24. Intent, plan, commit, and proof

### 24.1 Why commands are insufficient

“Reconstruct this video,” “archive the twin,” and “find the propane tank” are semantic requests.
Their implementation may touch thousands of objects and long-running external work. Directly
executing a command string makes retries, stale state, budgets, privacy, and completion ambiguous.

FDGR uses:

```text
semantic intent
→ compile against pinned anchor
→ read/write/negative witnesses
→ immutable plan and digest
→ capability/risk/privacy/budget validation
→ optional lease/fence/checkpoint
→ commit-time revalidation
→ short-lived effect tickets
→ observed external outcomes
→ authoritative validation/publication
→ terminal predicate proof
```

### 24.2 Intent

An intent declares desired semantic outcome and policy, not implementation strings. Examples:

```text
ImportOriginalMedia(source, capture_profile)
StartLiveObservation(device_profile, capture_policy)
BuildGeometry(anchor, quality_profile, scale_policy)
ResolveAsset(anchor, ontology_concept, spatial_scope)
ReplicateRoot(root, archive_policy)
ExportTwin(root, format, privacy_policy)
DeleteEvidence(scope, retention_policy)
```

### 24.3 Witnesses

The planner records:

- exact read roots and generations;
- source byte/frame/time domains;
- calibration, clock, coordinate, scale, model, ontology, and policy identities;
- absence or completeness predicates and their domains;
- destination and retention state;
- leases/reservations over publication names, devices, archive keys, or human review tasks;
- privacy and license preconditions;
- resource estimates and operation-cost basis.

Witness exhaustion may reject safe work. It MUST NOT authorize unsafe work.

### 24.4 Plan digest

The plan is canonical and immutable. Its digest covers:

```text
intent
basis anchor
witnesses
selected algorithms/model profiles
resource and cancellation policy
effect sequence and idempotency keys
publication and rollback strategy
terminal predicates
privacy/license/retention constraints
```

A commit supplies the digest, not mutable arguments. A changed intent produces another plan.

### 24.5 Commit-time validation

Before any effect, commit checks:

- anchor and registry compatibility;
- witness freshness and conflicts;
- capability caveats and lease/fence incarnation;
- model/dependency/license admission;
- source/destination identity;
- disk/cloud capacity and privacy scope;
- budget feasibility;
- checkpoint requirement;
- idempotency and prior effect lookup;
- cancellation state.

Rebase recompiles the intent. It does not edit the old plan's anchor.

## 25. Effect lifecycle

The effect registry is normative. Every effect distinguishes:

```text
planned
reserved
request_durable
submitted
accepted_by_adapter_or_process
output_observed
output_structurally_verified
semantic_postcondition_verified
published_or_reconciled
terminal
```

Not every effect uses every state, but states are never collapsed merely for UI convenience.

### 25.1 Media/model process effect

A sidecar effect is complete only when:

- exact executable/profile was invoked;
- input basis matches the plan;
- child and descendants are reaped;
- stdout/stderr and outputs respect bounds;
- output files are closed and digested;
- schemas and finite numeric policy pass;
- temporary paths are published or retired;
- no unresolved effect remains.

A zero exit code can still produce invalid output. A killed process may have produced valid staged
children but cannot publish them until validation completes.

### 25.2 Cloud effect

An upload is complete as an effect when provider lookup establishes the intended immutable object
or a typed failure. The stronger `CLAIM-ARCH-001` additionally requires provider-independent
readback and digest verification. A timeout after sending the final request is indeterminate, not
safe to blindly retry under a mutable key.

### 25.3 Publication effect

Publishing a root is irreversible as history, even when a successor can supersede it. Therefore
root identity and complete child closure are materialized off the critical section. The commit
coordinator only validates and advances the root pointer. It never performs model inference or
large encoding work while holding publication authority.

## 26. Idempotency

Every effectful semantic operation has an idempotency identity derived from session/intent/plan and
explicit caller key. Rules:

- same key + same canonical request returns the existing operation or terminal result;
- same key + different request fails with conflict;
- retry of a known failed pre-effect operation may create a new attempt under the same operation;
- retry of an indeterminate effect performs lookup/reconciliation before dispatch;
- immutable content-addressed writes may be naturally idempotent only after content and destination
  identity are verified;
- model/ffmpeg recomputation may be cacheable, but a cache hit still validates exact profile and
  basis.

Idempotency records are durable before potentially ambiguous effects.

## 27. Obligations

Long-running work is represented as durable obligations with:

```text
obligation_id
owner session/region
plan digest and anchor
state and progress high-water
child operations
budgets and deadline
cancellation/compensation policy
terminal predicate
last evidence and heartbeat
recovery strategy
result or blocking reason
```

Examples:

- drain a live capture and seal all gaps;
- transcode a 90-minute original;
- complete offline reconstruction;
- verify a 500 GB remote object graph;
- wait for human confirmation;
- execute a restore drill;
- repair a missing/corrupt child;
- delete a retained graph after references clear.

MCP Tasks may project obligations, but the application-owned obligation engine remains authoritative.

## 28. Cancellation

Cancellation is a semantic protocol:

1. record cancellation request and reason;
2. reject new child effects;
3. notify cooperative children;
4. signal/terminate noncooperative sidecars according to policy;
5. drain pipes, channels, readers, and writers;
6. reconcile device/cloud/process outcomes;
7. publish valid partial evidence only if the plan permits a partial generation;
8. quarantine or retire invalid staging;
9. preserve indeterminate effects for later lookup;
10. emit progress or terminal cancellation receipt;
11. close the region only when owned work reaches quiescence or explicit indeterminate boundary.

A timeout is a cancellation trigger, not evidence that cleanup succeeded.

## 29. Leases and fencing

Leases prevent concurrent agents or sessions from racing over scarce or exclusive resources:

- one live controller/device session;
- one publication name/branch head;
- one archive deletion plan;
- one compatibility probe with side effects;
- one human review assignment;
- one mutable local staging namespace.

Each lease has an incarnation/fence token. External adapters validate the current fence at effect
boundaries where possible. Expiration does not erase an in-flight effect; reconciliation remains
necessary.

---

# Part V — Capture, source adapters, media, clocks, and calibration

## 30. Capture subsystem overview: `SIGHTLINE`

`SIGHTLINE` is the logical capture subsystem. It contains source adapters, byte/packet accounting,
clock alignment, media probing/decoding, image-quality observations, calibration attachment, and
capture-health publication. It does not own geometry truth.

The adapter contract is source-neutral:

```text
probe(cx, candidate) -> CompatibilityObservation
open(cx, admitted_profile, capture_ticket) -> SourceSession
read(cx, session, bounded_request) -> SourceBatch
lookup(cx, operation_id) -> EffectObservation
close(cx, session) -> DrainReceipt
```

A source batch may contain exact bytes, packets, frames, telemetry, metadata, and gaps. The
semantic core converts validated batches into capsules.

## 31. Source profile

A `SourceProfile` contains:

- source kind: file, directory, removable media, application export, screen capture, HDMI/UVC,
  documented network stream, research network stream;
- device/application/controller identities and versions;
- authorization scope without raw secret material;
- container/codec/resolution/rate/color/crop/stabilization configuration;
- timestamp and telemetry capabilities;
- calibration applicability;
- reconnect and discontinuity behavior;
- known overlays, transformations, and limitations;
- compatibility probe evidence;
- profile status: admitted, degraded, research, unsupported, unknown.

Profiles are immutable. Updated evidence creates a successor profile.

## 32. DJI compatibility laboratory

The DJI program MUST be profile-driven, read-only first, and reproducible.

### 32.1 Probe classes

- device and controller enumeration through documented surfaces;
- version and capability introspection;
- original-media discovery/import;
- application/controller recording characterization;
- capture-card/UVC/HDMI characterization;
- local network endpoint and traffic observation on the operator's own session;
- stream container/codec/configuration/keyframe discovery;
- timestamp, latency, gap, reconnect, app background/foreground, and power-cycle behavior;
- secret leak scanning and redaction verification.

### 32.2 Protocol research rules

- No arbitrary packet injection is exposed.
- No bypass of encryption/authentication/pairing is implemented.
- Raw captures default to private local custody.
- Parsers are built from captured fixtures and state machines, not ad hoc offsets.
- Every parser field records confidence/source and tolerates unknown extensions.
- Malformed, truncated, reordered, duplicated, and adversarial frames are fuzzed.
- Firmware/app/profile scope is explicit; no “DJI Flip protocol” blanket claim is permitted.
- Useful releases cannot require protocol research success.

### 32.3 Live-source quality

A live adapter reports:

```text
bytes/packets received
sequence gaps, duplicates, reorder
codec configuration epochs
keyframe intervals and missing references
frame decode acceptance/rejection
source and host clock mapping
latency distribution
resolution/crop/format changes
buffer pressure and dropped work
reconnect state
```

It MUST NOT hide gaps by renumbering frames or synthesizing continuity without an explicit derived
operation.

## 33. Exact original-media import

Original import is the first production capture lane.

### 33.1 Import phases

```text
probe path/device under capability
→ enumerate candidates without following unauthorized links
→ snapshot metadata observations
→ reserve object identities and staging
→ stream bytes through bounded hashing/chunking
→ account for exact source ranges and read errors
→ fsync/verify local objects
→ run non-authoritative media probe
→ publish raw object and source receipt
→ optionally schedule normalization and archive
```

Source names, file times, and filesystem metadata are observations, not identity. The exact byte
content defines the raw object.

### 33.2 Partial or damaged files

A partial source can be valuable evidence. FDGR stores present ranges and explicit holes. It does
not pad missing bytes and claim an original. Repair or alternate-source equivalence is a separate
claim.

### 33.3 Duplicate detection

Exact duplicate bytes deduplicate naturally. Near-duplicate or rewrapped media remain separate
objects unless a deterministic equivalence analysis proves frame/pixel/timeline relationships. A
smaller app export is not deduplicated against the aircraft original based on perceptual similarity.

## 34. ffmpeg/ffprobe sidecar protocol

### 34.1 Why process, not FFI

ffmpeg provides indispensable codec/container coverage but brings C memory safety, global process
state, threading, native dependency, and release complexity. FDGR supervises an exact executable
profile rather than linking libav*.

### 34.2 Request manifest

A media-worker request covers:

```text
request and plan IDs
executable object/version/profile
input object roots and byte ranges
expected container/stream observations
exact arguments in normalized form
allowed environment and working directory
output contracts and maximum bytes/files
CPU/memory/I/O/wall budgets
pipe/file mode
network disabled
cancellation and kill escalation policy
```

The sidecar receives no archive or device credentials.

### 34.3 Output manifest

The supervisor records:

- process start/exit and descendant-drain receipt;
- stdout/stderr bounded digests and selected structured diagnostics;
- observed executable version/build configuration;
- input read high-water and broken-pipe state;
- output paths, sizes, hashes, stream metadata, and frame counts;
- resource usage;
- cancellation/failure classification;
- validation results and quarantine reason.

### 34.4 Deterministic profiles

Bit-identical encoded media may not be portable across every codec/build/hardware path. FDGR
classifies profiles:

- `STRICT`: byte-identical output required for exact executable/hardware profile;
- `SEMANTIC`: decoded-frame/timeline equivalence required within a frozen numeric policy;
- `BEST_EFFORT`: preview only, no durable geometry basis until independently validated.

Hardware encoders are not used for authoritative analysis media until their determinism and decode
equivalence are characterized. Original bytes remain available regardless.

## 35. Rendition classes

### 35.1 Exact original

Irreplaceable source. Never rewritten.

### 35.2 Analysis mezzanine

Optimized for deterministic random access and reconstruction. Candidate properties:

- all-intra or short closed GOP;
- stable pixel format and color conversion;
- no scaling unless calibration/crop transform is explicit;
- frame mapping to original PTS/DTS and source packet ranges;
- high quality or mathematically lossless where economically justified;
- chunk boundaries aligned to independent decode units.

The exact codec is a measured policy, not constitutional. A sequence of lossless still frames may
be preferable for some workloads; compact intra codecs may win for others.

### 35.3 Low-latency preview

Small, bounded-bitrate, fast-decode stream for humans/agents and online geometry. It may be lossy
and incomplete. Its claim scope is explicit.

### 35.4 Compact archive rendition

AV1, HEVC, or future codecs may substantially reduce cost. This rendition is derived and cannot
replace the last verified original. It is useful for quick retrieval and secondary analysis after
decode-equivalence measurement.

### 35.5 Keyframes and pyramids

Selected frame objects, thumbnails, image pyramids, masks, and feature products are derived from a
named decode/calibration policy. They remain connected to original frame/time/range evidence.

## 36. Packet, frame, and telemetry ledger

The ledger distinguishes:

- encoded packets and configuration units;
- decoded frame surfaces;
- accepted analysis frames;
- displayed preview frames;
- screen-captured frames;
- original recording frames;
- telemetry samples and interpolations;
- gaps, duplicates, reorder, corruption, and rejected units.

Each frame identity covers source object, stream/configuration epoch, original timestamps, decode
policy, crop/rotation/color transformation, and pixel object root. Two identical pixel arrays with
different times or sources are different observations but may share a pixel payload object.

## 37. Image-quality observations

Per-frame and regional observations include:

- blur and motion direction;
- exposure clipping, low light, flicker, white-balance/color shifts;
- compression blocking/ringing and corruption;
- texture/feature density and repetitiveness;
- sky, water, glass, reflective metal, foliage, dynamic person/vehicle/animal fractions;
- rolling-shutter risk and angular velocity proxy;
- occlusion and field-of-view overlap;
- expected ground sampling distance from current scale estimate;
- semantic target visibility.

Quality observations feed keyframe and coverage policies. They do not mutate frames.

## 38. Calibration acquisition and refinement

Calibration sources are ranked but not blindly trusted:

1. controlled FDGR calibration fixture and procedure;
2. exact manufacturer/device profile validated against fixtures;
3. embedded metadata with consistency checks;
4. joint multi-view refinement with gauge and priors;
5. model-predicted intrinsics;
6. generic product specification.

The calibration engine can branch alternative models and compare held-out reprojection, straight
line/plumb-line behavior, stability, and geometry impact. Crop/stabilization transforms are
estimated separately from physical lens distortion where possible.

## 39. Live-to-original alignment

A live preview can guide online geometry while the aircraft original arrives later. Alignment uses
visual correspondences, clock priors, frame fingerprints, and codec/crop transforms to produce a
`SourceAlignment`:

```text
preview frame/time interval
original frame/time interval
spatial image transform or camera relationship
residual and ambiguity
matched/unmatched domains
alignment generation
```

Online claims remain based on preview evidence. Offline reconstruction may reuse semantic targets
or capture decisions only through explicit alignment and revalidation against original frames.

---

# Part VI — Geometry reconstruction: `ATLAS`

## 40. Reconstruction postures

FDGR has two coordinated postures over the same evidence lineage.

### 40.1 Online draft

Purpose:

- detect whether capture is geometrically useful;
- maintain a rough trajectory and surface model;
- expose coverage and uncertainty gaps quickly;
- recognize likely semantic targets that need closer views;
- suggest additional manual capture before the flight ends.

Properties:

- bounded window/state and predictable GPU/CPU/memory;
- tolerant of missing frames and temporary model failure;
- explicit `provisional` status;
- may operate on live preview rather than original media;
- publishes frequent immutable draft generations or deltas;
- never upgrades relative scale or critical semantic claims without witnesses.

### 40.2 Offline converged

Purpose:

- consume exact original evidence and complete context;
- revisit keyframes, matches, loops, calibration, scale, and dynamic masks;
- optimize globally or hierarchically;
- fuse high-quality geometry and topology;
- validate on held-out frames and ground truth where available;
- publish a release-candidate twin with complete evidence receipts.

Properties:

- may use more expensive models and multiple branches;
- deterministic or bounded-nondeterministic profile;
- checkpointable and resumable;
- able to supersede, not overwrite, the online branch;
- refusal is an acceptable result when evidence is insufficient.

## 41. Reconstruction anchor and job contract

A job request pins:

```text
reconstruction anchor
spatial/time/source domains
calibration and clock generations
coordinate and scale policy
keyframe policy
allowed model profiles
reference/learned algorithm policy
quality and completeness profile
resource budgets
determinism profile
privacy and license scope
publication destination branch
```

The job reads only complete immutable inputs. Incremental online jobs may consume exact successor
deltas whose basis matches their state high-water. A gap, source epoch transition, incompatible
calibration, or hidden-state mismatch forces reinitialization or a new branch.

## 42. Deterministic keyframe selection

Keyframes determine geometry quality, model cost, storage, and agent latency. The policy balances:

- temporal spacing;
- visual overlap and parallax;
- camera motion and baseline;
- blur/exposure/compression quality;
- feature and texture diversity;
- estimated view direction and surface incidence;
- dynamic-region fraction;
- semantic target visibility;
- coverage novelty;
- loop-closure value;
- model window size and GPU budget;
- source quality class.

### 42.1 Reference policy

The first reference selector is deterministic and understandable:

1. reject structurally invalid or catastrophically poor frames;
2. retain mandatory boundary/configuration frames;
3. compute stable scalar observations over a fixed image pyramid;
4. maintain temporal and visual-distance thresholds;
5. score coverage/quality novelty using quantized fixed-policy terms;
6. resolve ties by canonical frame identity;
7. emit selected IDs plus rejected reasons and decision-path digest.

Learned selection may later propose candidates but must compete against and explain gains over the
reference.

### 42.2 Set-cover interpretation

Offline selection can formulate a weighted set-cover/submodular objective over view/scene/semantic
coverage with cost. Approximation policy, tie-breaks, budget, and complexity witness are explicit.
The output is a proposal; geometry validation can request additional frames if the selected set
fails.

## 43. Reference feature, match, and track pipeline

A simple classical path is essential as:

- a no-model degraded mode;
- a differential oracle for learned correspondences;
- a source of interpretable residuals and failure diagnoses;
- a geometry constraint independent of training priors;
- a bootstrap for loop closure and calibration.

The exact detector/descriptor may evolve. The semantic requirements are:

- deterministic preprocessing and feature ordering;
- bounded features per spatial cell/frame;
- descriptor identity and distance policy;
- symmetric matching, ratio/ambiguity tests, and stable tie-breaks;
- geometric verification under registered camera models;
- track union with conflict detection rather than naïve transitive merging;
- per-observation provenance and residual history;
- dynamic/semantic masks applied as explicit filters, not destructive edits;
- no hidden random RANSAC seed.

### 43.1 Robust estimation

RANSAC-like methods use deterministic seed/ordering or exhaustive bounded schedules under Lab.
Every result reports:

```text
model family
minimal sample policy
candidate ordering
inlier threshold and units
iterations / stopping proof
inlier IDs and residuals
degeneracy tests
alternative hypotheses
```

A homography-dominant planar sequence, pure rotation, weak baseline, repetitive facade, or rolling-
shutter stress is classified rather than forced into a generic essential-matrix success.

## 44. Model-worker geometry proposals

### 44.1 Sealed request

A model worker receives:

- exact model artifact/profile roots;
- read-only input frame/pyramid/calibration/pose-prior objects;
- a named crop, resize, normalization, and batching policy;
- coordinate/output schema;
- finite bounds and maximum output sizes;
- deterministic seed/profile where supported;
- GPU/CPU/memory/wall budgets;
- network disabled;
- an unpublished output directory.

The worker cannot read arbitrary source paths, secrets, archives, or unrelated captures.

### 44.2 Proposal bundle

A proposal bundle may contain:

- intrinsics/extrinsics or camera encoding;
- depth, inverse depth, point maps, world/camera points;
- rays and visibility/covisibility;
- correspondences/tracks/features;
- confidence or uncertainty-like values;
- masks or dynamic/static scores;
- text-alignment/semantic embeddings;
- hidden-state checkpoint for a streaming model;
- worker diagnostics.

FDGR rewraps all output into canonical objects. Framework file names and tensor serialization are
not durable identities.

### 44.3 Validation ladder

1. schema and declared shape/dtype;
2. finite values, bounds, normalized rotations, invertible calibration;
3. exact input frame and crop correspondence;
4. coordinate and transform-direction tests;
5. output determinism envelope;
6. internal geometric consistency;
7. reprojection against source pixels;
8. comparison to independent tracks/poses/depth;
9. domain/failure classifiers;
10. admission as observation/proposal, never direct publication.

A model may be highly useful while failing strict determinism. Its bounded numeric profile and
output quantization/canonicalization policy must then be explicit.

## 45. Pose graph

The pose graph contains camera/keyframe states, relative constraints, priors, loop candidates,
calibration parameters, scale variables, and robust-loss metadata.

### 45.1 Constraint sources

- classical two-/multi-view geometry;
- foundation-model relative/absolute pose proposal;
- feature/track reprojection;
- visual place recognition and loop closure;
- telemetry orientation/position/velocity priors;
- gimbal/body transforms;
- ground or gravity priors where justified;
- measured fiducial/segment constraints;
- source-alignment constraints between preview and original;
- optional external survey/LiDAR alignment.

Each constraint names its evidence, units, covariance/weight semantics, correlation class, and
validity scope. A neural confidence is not silently converted to Gaussian covariance.

### 45.2 Gauge and connectedness

The graph declares its gauge:

- projective/relative frame;
- arbitrary similarity frame;
- gravity-aligned local frame;
- metric local building frame;
- geodetic frame.

Disconnected components remain separate. The optimizer cannot manufacture a transform between
components with no evidence. A digital twin manifest may include multiple components with explicit
relationships `unknown`, `estimated`, or witnessed.

### 45.3 Loop closure

Loop candidates are proposals. Acceptance requires geometric verification and consistency with the
current graph. Large corrections trigger:

- relinearization/reoptimization;
- dependent depth/fusion invalidation or branch rebuild;
- changed coverage/semantic associations;
- a decision card explaining accepted/rejected loop evidence.

False loops on repetitive windows, siding, roof shingles, or vegetation are a required adversarial
campaign.

## 46. Bundle and local refinement

The refinement engine optimizes registered variables under explicit numeric policy. Potential
variables include camera poses, points/inverse depths, intrinsics, distortion, rolling-shutter
parameters, scale, and selected rigid-object motions.

### 46.1 Residual families

- feature reprojection;
- dense photometric or learned correspondence consistency;
- depth/point-map consistency;
- epipolar/line/plane constraints;
- telemetry and scale priors;
- loop closure;
- regularization and marginalization priors.

Residuals carry units and robust-loss identity. Weighting cannot be ad hoc model-score mixing.
Correlated residual families use explicit normalization or covariance policy.

### 46.2 Determinism and numeric policy

The reference solver prioritizes stable results over peak speed:

- canonical variable and residual ordering;
- fixed initialization and damping policy;
- deterministic linear algebra path where feasible;
- bounded iterations and termination reason;
- finite checks after each update;
- canonical quaternion sign and gauge normalization;
- stable reduction order or compensated summation;
- decision card for rejected observations and adaptivity.

Optimized SIMD/GPU solvers may enter later with scalar/reference fallback and an equivalence
envelope. They cannot silently change minima, outlier classification, or tie-breaks.

### 46.3 Marginalization and streaming state

Online sliding-window refinement publishes marginalization priors as explicit state objects with
basis and numeric policy. Dropping old states is a derivation, not forgetting. Offline replay can
recompute without the marginalization approximation.

## 47. Depth and correspondence ensemble

FDGR may combine:

- model depth/point maps;
- stereo/multi-view depth from refined cameras;
- sparse track triangulation;
- plane/line/edge structure;
- telemetry or scale priors;
- repeated passes and source qualities.

### 47.1 Correlation-aware fusion

Multiple models trained on overlapping data or sharing architecture are not independent. The model
registry assigns lineage/correlation classes. Ensemble uncertainty uses conservative aggregation,
held-out calibration, disagreement, and geometric residuals rather than averaging confidence maps.

### 47.2 Depth observation contract

Each depth sample or tile names:

```text
source frame/pixel domain
camera/calibration/pose generation
scale state and units
producer/model/profile
validity mask
confidence/uncertainty semantics
occlusion and dynamic flags
min/max range and finite policy
supporting/counter evidence
```

Depth from a relative model remains relative even if the tensor happens to contain numbers near
meters.

### 47.3 Edge and thin-structure policy

Depth smoothing can erase window frames, railings, wires, gutters, spigots, and branches. The
fusion policy preserves discontinuity evidence and reports unresolved thin structures. Semantic
masks can guide but not force geometry.

## 48. Dynamic and non-Lambertian regions

Required categories include:

- people, animals, cars, moving equipment;
- foliage and water;
- sky;
- transparent/reflective windows;
- specular metal;
- shadows and exposure changes;
- screens and changing displays;
- temporary objects.

The system stores dynamic/non-geometric observations and excludes or separately models them. It
must not erase source evidence or assume every rejected pixel is empty space. A window may be a
surface with uncertain depth and a semantic opening relationship rather than a reliable textured
plane.

## 49. Geometry fusion

### 49.1 Authority ladder

The canonical geometry candidate is a layered representation, not one file:

1. camera/track/constraint graph;
2. per-view depth/point/ray observations;
3. fused surfels or equivalent oriented samples with uncertainty;
4. occupancy/free-space evidence;
5. TSDF or signed-distance tiles where evidence supports them;
6. mesh/topology extracted from fused evidence;
7. presentation point clouds/textures/LOD;
8. appearance-only Gaussian splats or neural render assets.

The manifest marks each representation `canonical_geometry`, `derived_geometry`, or
`appearance_only`.

### 49.2 Spatial tiling

Large twins are partitioned into deterministic spatial tiles under a named coordinate frame and
resolution hierarchy. Tile manifests include:

- bounds and level;
- source observation ranges;
- point/surfel/voxel/triangle counts;
- uncertainty summaries;
- coverage and dynamic masks;
- child roots;
- neighbor/overlap relationships;
- geometry and semantic high-water marks.

Content addressing allows unchanged tiles to be shared across generations.

### 49.3 Surfel fusion

A surfel may carry position, normal, radius, color statistics, observation count, view-angle
statistics, time span, uncertainty, dynamic probability, and evidence links. Update order is
canonical or batch-reduced deterministically. Conflicting surfaces remain multimodal or trigger
split/rejection rather than averaging through walls.

### 49.4 TSDF/occupancy

Signed-distance and free-space inference requires known camera rays, depth uncertainty, truncation,
and occlusion policy. Unknown is distinct from free. A detector saying “no object” does not create
free space. Rays through transparent windows or depth-invalid sky require special handling.

### 49.5 Mesh and topology

Mesh extraction reports:

- source geometry root and algorithm policy;
- manifold/nonmanifold edges;
- holes and boundary loops;
- self-intersections and degeneracies;
- components and tiny artifacts;
- normal consistency;
- simplification error and protected semantic edges;
- uncertainty/coverage projection to vertices/faces;
- LOD relationships.

Hole filling is a derived hypothesis. Filled surfaces are tagged inferred and cannot masquerade as
observed geometry.

### 49.6 Appearance assets

Textures, panoramas, Gaussian splats, and neural renders improve inspection. They cite the geometry
and frame basis and receive privacy controls. They never witness scale or surface topology.

## 50. Geometry validation

### 50.1 Internal validation

- finite, bounded coordinates and valid transforms;
- connectedness/gauge classification;
- reprojection residual distribution;
- track length and triangulation angle;
- loop consistency;
- depth cross-view consistency;
- free-space/surface contradictions;
- calibration stability;
- scale witness residuals;
- mesh topology and extraction diagnostics;
- deterministic output identity or numeric envelope.

### 50.2 Held-out views

A subset of frames is excluded from fitting according to a deterministic policy. Geometry renders
or projects into held-out views, and residuals are measured. Held-out validation is not perfect—
models may share priors and frames may be correlated—but it is stronger than evaluating only on
fitted observations.

### 50.3 Ground truth

Qualification corpora use survey/LiDAR/reference geometry, measured segments, or synthetic exact
truth. Metrics include trajectory, depth, surface distance, normal, completeness, topology,
scale, dimensions, and uncertainty calibration. Claims are scoped to the corpus and profile.

### 50.4 Refusal

A reconstruction can terminate as:

- `qualified` for a named profile/domain;
- `partial` with explicit connected components/coverage;
- `relative_only`;
- `provisional`;
- `blocked` by calibration/scale/coverage/privacy/license;
- `failed` with typed reason;
- `indeterminate` after ambiguous external effect.

Producing no mesh is better than publishing a false metric twin.

## 51. Uncertainty

### 51.1 Types

FDGR distinguishes:

- aleatoric/image ambiguity;
- epistemic/model disagreement;
- pose and calibration covariance/approximation;
- scale transform uncertainty;
- surface spatial uncertainty;
- occupancy/free-space confidence;
- coverage/detectability;
- semantic presence and identity uncertainty;
- source/time alignment uncertainty;
- numerical/reproducibility envelope.

These are not collapsed into one confidence heatmap.

### 51.2 Calibration

Uncertainty is calibrated on held-out and ground-truth corpora. The system reports coverage of
prediction intervals, selective risk, and error versus declared uncertainty. Foundation-model
confidence maps may be features; they are not accepted as calibrated uncertainty without evidence.

### 51.3 Propagation

Measurement uncertainty accounts for selected geometry uncertainty, pose/calibration/scale
uncertainty, point/region selection, and method. Semantic dimensions inherit geometry generation
and scale witness. Reports display intervals and blocking conditions.

## 52. Measurements

A measurement request specifies:

```text
anchor and geometry generation
coordinate frame
scale requirement
selection: points, surface regions, asset, opening, path
method: Euclidean, geodesic, fitted primitive, area, volume, clearance, slope
robust/outlier policy
uncertainty/confidence output
```

The result includes exact selection evidence and replay command. Snapping to semantic edges is
explicit. A fitted rectangle around a window is a model with residuals, not the window's ground
truth dimensions.

## 53. Incremental reconstruction

The online and repeated-capture future requires incremental maintenance:

- new frames update quality, tracks, pose graph, coverage, and provisional fusion;
- loop closures may invalidate large dependent regions;
- calibration/scale changes trigger dependency-aware recomputation;
- unchanged spatial tiles are reused by content identity;
- semantic observations reproject when geometry changes;
- indexes and reports consume high-water deltas;
- subscriptions state completeness and lag.

The reference implementation may recompute fully. Incremental algorithms are admitted only after
full-recompute equivalence and invalidation tests.

---

# Part VII — Semantic digital twin: `LANTERN`

## 54. Ontology principles

The ontology is practical, observable, and evidence-aware. It distinguishes physical assets from
views, hypotheses, and functions. Initial concept families include:

### 54.1 Structure

```text
building
facade / wall region
roof / eave / gutter / downspout
foundation / slab / retaining wall
window / exterior door / garage opening
porch / deck / balcony / railing
stairs / ramp / landing
fence / gate
```

### 54.2 Access and terrain

```text
driveway / walking path / lawn / garden bed
slope / step / curb / drainage channel
parking area / entrance / route segment
```

### 54.3 Utilities and equipment

```text
hvac.outdoor_condenser
hvac.heat_pump_outdoor_unit
fuel.propane_tank
water.exterior_spigot
water.meter_or_access
water.well_head
sewer.cleanout
utility.electrical_meter
utility.service_drop
utility.service_entry
utility.generator
communications.service_entry
```

The ontology MUST distinguish visible exterior evidence from hidden system components. An outdoor
condenser does not prove an indoor air handler. A meter/service drop does not reveal the hidden
main panel or conductor route. A spigot does not reveal pipe routing.

### 54.4 Vegetation and context

```text
tree / trunk / canopy
shrub / hedge / garden
water body
neighboring structure
temporary object
vehicle
person / animal as dynamic privacy-sensitive class
```

Personal attributes are out of scope.

## 55. Semantic observations

Observation producers include:

- Qwen or another admitted VLM over selected frames/crops/video;
- segmentation/tracking masks;
- OCR and symbol/text recognition;
- geometry primitives, dimensions, and topology;
- detector/classifier models;
- metadata or asset inventory import;
- human annotation/confirmation;
- change detection across captures.

Each observation names frame/time, mask/bounds/rays, geometry association, concept candidates,
attributes, producer/profile, prompt or deterministic rule identity, uncertainty semantics, and
support/counterevidence.

Natural-language explanations are derived from structured observations. They are not parsed back
into authority.

## 56. Multi-view association

The resolver projects masks/rays into geometry and groups observations using:

- spatial overlap and surface association;
- camera/view consistency;
- appearance and embedding similarity;
- track identity;
- dimensions and shape;
- attachment/topological relations;
- temporal persistence;
- mutual exclusivity and ontology constraints;
- counterevidence and occlusion.

A 2D box from one frame is not a 3D asset. Association can remain multimodal when evidence supports
multiple locations or identities.

## 57. Asset hypothesis

An `AssetHypothesis` includes:

```text
hypothesis_id
ontology candidates
spatial extent distribution
supporting observations
counterevidence
geometry generation and associations
attributes with individual evidence
relations to building/terrain/assets
resolver policy and decision path
required missing evidence
status
```

Hypotheses are useful to agents and capture planning. They are visibly different from resolved
assets.

## 58. Resolution gates

Each concept has its own gate. Examples:

### 58.1 Window/door/opening

Potential evidence:

- persistent multi-view mask/edges;
- facade-plane opening geometry;
- depth discontinuity/recess;
- repeated rectangular structure;
- handle/frame/glass/door semantic cues;
- topology and access relation;
- dimensions within a broad plausible range.

Reflective siding, painted rectangles, screens, and shadows are counterexamples.

### 58.2 HVAC outdoor equipment

Potential evidence:

- multi-view persistent equipment volume;
- fan grille/coil/cabinet cues;
- ground/wall placement and clearance;
- line-set/electrical disconnect observations where visible;
- dimensions and geometry;
- VLM/detector agreement;
- human confirmation for high-confidence operational use.

The resolved concept remains exterior equipment, not the entire HVAC system.

### 58.3 Propane tank

Potential evidence:

- cylindrical tank geometry and supports;
- valve/dome/regulator observations;
- location/clearance context;
- multiple views and dimensions;
- signage or human confirmation;
- confusion checks against horizontal compressors, barrels, water tanks, or trailers.

### 58.4 Electrical service

Separate concepts:

- overhead service drop;
- meter enclosure;
- service entry point/conduit;
- exterior disconnect;
- hypothesized indoor main-panel relation.

Critical resolution requires multi-view evidence and usually human confirmation. The system must
avoid publishing a precise safety-relevant location to a lower-privacy export unless policy allows.

### 58.5 Exterior spigot

Potential evidence:

- small protruding fixture/valve geometry;
- wall association and height;
- multi-view close capture;
- hose/handle cues;
- confusion checks against vents, cable entries, cleanouts, or shadows.

Thin/small asset resolution requires appropriate ground sampling distance and detectability.

## 59. Counterevidence

The resolver actively searches for evidence against a hypothesis:

- alternative concept with better geometry/context;
- inconsistent location across views;
- mask not attached to the hypothesized surface;
- impossible dimensions after witnessed scale;
- object disappears under improved view/crop;
- observation lies on reflection/texture rather than geometry;
- temporal change indicates movable object;
- later original-media frame contradicts live-preview interpretation;
- human rejection;
- ontology relation conflict.

Resolution receipts include the strongest counterevidence considered, not only supporting examples.

## 60. Human confirmation

Human confirmation is a typed evidence event:

- exact asset/hypothesis and anchor;
- views/geometry presented;
- confirmation question and allowed answers;
- reviewer identity/scope;
- answer, uncertainty, and notes;
- timestamp and policy;
- whether confirmation establishes concept, location, dimensions, function, or merely plausibility.

A human can be wrong. Later evidence can supersede a confirmation. Critical-asset policies may
require confirmation from the property owner or qualified reviewer without encoding professional
credentials as model authority.

## 61. Absence and completeness

### 61.1 Absence is domain-scoped

Valid forms:

- “No qualifying exterior spigot observation was found on the covered north and east facade
  regions under detectability profile X.”
- “No propane-tank-sized cylindrical asset is visible in the authorized exterior coverage domain.”

Invalid overreach:

- “The property has no spigot.”
- “The house does not use propane.”
- “There is no electrical main panel.”

### 61.2 Absence witness

`CLAIM-ABS-001` requires:

```text
authorized spatial/semantic domain
coverage certificate
minimum resolution/view/lighting/detectability profile
occlusion and inaccessible-region accounting
model/rule search receipt
known failure modes and counterexamples
no qualifying observation
```

A detector returning zero results without coverage is not evidence of absence.

## 62. Scene graph

The scene graph connects assets, regions, geometry, access, utilities, observations, claims, and
provenance. Example relations:

```text
part_of
attached_to
supported_by
adjacent_to
above / below / inside / outside
opens_into
accessed_via
connected_to_visible
serves_hypothesized
occludes
observed_from
projected_to
supported_by_evidence
contradicted_by
supersedes
```

Relations distinguish observed, derived, hypothesized, and resolved status. Graph algorithms and
queries preserve deterministic order and anchor high-water.

## 63. Semantic query

Queries can combine:

- ontology concept and disposition;
- spatial region or relation;
- time/capture generation;
- geometry/scale/uncertainty bounds;
- evidence producer and model profile;
- human confirmation;
- coverage and detectability;
- supporting/counterevidence;
- provenance path length;
- archive/availability state.

Examples:

```text
show resolved exterior openings on the south facade with witnessed dimensions
show all observations supporting the electrical service entry hypothesis
show critical assets lacking human confirmation
show surfaces within 2 m of a walking path whose uncertainty exceeds 5 cm
show unresolved utility hypotheses that need another oblique close view
why was the west-wall spigot hypothesis rejected?
```

Results include anchors, status, uncertainty, completeness, and explanation. Search may rank them;
exact query semantics remain separate.

## 64. Change over time

Repeated captures create temporal asset and geometry histories:

- unchanged observation/asset with new evidence;
- moved, added, removed, occluded, or seasonally changed asset;
- construction/maintenance change;
- calibration/model-only reinterpretation;
- uncertain correspondence across captures.

Change claims distinguish physical change from reconstruction or semantic-policy change. A new
model detecting an old asset is not evidence the asset appeared recently.

---

# Part VIII — Coverage and manual capture guidance: `COMPASS`

## 65. Coverage model

Coverage is a first-class spatial evidence product. It estimates whether a surface/region/asset was
observed sufficiently for a named task.

### 65.1 Coverage cells

Surface/space is partitioned into deterministic cells or samples. Each records:

- geometry/region identity;
- supporting frames and rays;
- observation count and temporal/source diversity;
- view-angle distribution;
- baseline/parallax distribution;
- projected pixel density / ground sampling distance;
- blur/exposure/compression quality;
- occlusion and dynamic fraction;
- depth/pose uncertainty;
- semantic detectability profile;
- privacy/authorization constraints;
- uncovered/unknown/free-space distinction.

A cell can be geometrically covered but semantically insufficient for a small spigot. Coverage is
claim-specific.

### 65.2 Pre-geometry coverage

Early online capture lacks a stable surface. The system uses camera-frustum, image-overlap,
provisional depth, and view-sphere bins. This is provisional and later reprojected to converged
geometry.

### 65.3 Coverage certificate

A certificate names:

```text
anchor and geometry generation
spatial domain
coverage task/profile
cell policy and resolution
quality/detectability thresholds
covered, partial, unknown, excluded areas
supporting frames
uncertainty and limitations
certificate root
```

A certificate is required for completeness and absence claims.

## 66. Next-best-view proposals

Candidate capture actions aim to reduce uncertainty or satisfy semantic/geometry coverage. They may
optimize:

- new surface coverage;
- triangulation baseline;
- incidence angle diversity;
- loop closure;
- resolution/detail for a target asset;
- occlusion relief;
- scale/calibration witness visibility;
- route/time/battery cost;
- operator safety, privacy, and no-fly exclusions.

### 66.1 Candidate form

```text
semantic objective
suggested viewpoint region and camera orientation
not a flight command
estimated information gain and assumptions
surfaces/assets affected
route/time/battery estimate
safety/privacy exclusions
evidence basis
score components and counterfactual
```

The initial system presents human-readable guidance: “move to the northeast corner and capture the
north facade obliquely,” not motor commands.

### 66.2 Optimization

Potential methods include greedy submodular selection, set cover, graph search, visibility analysis,
and learned ranking. Deterministic tie-breaks and complexity witnesses are required. Candidate
scores do not grant device authority.

## 67. Capture profiles

Configurable profiles include:

- rapid exterior overview;
- high-quality facade/roof/terrain geometry;
- utility asset inspection;
- entrance/accessibility/path survey;
- before/after change capture;
- privacy-restricted redacted capture;
- calibration/scale target capture.

Profiles specify desired overlap, speed/angular motion, view diversity, detail, target ontology,
quality thresholds, source settings, budgets, and exclusions. They are targets, not universal
physical guarantees.

## 68. Operator interface

The live interface SHOULD show:

- source health and frame gaps;
- current provisional trajectory/map;
- quality warnings;
- covered/uncertain/unseen regions;
- semantic target observations and unresolved hypotheses;
- scale/calibration status;
- highest-value next views;
- battery/time/storage budget;
- whether original recording is active and verified later;
- explicit degraded state when live geometry/model is unavailable.

Warnings are prioritized and rate-limited. The operator should not be buried in raw model output.

---

# Part IX — Evidence storage, archive, repair, and recovery: `ORIGIN` + `VAULT`

## 69. Storage roles

FDGR separates:

- **ledger:** small transactional metadata, capsules, claims, obligations, roots, receipts;
- **CAS:** immutable large objects and manifests;
- **staging:** unpublished work owned by an operation;
- **derived indexes/cache:** rebuildable search, graph acceleration, thumbnails, model caches;
- **replicas:** verified local/remote copies and repair material;
- **exports:** policy-scoped derivatives with their own manifests.

No single SQLite blob file, directory tree, cloud bucket, or graph database is the entire truth.
The published root connects roles.

## 70. Local content-addressed store

### 70.1 Object classes

```text
raw_source_chunk
logical_source_manifest
packet/frame/timeline object
calibration/clock/transform object
model artifact and proposal
geometry tile / mesh / uncertainty / coverage
semantic/scene graph segment
report/export
receipt/doctor/qualification
repair symbol
```

### 70.2 Object header

A conceptual header includes:

```text
magic and codec version
domain/type
logical length
compression/encryption policy
child count or chunk policy
payload digest
header checksum
flags and bounds
```

The object identity covers canonical logical content or the exact encoded representation according
to domain. Compression/encryption transformations are explicit wrappers, not invisible mutations.

### 70.3 Staging and publication

Staging paths are capability-scoped and operation-owned. The publisher validates:

- child existence and digest;
- manifest canonicality;
- object/type/size/depth/count bounds;
- no path traversal or unauthorized links;
- durability policy;
- no missing temporary write;
- root identity;
- ledger plan/witness/fence;
- cancellation state.

Then it advances the branch/root pointer atomically and records the receipt.

## 71. FrankenSQLite adapter

The reference ledger defines semantics. The FrankenSQLite adapter eventually stores:

- capsule headers/order and root pointers;
- object metadata and reachability summaries;
- source/frame/clock/calibration indexes;
- intents, plans, witnesses, idempotency, leases, effects, obligations;
- claim state and evidence relationships;
- archive/retention/repair state;
- derived-work queues and high-water marks.

Large blobs remain in CAS. Transactions do not call ffmpeg, models, networks, or fsync large object
graphs inside the commit coordinator. Materialization occurs first; a short commit validates and
publishes identities.

## 72. FrankenFS adapter

FrankenFS provides capability-rooted paths/devices, snapshot/COW semantics, crash-evidence events,
repair plans, and durability transitions. The adapter cannot change FDGR identity or publication
semantics. A reference staged-filesystem implementation remains the differential oracle.

## 73. Archive classes and policies

Suggested temperature classes:

| Class | Contents | Default custody |
|---|---|---|
| `irreplaceable` | exact originals, calibration/scale fixtures, human confirmations | strongest local + verified remote + optional repair |
| `authoritative` | ledgers, roots, clock/calibration, admitted geometry/semantics, receipts | durable local + verified remote |
| `warm_analysis` | mezzanine, keyframes, features, proposal bundles | local/remote by recompute cost |
| `preview` | low-latency media, thumbnails, temporary online drafts | cacheable, rebuildable |
| `model_artifact` | exact admitted weights/code environments | verified local cache + source/redistribution policy |
| `derived_export` | glTF, point cloud, reports, redacted products | policy-dependent |

Retention is graph-aware. Objects referenced by retained roots or obligations cannot be collected.

## 74. S3-compatible replication

### 74.1 Provider-neutral keying

Keys derive from object identity and domain, for example:

```text
fdgr/v1/objects/<domain>/<aa>/<bb>/<digest>
fdgr/v1/roots/<lineage>/<branch>/<sequence>-<digest>
```

Object keys are immutable. Branch-head/root pointers use a separate conditional-update protocol or
append-only marker chain. Content objects never depend on provider version IDs for identity.

### 74.2 Multipart protocol

```text
reserve upload obligation
→ choose registered part size within provider profile
→ upload parts with per-part digest and idempotency
→ list/reconcile parts after failure
→ complete multipart
→ lookup object metadata
→ read back full object or deterministic samples under policy
→ verify FDGR digest
→ publish verified replica receipt
```

Provider-specific limitations are encoded in compatibility profiles. Re-uploading a part number,
minimum part size, checksum fields, same-key write rate, abort lifecycle, and object-lock support are
not assumed uniform.

### 74.3 Checksums

FDGR computes its own complete logical/representation digest. Provider checksums and ETags are
additional observations. The archive adapter records exactly which checksum semantics were used.

### 74.4 Credentials

Archive credentials are narrow capabilities with endpoint, bucket, prefix, methods, expiration,
and byte limits. They are never serialized into plans, evidence, model manifests, or support
bundles. Environment variables are a bootstrap mechanism, not the final secret architecture.

## 75. ATP and RaptorQ

Once admitted, ATP can move immutable object graphs across local hosts and archive gateways:

- resumable symbol transfer;
- path racing and bonded donors;
- repair symbols for damaged/missing chunks;
- manifest-first or child-first scheduling under policy;
- receiver validation before root publication;
- bandwidth/storage budgets and cancellation.

RaptorQ protection is policy-driven. It is most valuable for irreplaceable originals, manifests,
and difficult-to-recompute geometry, not every disposable thumbnail. Repair symbols are content-
addressed objects with their own custody and verification.

ATP MUST NOT carry device-control authority or become a generic effect RPC. It moves immutable
state/evidence.

## 76. Encryption

Encryption policy may include:

- local volume/filesystem encryption outside FDGR;
- client-side object encryption with content identity over plaintext and a separate ciphertext
  representation identity;
- provider-side encryption observations;
- per-capture or per-privacy-domain keys;
- key rotation through new wrappers/manifests, not rewriting plaintext identity;
- ability to verify/decrypt during restore drills.

Key material is not content-addressed evidence. Losing a key can make a replica unavailable even
when bytes exist; that state is explicit.

## 77. Repair

`fdgr doctor` scans bounded metadata and objects and produces findings. `fdgr repair plan` creates a
sealed plan describing:

- basis root and current retention obligations;
- missing/corrupt/unverified objects;
- available local/remote replicas and repair symbols;
- exact reads/writes/deletions;
- expected resulting roots/replica state;
- budgets and privacy/provider capabilities;
- plan digest.

`repair apply` revalidates the basis and seal. It never writes different bytes under an existing
identity. If remote state is ambiguous, it reconciles before mutation.

## 78. Restore and disaster recovery

A restore drill starts from a declared loss scenario:

- loss of derived indexes only;
- loss of local CAS with ledger surviving;
- loss of ledger with root/checkpoint surviving;
- loss of one remote provider;
- corrupt subset of chunks;
- lost staging plus indeterminate upload;
- complete local host loss.

The drill reconstructs retained roots, validates object closure/digests, opens the ledger/history,
rebuilds derived indexes, renders reports, and checks selected media/geometry/semantic claims. The
receipt records bytes read, missing objects, repair used, duration, and resulting root identity.

A backup that has never restored does not earn `CLAIM-ARCH-001` or release readiness.

## 79. Retention and deletion

### 79.1 Mark graph

Deletion planning traverses:

- retained root manifests and branches;
- legal/owner retention policies;
- active obligations and idempotency records;
- replica/repair dependencies;
- reports/exports whose provenance must remain valid;
- model/license artifacts required for replay;
- privacy deletion requests.

### 79.2 Deletion protocol

```text
request scope and reason
→ compute candidate graph cut
→ identify blockers and surviving reconstructibility
→ human/critical approval where policy requires
→ seal plan
→ revalidate basis
→ delete local/remote objects with lookup/reconciliation
→ rebuild reachability/replica state
→ verify requested privacy effect and surviving roots
→ publish tombstone/deletion receipt
```

`archive.delete` is a critical capability. Garbage collection does not bypass this protocol merely
because objects are unreachable from the current branch; historical/retained roots matter.

---

# Part X — Agent cognitive and control substrate: `FABRIC` + `HELM`

This part is the system's center of gravity. Capture, geometry, semantics, storage, transfer,
qualification, and presentation exist to make this loop more truthful, economical, safe,
replayable, and accretive. No subsystem may expose a competing lifecycle or require the agent to
join unrelated status models.

## 80. Driver-seat contract

After every success, progress event, or error, an agent MUST be able to answer:

1. What is known to be true at the exact current anchor?
2. What changed since the anchor I acknowledged?
3. What matters now and which objectives are affected?
4. What is uncertain, contradicted, stale, uncovered, or indeterminate?
5. What work is active, blocked, awaiting confirmation, draining, or reconciling?
6. Which semantic actions are currently expressible and why are others unavailable?
7. Which next protocol step has the greatest expected value per total control cost?
8. What evidence and terminal predicate would prove it complete?
9. What did previous attempts teach, with what applicability and confidence?
10. What compact capsule would let another agent resume safely?

The optimization target is decision quality per total control cost:

```text
tokens + canonical reads + derived queries + graph/search operations
+ CPU/GPU time and peak memory + source/network/storage bytes
+ wall time + operator attention + flight time + risk + recovery burden
```

Saving tokens by omitting continuity, counterevidence, active indeterminate work, or safety state is
not economy.

## 81. One operating loop and abstraction tower

Every public operation is a view or transition in:

```text
bootstrap → orient → focus → inspect → formulate → propose → compare
          → commit → watch → verify/reconcile → learn → handoff/resume
```

The abstraction tower is:

```text
L9  Campaign / mission / policy
L8  Objective graph
L7  Question graph, uncertainty, coverage, evidence deficits
L6  Candidate plans, counterfactual branches, decision cards
L5  Obligations, effects, progress, verification, reconciliation, surprise
L4  Scene claims, assets, measurements, topology, coverage certificates
L3  Constraint fabric: clocks, calibration, scale, tracks, poses, depth, factors
L2  Observation capsules and immutable history
L1  Content-addressed objects, custody, transfer, repair, restore
L0  External devices, processes, filesystems, providers, and operators
```

Each object names typed links upward and downward. Higher levels compress lower-level evidence but
cannot strengthen epistemic status or authority. `INV-031` freezes the loop; `INV-034` freezes the
synchronized ledger projection.

## 82. Canonical Agent Turn Packet

Every result, including progress and errors, carries `SCHEMA-AGENT-TURN` and the machine contract in
`architecture/agent_turn_contract.json`:

```text
schema, operation, phase, session_id, turn_id, request_id
exact anchor vector and continuity
profile and focus
world, epistemic, work, and system ledgers
changes, attention, affordances, recommendations
uncertainty, coverage, budget, references, continuation
```

The four ledgers answer:

- **world:** what is established about the property;
- **epistemic:** what is not established and why;
- **work:** what is active and what proves it terminal;
- **system:** whether source, compute, model, storage, archive, privacy, and qualification machinery
  are healthy and affordable.

The packet is derived and authority-free. `turn_id` identifies presentation; `request_id` identifies
an authority-bearing semantic request when one exists. They MUST NOT be silently aliased.

Continuity states are `bootstrap`, `continuous`, `heartbeat`, `partial`, `gap`, `reset`, `stale`,
and `indeterminate`. A packet either binds one compatible anchor vector or explicitly declares why The vector conforms to `SCHEMA-ANCHOR-VECTOR`, carries one canonical `anchor_digest`, and is reused without transport-specific reduction by packets, Decision Frames, plans, obligations, episodes, handoffs, pilot cards, and spatial handles (`INV-053`, `TEST-044`).
it cannot.

## 83. Profiles, context packs, and Pack DNA

Observation profiles are stable contracts:

- `pulse`: cheapest safe heartbeat and active-work transition;
- `briefing`: cold-arrival or resume orientation;
- `tactical`: one decision, question, region, asset, or obligation;
- `pilot`: immediate manual-capture guidance tied to evidence deficits;
- `forensic`: bounded evidence-complete audit or reconciliation;
- `custom`: explicit union of registered projections under hard bounds.

A context pack is keyed by anchor, focus, grants, privacy, profile, allowed epistemic classes,
freshness, policy, and token/byte budget. The reference selector maximizes marginal decision value
under mandatory safety and continuity constraints, using deterministic submodular selection and
canonical ties.

`SCHEMA-CONTEXT-PACK` includes **Pack DNA**: mandatory items, selected items with marginal gain,
redundancy groups, omitted high-scoring items and reason, coverage gained, remaining deficits,
budget, and continuation. An agent can ask what another 500 tokens would buy instead of requesting
a blind larger dump. `INV-037` and `INV-038` prevent opaque compression and safety-dropping
truncation.

## 84. Mission, objective, question, and deficit graph

A mission declares purpose, success, forbidden outcomes, budget/risk/privacy policy, and owner. An
objective declares desired terminal predicates, hard constraints, soft utility, horizon, evidence
requirements, dependencies, conflicts, and stopping. A question is the primary unit of uncertainty:

```text
question identity and proposition
current epistemic state
supporting and contradicting evidence
coverage/detectability requirements
evidence deficits
affected objectives
candidate observations/computations
terminal predicate and stopping rule
```

The core graph is:

```text
Mission → Objective → Question → EvidenceDeficit
        → CandidateObservation | CandidateComputation | CandidateEffect
        → CandidatePlan → Obligation → Evidence → Claim/Abstention
        → ObjectiveProgress → Episode/Surprise
```

Questions may be durable or compiler-ephemeral, but every material uncertainty MUST have an
explicit terminal evidence predicate. `SCHEMA-OBJECTIVE` and `SCHEMA-QUESTION` freeze the initial
exchange form. `INV-033` prevents vague “confidence work” with no proof target.

### 84.1 Value of information

Before recommending inspection or capture, estimate:

```text
VOI = probability of changing a material decision
    × reduction in expected decision loss
    + future-control reuse and coverage value
    - observation, compute, token, storage, delay, operator, flight, and risk cost
```

VOI chooses among already admissible steps. It cannot weaken hard scale, coverage, privacy,
capability, freshness, or publication requirements.

### 84.2 Stopping

Stopping is an explicit result when terminal evidence is satisfied, remaining uncertainty cannot
change an accepted decision, marginal gain falls below policy, budget expires with an accepted
partial scope, work is proved impossible/blocked, or safety/privacy/compatibility forbids more.
The system MUST NOT capture or compute merely because resources remain.

## 85. Attention, affordances, and recommendations

`attention` answers what matters now. Each item includes consequence, expiry/review condition,
epistemic state, causal contributors, evidence, and affected objectives/questions.

An `affordance` is a currently expressible semantic action template. It includes parameter schema,
enabled/degraded/blocked state, grants, known/unresolved preconditions, compatibility, risk,
confirmation/checkpoint policy, reversibility, expected cost, question impact, and a structured
`fdgr.propose` template. It is not a promise that commit will pass.

A recommendation is a ranked **next protocol step**, not prose. It includes reason, objective and
question impact, expected utility, information value, cost interval, risk, reversibility,
prerequisites, invalidators, confidence/evidence, and confirmation. Ranking is safety-first:

1. resolve continuity, authority, or indeterminate-effect hazards;
2. prevent irreversible evidence loss or unsafe operator state;
3. satisfy explicit hard objectives/deadlines;
4. obtain high-value missing information;
5. improve expected twin utility and future control cost;
6. prefer lower risk/cost and greater reversibility when otherwise equivalent;
7. use canonical tie breaks.

Wait, stop, reconcile, ask the operator, and do nothing are valid recommendations (`INV-045`).

## 86. Candidate sets, counterfactuals, and commit

`fdgr.propose` returns `SCHEMA-PLAN-CANDIDATE` objects. Materially different safe strategies are
kept as a bounded Pareto frontier over evidence gain, terminal-predicate success, robustness,
latency, CPU/GPU/memory, bytes/storage, operator/flight cost, risk, reversibility, checkpoint and
reconciliation burden, and conflict probability.

Every candidate shares the same anchor, objective/question set, policy, grants, and budget. It
records assumptions, positive/negative/read/write witnesses, predicted delta, invalidators, and a
decision card. Counterfactual branches are immutable derived worlds and cannot dispatch.

Commit performs current witness, capability, privacy, compatibility, idempotency, lease/fence,
checkpoint, budget, and policy validation. Rebase recompiles intent. Concurrent merge uses exact
replay, stable-key composition, registered commutativity, explicit ordering with re-proof, or
rejection. Last-writer-wins is forbidden. `INV-039` freezes common-basis comparison.

## 87. Obligations, watch, verification, and reconciliation

A committed plan creates region-owned obligations projected through `SCHEMA-OBLIGATION-PROGRESS`:

```text
prepared → committed → dispatching → accepted?
         → effect_observed? → verifying
         → stable_complete | failed | cancelling/cancelled | indeterminate
```

Progress names semantic stage, processed high-water, total when known, active children, potential,
resource use, evidence produced, blockers, affected questions/objectives, and next heartbeat. It is
not a fabricated percentage.

Verification distinguishes dispatch acceptance, effect observation, postcondition satisfaction,
stability interval, and objective completion. Unknown external outcome requires operation lookup,
observation, and reconciliation; blind retry is forbidden. Cancellation is request, stop new
effects, drain, reconcile/compensate, seal evidence/progress certificate, finalize.

`INV-035` requires all active work to remain visible across context loss and errors.

## 88. Explain, query, and self-description

`fdgr.query` answers bounded exact, temporal, graph, spatial, ranked, or provenance questions and
always states coverage/completeness. `fdgr.explain` traverses any typed handle downward to evidence
and upward to consequence. It answers what was claimed, basis, producer, alternatives,
counterevidence, thresholds/ties, omissions, uncertainty, reproduction, and what would change the
result.

Context retrieval combines exact filters, lexical/semantic search, graph expansion, spatial and
temporal relevance, contradiction value, and deterministic selection. Indexes are immutable
rebuildable generations and never authoritative.

Self-description includes operation/capability/profile/schema/error/status/recovery dictionaries,
valid enum values, compatibility and maturity matrices, examples, and generated help/robot docs.
Unknown commands/flags may receive safe typo suggestions, but effectful intent is never silently
corrected. `INV-043` requires help, MCP, manifests, and qualification reality to agree.

## 89. Active perception and pilot profile

The pilot profile consumes unresolved question bundles, visibility/coverage graph, geometry and
semantic uncertainty, scale/calibration deficits, device/operator limits, obstacle/no-fly/privacy
policy, battery/time/bandwidth, and active work.

A candidate maneuver states target/framing, admissible position/orientation region, baseline,
angle, resolution, exposure/dwell, questions expected to change, information-gain interval, cost,
risk, invalidators, and good-enough/stop/abort predicates. Route planning can use set cover,
submodular selection, orienteering, shortest paths, flow, and matching with deterministic
certificates.

Guidance proposed, operator acknowledged, maneuver observed, usable evidence acquired, quality
accepted, and question resolved are separate states. `INV-044` prevents guidance from becoming an
opaque or implied flight command.

## 90. Episodes, surprise, and evidence-gated accretion

Every meaningful closed-loop attempt publishes `SCHEMA-EPISODE-CAPSULE` with starting/ending
anchors, context pack, candidates, selected plan, obligations, predictions, observations, actual
cost, outcome, evidence, surprises, and lesson candidates.

A surprise records material divergence: absent or unexpected effects, rejected geometry/semantic
hypotheses, wrong information gain, cost/latency outside interval, changed preconditions,
compensation failure, or recommendation regret. Silent prediction error is forbidden by
`INV-040`.

Learning strata are episodic, semantic, procedural, policy, and negative. Promotion is:

```text
episode → lesson candidate → independent support → confound/contradiction review
→ bounded applicability → deterministic replay → shadow evaluation
→ counterfactual/regret evidence → canary policy epoch → monitor → retain/rollback
```

Adaptive policy may allocate effort but cannot weaken hard evidence or safety gates. `INV-041`
prevents one anecdote or agent feedback item from changing production behavior.

## 91. Eleven-operation semantic narrow waist

The logical surface is:

| Operation | Purpose |
|---|---|
| `fdgr.open_session` | negotiate lineage, protocol, grants, profiles, budgets, continuity |
| `fdgr.orient` | compact four-ledger briefing and ranked attention |
| `fdgr.query` | bounded question or typed-handle expansion |
| `fdgr.propose` | compile objectives/next steps into sealed candidates |
| `fdgr.compare` | compare candidates/branches and expose Pareto frontier |
| `fdgr.commit` | revalidate and create obligations/effect tickets |
| `fdgr.watch` | semantic progress and terminal transitions |
| `fdgr.cancel` | request cancellation, drain, compensate/reconcile |
| `fdgr.explain` | decisions, claims, scores, witnesses, omissions, evidence |
| `fdgr.handoff` | create/resume sealed authority-free continuity capsule |
| `fdgr.doctor` | compatibility, health, custody, compute, policy, qualification |

CLI domain verbs compile to this waist. FastMCP Rust is a replaceable presentation and only
`fdgr-mcp` may depend on it. No arbitrary shell, ffmpeg/Python invocation, SQL/graph query string,
filesystem path, DJI packet, or vendor command is exposed to an agent.

## 92. Handoff, resume, and Eidetic memory

`SCHEMA-HANDOFF-CAPSULE` contains last acknowledged anchor, missions/objectives, question frontier,
active work and minimum safe next step, attention/uncertainty, grants/budgets, rejected options and
why, references, and digest. It is sufficient to resume but grants no lease, capability, or effect
authority (`INV-042`).

Eidetic Engine remains outside the canonical workspace as advisory campaign memory. FDGR exports
privacy-scoped episode, surprise, failure, qualification, and lesson candidates with anchors and
evidence digests. Memory can improve orientation and proposals but cannot satisfy a question,
scale witness, precondition, or completion predicate.

## 93. Multi-agent coordination and qualification

Multiple agents may read and branch concurrently. The packet exposes objective ownership,
delegation, branch visibility, semantic reservations, plan witness overlap, leases/incarnations,
fences, likely conflicts, duplicate-work opportunities, confirmations, and handoffs.

Shared evidence acquisition is optimized across question bundles. Mutation requires current
knowledge plus authority. Stale workers cannot publish after a fence advances. Merge follows the
semantic ladder; agents do not coordinate through filename locks or private prose memory.

Agent-native qualification is not schema validation alone. Required scenarios include cold
arrival, cheap heartbeat, context loss, gap/reset, unavailable affordance, equal alternatives,
budget pressure, indeterminate effect, pilot guidance, surprise, handoff/resume, multi-agent race,
self-description, and live/lab parity. Work packages `WP-033` through `WP-044`, gates `GATE-018`
through `GATE-023`, and tests `TEST-023` through `TEST-037` make this revision executable.

---

### 93.1 The singular Decision Frame

A turn packet is the common response spine; a Decision Frame is the common decision object. Every
material decision joins one compatible objective slice, focal questions, admitted facts,
epistemic debt, hard constraints, affordances, candidate Pareto frontier, active obligations,
budget, stopping rule, invalidators, and recommended next protocol steps. The agent never has to
join these from unrelated calls. A heartbeat may carry no active frame. A decision-bearing result
may not carry two competing frames.

The frame is immutable and authority-free. Evidence or policy changes publish a successor and
explicit delta; commit still requires a sealed plan plus current witnesses, capabilities, privacy,
leases, fences, and budgets. Waiting or accepting bounded uncertainty is a first-class candidate.

### 93.2 Epistemic debt and stable attention

Unresolved decision-relevant uncertainty is represented as epistemic debt, not hidden behind a
confidence number. Debt names the question, possible material answers, expected loss if ignored,
coverage/detectability, affected objectives, resolution actions, cost, review condition, and
terminal state. It terminates as resolved, consciously accepted, blocked, superseded, or
indeterminate.

Attention is a classed interrupt system over that debt. Protocol continuity and physical safety
outrank objective, efficiency, and learning signals. Deterministic ordering, hysteresis,
acknowledgement, suppression keys, expiry, and material-change re-entry prevent model/event
frequency from thrashing the agent.

### 93.3 Frame-complete spatial handles

Every public spatial reference names its anchor and branch, coordinate frame, handedness, axes,
units, origin, transform direction, scale authority, uncertainty, spatial support, temporal scope,
coverage, privacy scope, aliases, and expansion affordances. Naked points, bounding boxes, pixel
indices, and implementation-local mesh IDs are forbidden.

Anchor-bound aliases such as “north garage wall” or “uncovered strip behind the maple tree” make
human-agent communication economical while preserving canonical identity and ambiguity. Refinement
creates correspondence and supersession edges; old handles remain historically resolvable.

### 93.4 Human-agent flight protocol

Initial FDGR is a human-piloted evidence-acquisition system. Pilot guidance separates recommendation,
operator acknowledgement/refusal, observed aircraft maneuver, usable evidence, and question
resolution. One normal pilot card is foregrounded at a time, with at most two short cues, a stable
landmark, why the view matters, quality conditions, cost, done condition, and abort conditions.
Safety preemption dominates all ordinary guidance. Recommendation grants no flight-control
authority.

### 93.5 Agent-native measurement

Qualification measures cold-arrival success, context value density, question closure, unnecessary
evidence, Decision Frame sufficiency, attention stability, spatial-handle resolution, useful
evidence per pilot minute/battery, reflights avoided, semantic progress accuracy, handoff success,
first-try repair, surprise calibration, duplicate-work prevention, and repeated-workload control
cost. Each result is workload- and receipt-scoped; no aggregate “agent friendly” claim is accepted
without negative controls.

# Part XI — Rust architecture and dependency DAG

### 93.6 Canonical wire vocabulary

All public JSON, JSONL, MCP, CLI-JSON, receipt, manifest, context-pack, and model-worker surfaces
use lower `snake_case` field and enum names and `fdgr.<name>/1` payload schema identities
(`ADR-0011`, `INV-051`). Names derive from canonical registries rather than handwritten adapters.
Compatibility aliases are bounded ingress migrations only and are never emitted. Units, coordinate
frames, time bases, scale authority, privacy scope, and epistemic state remain explicit rather than
encoded into prose or implicit naming. `TEST-043` rejects drift across schemas, examples,
self-description, and transports.

## 94. Constitutional Rust profile

- Rust edition 2024;
- exact dated latest-nightly toolchain;
- `#![forbid(unsafe_code)]` in every FDGR crate;
- Asupersync as exclusive async runtime;
- explicit `&Cx` on I/O, blocking, shared-resource, effect, and long-running APIs;
- no Tokio, Rayon, reqwest, hyper, axum, tower, SQLx, ORM, C/C++ FFI, or in-process Python;
- stable machine schemas and typed errors;
- deterministic reference implementation before optimization;
- exact revision/feature/license allowlist;
- no hidden network/download during build or test.

The initial checked-in scaffold deliberately has no third-party Rust dependencies. Owned siblings
are added only as their work-package gates arrive.

## 95. Logical subsystems

| Name | Responsibility |
|---|---|
| `ORIGIN` | identity, canonical codec, evidence capsules, claims, anchors, publication |
| `SIGHTLINE` | source adapters, packet/frame timeline, clocks, media, calibration |
| `ATLAS` | keyframes, tracks, poses, depth, refinement, fusion, geometry |
| `WITNESS` | scale, uncertainty, coverage, validation, terminal claim proof |
| `LANTERN` | ontology, semantic observations, resolution, scene graph |
| `COMPASS` | coverage, information gain, manual next-best-view proposals |
| `VAULT` | CAS, archive, ATP/RaptorQ, retention, repair, restore |
| `FABRIC` | CLI, protocol, MCP, reports, search, sessions, obligations |
| `LAB` | deterministic runtime adapters, faults, corpora, benchmarks, qualification |

Names organize design; crate boundaries follow dependency/authority needs rather than one crate per
marketing name.

## 96. Target crate graph

### 96.1 Foundation

```text
fdgr-error          stable error IDs and bounded diagnostics
fdgr-types          IDs, anchors, claims, coordinates, units, states
fdgr-codec          canonical binary codec and test vectors
fdgr-registry       immutable registry loading/validation/fingerprints
fdgr-policy         deterministic policies and decision cards
fdgr-claims         claim lifecycle, witnesses, terminal predicates
```

### 96.2 Evidence and custody

```text
fdgr-evidence       capsules, lineage, roots, provenance
fdgr-ledger         semantic ledger traits and reference implementation
fdgr-ledger-fsqlite FrankenSQLite adapter
fdgr-cas            immutable object/chunk/manifest store
fdgr-publication    reserve/materialize/verify/publish
fdgr-custody        durability, replica, retention, repair states
fdgr-fs             FrankenFS adapter
```

### 96.3 Capture and media

```text
fdgr-device         generic source/device profile and adapter traits
fdgr-dji            DJI compatibility/read-only adapter family
fdgr-media          packet/frame/container/rendition semantic types
fdgr-ffmpeg         supervised media-sidecar protocol
fdgr-clock          clock domains, mappings, discontinuities
fdgr-calibration    camera/crop/distortion/rolling-shutter/gimbal models
fdgr-capture        capture session orchestration
fdgr-quality        image-quality and dynamic-region observations
```

### 96.4 Geometry

```text
fdgr-keyframes      deterministic selection and decision certificates
fdgr-features       reference features/descriptors/matches/tracks
fdgr-model-protocol sealed model-worker requests/results
fdgr-models         admitted model profile routing
fdgr-pose           constraints, pose graph, loops, gauges
fdgr-opt            deterministic reference refinement
fdgr-depth          depth/correspondence observations and ensemble
fdgr-scale          scale witnesses, fusion, conflict, transforms
fdgr-fusion         surfel/TSDF/occupancy fusion
fdgr-mesh           topology, extraction, simplification, LOD
fdgr-geometry       generation manifests, tiles, measurements, exports
fdgr-uncertainty    typed uncertainty and calibration
```

### 96.5 Semantics and cognition

```text
fdgr-ontology       stable concepts, attributes, relation/gate registry
fdgr-semantics      observations, hypotheses, resolver, counterevidence
fdgr-scene          reference scene/provenance graph
fdgr-graph          FrankenGraphDB adapter and algorithms
fdgr-coverage       surface/detectability coverage certificates
fdgr-compass        next-best-view and capture suggestions
fdgr-search         Frankensearch adapter and result shaping
fdgr-report         typed report IR and Franken Markdown adapter
fdgr-objectives     missions, objectives, questions, deficits, stopping
fdgr-context        Agent Turn Packet, profiles, context packs, Pack DNA
fdgr-planner        affordances, candidates, Pareto comparison, decision cards
fdgr-agent          attention, recommendations, handoff, episode/surprise
```

### 96.6 Archive and surfaces

```text
fdgr-archive        provider-neutral replica/restore protocol
fdgr-s3             S3-compatible operation semantics and profiles
fdgr-atp            ATP/RaptorQ immutable-state movement
fdgr-session        sessions, capabilities, leases, obligations
fdgr-domain         public command/query facade
fdgr-cli            complete command-line surface
fdgr-mcp            replaceable FastMCP presentation
fdgr-daemon         supervised local service and streaming
fdgr-view           optional local viewer/export server
```

### 96.7 Verification

```text
fdgr-reference      simple independent semantic/oracle paths
fdgr-lab            LabRuntime adapters and fault schedules
fdgr-conformance    schemas, compatibility, protocol, model/adapter suites
fdgr-bench          same-binary benchmarks and receipts
fdgr-fuzz           bounded parser/codec/state-machine fuzz targets
fdgr-qualify        local release qualification and evidence bundle
```

## 97. Layering constraints

A target DAG:

```text
fdgr-error/types/codec/registry
        ↓
policy/claims/evidence/ledger/cas/publication
        ↓
media/clock/calibration/device/model-protocol
        ↓
keyframes/features/pose/depth/scale/fusion/geometry
        ↓
coverage/ontology/semantics/scene/search/report
        ↓
session/domain/archive/capture orchestration
        ↓
cli/mcp/daemon/view

reference/lab/conformance/bench depend inward for testing,
never the production core depending on test harnesses.
```

Forbidden cycles include:

- geometry depending on MCP/UI;
- claims depending on a model framework;
- evidence depending on search/graph projections;
- canonical codec depending on database/filesystem adapters;
- device adapter depending on semantic resolver;
- archive depending on report;
- pure math kernels depending on Asupersync or OS I/O.

The repository validator checks manifest dependency edges against the frozen layer registry once
crates land.

## 98. Core traits

Conceptual traits remain small and semantic:

```text
EvidenceLedger
ObjectStore
Publisher
SourceAdapter
MediaWorker
ClockEstimator
CalibrationStore
ModelWorker
PoseEngine
DepthEngine
FusionEngine
ScaleResolver
CoverageEngine
SemanticResolver
SceneStore
ObjectiveStore
QuestionEngine
ContextPackBuilder
CandidatePlanner
ObligationStore
EpisodeStore
HandoffStore
ArchiveProvider
ReportRenderer
```

Traits do not expose backend transactions, file descriptors, Python tensors, ffmpeg AV structs,
MCP values, or provider SDK objects. Cancellation/budgets flow through `&Cx` at effectful seams.

## 99. Dependency admission

The machine allowlist is `registries/dependency_allowlist.toml`. Each admitted item records exact
revision/version, features, transitive closure, license, unsafe/FFI surface, runtime/thread/network
behavior, deterministic policy, supply-chain verification, integration owner, reference oracle,
and qualification receipt.

An owned sibling is not automatically safe to adopt. Its current APIs and claims may evolve.
Exact integration evidence at the pinned revision is required.

## 100. External process profiles

A sidecar profile includes executable/container root, code/weight roots, runtime/drivers, command
schema, environment, filesystem mount set, network policy, resource limits, deterministic/numeric
classification, input/output schemas, cleanup behavior, license receipt, and platform matrix.

The process protocol is deliberately more stable than model-specific Python APIs. A new model can
be wrapped without changing Rust domain schemas when it supplies an admitted proposal contract.

## 101. Build and release

- builds are reproducible from exact lock/toolchain/artifact manifests;
- no build script downloads models or codecs;
- release artifacts include checksums, signatures/provenance when available, SBOM/dependency
  closure, registries, schemas, and qualification receipt;
- installers verify artifacts and run a bounded self-test/doctor;
- model artifacts may be separately provisioned due size/license;
- platform/device/model profile claims are matrix rows, not universal version numbers;
- GitHub Actions mirrors qualification but cannot substitute for local hardware/device/model tests.

---

# Part XII — Determinism, laboratory testing, benchmarks, and performance

## 102. Determinism classes

### 102.1 Strict deterministic

Same canonical inputs, registries, policy, build/profile, and schedule produce byte-identical
outputs. Required for identities, plans, manifests, registry fingerprints, claim transitions,
reference algorithms, ordering, reports where eligible, and replay decisions.

### 102.2 Numeric deterministic

Same exact hardware/kernel profile produces byte-identical numeric outputs; cross-profile outputs
must fall within a frozen equivalence envelope and canonical postprocessing policy.

### 102.3 Bounded nondeterministic

External model/GPU/codec behavior may vary within measured bounds. Raw outputs receive distinct
identities. Admission depends on semantic equivalence metrics and uncertainty. Nondeterminism is
never hidden behind a stable logical ID.

### 102.4 Adaptive deterministic

Policy can choose among admitted paths using measured state. The decision function, observations,
tie-breaks, and decision card replay. Adaptivity cannot weaken proof gates.

## 103. Laboratory runtime

The same orchestration code runs under Asupersync LabRuntime with virtual time and injected:

### 103.1 Capture faults

- packet loss, duplication, reorder, jitter, truncation;
- configuration/keyframe loss;
- variable frame rate and timestamp wrap/reset;
- controller/app backgrounding, reconnect, source switch;
- buffer pressure and backpressure;
- partial file reads and removable-media disappearance;
- telemetry dropout, drift, outliers, and unit mistakes.

### 103.2 Process faults

- spawn failure;
- child hang or slow output;
- malformed stdout/stderr/progress;
- nonzero/zero exit with invalid outputs;
- partial files and ENOSPC;
- descendant escape/leak;
- ignored termination signal;
- model NaN/infinity/huge dimensions;
- framework version/profile mismatch;
- GPU OOM or driver reset.

### 103.3 Storage/publication faults

- short writes, torn metadata, fsync failure;
- crash at every reserve/materialize/verify/publish step;
- missing/corrupt child and wrong manifest;
- stale branch head or fence;
- disk full and permission change;
- symlink/path traversal attempts;
- ledger/CAS disagreement;
- garbage collection racing publication.

### 103.4 Cloud faults

- DNS/TLS/auth failure;
- throttle, timeout, connection reset;
- ambiguous completion response;
- part replacement/list inconsistency;
- wrong size/metadata/checksum;
- object substitution or truncated readback;
- credential expiry/rotation;
- lifecycle deletion and object-lock conflict;
- provider outage during restore.

### 103.5 Geometry/semantic adversaries

- pure rotation, weak baseline, planar scene, repetitive facade;
- rolling shutter, motion blur, exposure flicker;
- sky/water/glass/specular metal;
- foliage and dynamic people/cars;
- calibration/crop mismatch;
- false loop closure;
- disconnected components;
- conflicting scale witnesses;
- detector/VLM hallucination;
- small/thin asset below detectability;
- unobserved domain requested as absence;
- geometry change versus model reinterpretation.

## 104. Required test families

| ID | Family |
|---|---|
| TEST-001 | canonical codec vectors, malformed bounds, roundtrip, cross-implementation |
| TEST-002 | identity/domain-separation and collision/substitution handling |
| TEST-003 | anchor snapshot isolation and mixed-generation rejection |
| TEST-004 | clock discontinuity, drift, packet/frame accounting |
| TEST-005 | coordinate conversion and known-camera projection/unprojection |
| TEST-006 | scale witness admission, conflict, demotion, and metric-output refusal |
| TEST-007 | publication kill-point crash matrix and orphan staging repair |
| TEST-008 | cancellation request/drain/reconcile/finalize at every boundary |
| TEST-009 | idempotency and indeterminate external effect reconciliation |
| TEST-010 | source/device compatibility and secret leak scans |
| TEST-011 | media sidecar malformed input/output, descendant cleanup, timeline equivalence |
| TEST-012 | model artifact/license/profile and output validation |
| TEST-013 | reference/optimized/model geometry differential tests |
| TEST-014 | loop closure, refinement, fusion, topology, and uncertainty |
| TEST-015 | semantic presence, counterevidence, critical resolution, absence refusal |
| TEST-016 | coverage certificate and next-best-view deterministic policy |
| TEST-017 | archive multipart, checksum, readback, repair, and restore |
| TEST-018 | capability noninterference and privacy export |
| TEST-019 | graph/search/report rebuild and provenance equivalence |
| TEST-020 | CLI/MCP schemas, pagination, cancellation, tasks, and multi-client isolation |
| TEST-021 | migration, backward/forward compatibility, and old-capture reinterpretation |
| TEST-022 | local release qualification and evidence bundle self-verification |

Each test family has positive and negative cases. A rejection-only test does not prove the accepted
path.

## 105. Fixture hierarchy

- tiny hand-authored canonical objects and state machines;
- synthetic exact camera/geometry scenes;
- procedural homes with ontology truth;
- rendered codec/camera/noise/rolling-shutter variants;
- public photogrammetry/SLAM/depth datasets under compatible terms;
- UAVFF3D and UAV-domain corpora;
- private DJI Flip and generic-device compatibility corpus;
- private real-home corpus with measured/survey/LiDAR references;
- large-scale soak captures and remote archive graphs;
- retained production doctor bundles transformed into private/minimized regression fixtures.

Private fixtures use access controls and are not embedded into public binaries or support bundles.
Synthetic/minimized public equivalents are preferred for bug reproduction.

## 106. Geometry benchmark metrics

### 106.1 Camera and trajectory

- rotation and translation error;
- ATE/RPE where applicable;
- loop consistency;
- connected-component/gauge correctness;
- pose confidence calibration;
- initialization/relocalization success and failure classification.

### 106.2 Depth and surface

- absolute-relative/RMSE/threshold depth metrics;
- edge and thin-structure fidelity;
- point/surface Chamfer distance;
- precision, recall, F-score at multiple thresholds;
- completeness and accuracy by material/region/view quality;
- normal consistency;
- free-space contradiction;
- mesh holes, nonmanifold edges, self-intersection, topology;
- held-out reprojection and photometric/feature consistency.

### 106.3 Scale and measurement

- similarity scale error;
- local/nonrigid scale drift;
- measured segment/opening/asset dimension error;
- area/volume/path/slope error;
- uncertainty interval coverage;
- witness conflict/refusal behavior.

## 107. Semantic benchmark metrics

- concept presence precision/recall;
- confusion matrix and open-world unknown handling;
- 2D mask IoU and video tracking identity;
- 3D location, extent, orientation, and surface association error;
- relation accuracy;
- critical-asset false-resolution rate;
- counterevidence sensitivity and calibration;
- human review rate/time per resolved asset;
- absence refusal on uncovered domains;
- certified-absence accuracy on controlled covered domains;
- change detection: physical change versus reinterpretation.

A low false-resolution rate for critical assets is more important than maximizing recall through
confident guessing.

## 108. System and economic metrics

- source bytes/s, packet/frame gaps, CPU, memory, disk write amplification;
- time to first preview, first provisional pose/map, first coverage suggestion;
- online update latency and staleness;
- converged reconstruction wall time and GPU-hours;
- peak and steady CPU/GPU memory;
- local storage by object class and deduplication ratio;
- archive bytes, requests, multipart overhead, egress, restore time;
- energy and monetary cost per capture/minute/square meter/qualified twin;
- cancellation drain and unresolved obligation count;
- operator flight/review/intervention time;
- agent tokens and round trips per representative task;
- index/report rebuild time;
- deterministic replay rate and numeric envelope.

## 109. Benchmark receipt

Every benchmark receipt records:

```text
source commit and dirty state
toolchain/target/build/profile
dependency and registry roots
model/code/environment/driver roots
hardware/OS/device profile
input corpus and anchor roots
algorithm arm and exact policy
warmup, samples, random/deterministic schedule
resource isolation and background load observations
correctness/equivalence results
raw samples and statistical summary
output root digests
negative evidence and excluded cases
```

No chart without the underlying receipt is release evidence.

## 110. Same-binary A/B doctrine

Alternative implementations compile into one qualification binary and select arms at runtime from
identical immutable inputs. Setup is shared. Output semantics are compared before performance. If
outputs differ, the benchmark either defines an intentional quality tradeoff or is invalid as a
speed comparison.

Potential arms:

- reference versus optimized codec/hash/chunker;
- classical versus learned keyframe/correspondence;
- MapAnything versus streaming model versus ensemble;
- scalar versus vectorized refinement/fusion;
- full recompute versus incremental update;
- local-only versus ATP/cloud transfer policy;
- lexical versus hybrid search;
- point/surfel/TSDF fusion policies.

## 111. Initial SLO targets

Targets are profile-specific and remain hypotheses until qualified.

### SLO-001 — Capture accounting

**TARGET:** Every admitted source byte/packet/frame range is either published, explicitly rejected,
or represented as a gap; unaccounted loss is zero.

### SLO-002 — Online feedback

**TARGET:** On a qualified workstation profile, p95 source-frame-to-quality/coverage update is under
2 seconds and progress heartbeat under 500 ms, excluding known source latency. Geometry may be
slower and explicitly stale.

### SLO-003 — Publication atomicity

**TARGET:** Across the complete kill-point campaign, readers observe only the prior or successor
complete root; zero partial roots.

### SLO-004 — Cancellation

**TARGET:** Cooperative local work stops accepting new effects immediately and reaches terminal or
explicit indeterminate state within the profile drain budget; zero silent descendants or lost
obligations.

### SLO-005 — Exterior geometry standard profile

**TARGET:** On the ground-truth home corpus under a defined capture protocol and witnessed scale,
visible static surface median error ≤ 5 cm, 90th percentile ≤ 15 cm, and certifiable surface
completeness ≥ 90%, with calibrated uncertainty and named excluded materials/regions.

### SLO-006 — High-quality profile

**TARGET:** Under closer/slower capture and stronger calibration/scale evidence, visible static
surface median error ≤ 2.5 cm and selected opening/asset dimension median error ≤ 3 cm on qualified
corpora. This is not a universal product promise.

### SLO-007 — Critical semantic precision

**TARGET:** Resolved critical-asset false-positive rate below 1% on the qualification corpus, using
abstention/human confirmation as needed. Hypothesis recall is measured separately.

### SLO-008 — Archive recovery

**TARGET:** A full local-loss restore from each advertised provider reconstructs every retained
root with byte-identical object identities and zero silent substitution.

### SLO-009 — Deterministic reference replay

**TARGET:** Strict reference state machines, plans, manifests, claims, and reports reproduce
byte-identically across qualified platforms where specified.

### SLO-010 — Agent economy

**TARGET:** Representative “what is missing?”, “why this asset?”, “measure this opening,” and
“verify archive” workflows complete with bounded context packs and an order of magnitude fewer
transferred tokens than raw frame/point dumps.

### SLO-011 — Cold-agent orientation

**TARGET:** One briefing lets a fresh agent identify mission, exact anchor/continuity, top world
facts, highest-value questions, all critical active work, authority posture, and safest next step.

### SLO-012 — Heartbeat economy

**TARGET:** When nothing meaningful changed, p95 pulse output is at most 400 tokens, contains no
unchanged inventory, and preserves continuity, indeterminate work, and critical warnings.

### SLO-013 — Context value density

**TARGET:** On qualified agent tasks, selected packs achieve at least full-context decision success
with at most one tenth of transferred tokens.

### SLO-014 — Handoff sufficiency

**TARGET:** A fresh agent resumes and completes the minimum safe next step from the handoff capsule
without transcript replay in every qualified scenario.

### SLO-015 — Recommendation calibration

**TARGET:** Information gain, cost, and completion intervals are calibrated by workload class and
surface drift before any related policy promotion.

### SLO-016 — First-try discoverability

**TARGET:** At least 95% of benchmark intents reach a valid read/propose path on the first call
using only self-description; each rejection names an exact safe repair.

### SLO-017 — Accretive control cost

**TARGET:** Admitted learning reduces median total control cost on repeated qualified workloads
without regression in proof, calibration, safety, privacy, or recovery.

Targets may be tightened or split by profile through ADR. They cannot be quietly relaxed to make a
release pass.

## 112. Performance architecture

Performance comes from architecture before micro-optimization:

- progressive live/offline work;
- keyframe/region/tile selection;
- immutable content reuse across generations;
- derived caches with exact basis;
- bounded channels and backpressure;
- zero-copy/streaming within safe Rust where semantics permit;
- model batching and window scheduling;
- spatial tiling and incremental invalidation;
- compression tiering and local/cloud parallelism;
- ATP resumability/repair;
- query planning and graph/search fusion;
- avoiding duplicate decode/model/fusion work through exact cache identity.

Optimization never bypasses custody, validation, scale, uncertainty, privacy, or publication.
Unsafe SIMD is not admitted into FDGR crates; optimized native/GPU kernels remain behind process or
owned-safe abstractions and require reference fallback.

---

# Part XIII — Security, privacy, safety, and governance

## 113. Threat model summary

The detailed security document is normative. Primary threats include:

- malicious media/container/codec/parser input;
- resource exhaustion and decompression/tensor bombs;
- path traversal, symlink races, overwrite, and partial publication;
- process escape, descendant leak, and unbounded GPU/native behavior;
- model output injection, NaN/huge coordinates, or prompt-derived command text;
- credential leakage from DJI/vendor/archive sessions;
- unauthorized device, archive, delete, or export capability;
- cloud substitution, incomplete multipart, public bucket, retention/lifecycle errors;
- false or overprecise critical asset semantics;
- private home geometry/imagery leakage;
- supply-chain substitution of binaries, weights, drivers, containers, or calibration;
- agent race or stale-plan effects;
- hidden adaptive policy weakening gates.

## 114. Bounds

Every untrusted parse and operation has explicit limits:

- input bytes/chunks/packets/frames/streams;
- image dimensions/pixels/frame rate/duration;
- metadata/string/nesting/table counts;
- points/voxels/surfels/triangles/tiles/assets/edges;
- model inputs/outputs/tokens/tensors;
- filesystem paths/files/directories;
- child processes/descriptors/pipes;
- CPU/GPU/memory/disk/network/storage/time;
- diagnostics/log/report size;
- query expansion/path depth/result count.

Limit exhaustion yields a stable typed error and partial/progress receipt where applicable. It does
not crash, allocate until OOM, or truncate silently.

## 115. Secret handling

Secrets are capability objects, not strings threaded through arbitrary config. Rules:

- use OS secret storage or short-lived delegated credentials where possible;
- expose only endpoint/bucket/prefix/method/time/byte scope needed;
- never pass archive credentials to model/media workers;
- never serialize raw tokens/cookies/passwords in plan, evidence, trace, doctor, report, or memory;
- allowlist logged fields rather than relying solely on redaction regex;
- scan sidecar environment/stderr and protocol fixtures for secret patterns;
- erase/release session capability at region finalization where practical;
- treat packet captures as highly sensitive private evidence.

## 116. Private-space geometry

A home twin can expose entrances, windows, equipment, paths, and coordinates. Privacy classes are:

```text
private_evidence
private_operational
shareable_owner
shareable_redacted
public
```

A transition to a less restrictive class is an effect with explicit plan, review, redaction,
verification, and receipt. Removing EXIF is insufficient; geometry and textures may reveal
location/identity. Public exports may remove geodetic alignment, crop neighboring property, redact
critical assets, remove textures, simplify geometry, or generalize coordinates.

## 117. Bystanders and neighboring property

Capture and processing SHOULD minimize bystanders and unnecessary neighboring private space.
People/vehicles/animals are dynamic privacy-sensitive observations used for fusion exclusion, not
identity analysis. Exports apply masks/crops/generalization under policy. Coverage excludes
unauthorized domains rather than declaring them empty.

## 118. Operational flight safety

FDGR's initial live role is observation and guidance. It does not send flight commands. Capture
suggestions are not guaranteed safe routes. The human operator remains responsible for lawful
flight, line of sight, obstacles, people, property boundaries, weather, battery, and device
warnings.

Any future flight-control proposal requires a separate plan covering:

- control authority and human override;
- geofence/regulatory policies;
- obstacle/perception uncertainty;
- fail-safe, return-to-home, lost-link, and battery behavior;
- command idempotency and actuator outcome;
- simulation and hardware qualification;
- formal/runtime safety invariants;
- liability and deployment scope.

It cannot be added as an incremental extension to `device.observe`.

## 119. Model and dataset governance

Each artifact has:

- exact digest/source/version;
- license and acceptable-use terms;
- redistribution and commercial scope;
- known dataset restrictions/lineage;
- checkpoint access/gating status;
- vulnerability/malware scan;
- model card and maintainer disclosures;
- FDGR benchmark and negative evidence;
- deprecation/revocation policy.

If terms change or a checkpoint is withdrawn/contaminated, the registry can block new execution
while retaining enough identity to interpret historical results. Historical derived output is not
silently relabeled.

## 120. Governance of claims

Public release notes and README tables use typed claims. A claim must name:

- exact version/profile/device/model/platform;
- evidence receipt and corpus;
- positive dimensions earned;
- negative evidence and exclusions;
- expiry/requalification trigger;
- whether the claim is strict, numeric-envelope, or best-effort.

Words such as “real-time,” “metric,” “complete,” “accurate,” “production-ready,” “supports DJI
Flip,” “secure,” and “lossless” are prohibited without registered definitions and evidence.

---

# Part XIV — Work program, gates, and release progression

## 121. Work-package doctrine

`registries/work_packages.toml` is the machine source. Work packages are dependency ordered.
Agents MUST NOT start a later package by implementing around a missing prerequisite. A discovered
prerequisite updates the work graph.

A work package is complete only when:

- contract/registry/schema changes are stable;
- reference behavior exists where applicable;
- success/failure/cancellation/recovery behavior is tested;
- compatibility/migration is explicit;
- positive and negative evidence is retained;
- status/docs agree;
- its acceptance gate predicate passes.

## 122. Workstream A — Constitution and reference substrate

### WP-000 — Constitution and registries

Freeze vocabulary, stable IDs, invariants, claims, effects, errors, capabilities, schemas,
coordinates, clocks, scale states, model/license policy, and dependency rules.

**Depends:** none
**Gate:** GATE-000

### WP-001 — Workspace and qualification scaffold

Create strict crate DAG, latest-nightly policy, safe-Rust lints, deterministic CLI scaffold,
repository validator, CI mirror, release/evidence structure, and Beads bootstrap.

**Depends:** WP-000
**Gate:** GATE-000

### WP-002 — Canonical identity and codec

Implement domain-separated hashing, canonical binary encoding, finite numeric policy, chunk
manifests, test vectors, malformed bounds, and cross-implementation oracle.

**Depends:** WP-000, WP-001
**Gate:** GATE-001

### WP-003 — Reference evidence ledger

Build append-only capsules, lineage roots, anchor reads, claim/effect/obligation state, replay, and
a deliberately simple deterministic file/in-memory adapter.

**Depends:** WP-002
**Gate:** GATE-001

### WP-004 — Content-addressed publication

Build CAS, staging, reserve/materialize/verify/publish, root traversal, kill-point crash matrix,
orphan cleanup, and repair-plan reference.

**Depends:** WP-002, WP-003
**Gate:** GATE-002

## 123. Workstream B — Recorded media and time

### WP-005 — Original media import

Exact byte/range import, metadata observations, duplicate handling, damaged/partial files, custody
states, and source receipts.

**Depends:** WP-003, WP-004
**Gate:** GATE-002

### WP-006 — Media sidecar protocol

Supervise exact ffprobe/ffmpeg profiles, sealed manifests, pipes/files, bounds, cancellation,
descendant cleanup, output validation, and deterministic/semantic classes.

**Depends:** WP-004, WP-005
**Gate:** GATE-003

### WP-007 — Packet/frame/clock ledger

Represent PTS/DTS/arrival/decode/display/telemetry clocks, gaps, duplicates, reorder, drift,
discontinuities, and epoch transitions.

**Depends:** WP-005, WP-006
**Gate:** GATE-003

### WP-008 — Calibration registry

Implement camera/crop/distortion/stabilization/rolling-shutter/gimbal models, fixture procedures,
refinement, residuals, uncertainty, and compatibility scope.

**Depends:** WP-007
**Gate:** GATE-004

### WP-009 — Scale witness system

Implement relative/estimated/witnessed/surveyed states, witness schemas, correlation, robust fusion,
conflict/demotion, and metric-output non-bypassability.

**Depends:** WP-008
**Gate:** GATE-004

## 124. Workstream C — DJI and live acquisition

### WP-010 — DJI compatibility laboratory

Build read-only probes and exact aircraft/controller/app/firmware/OS/region profiles, redacted
fixtures, reconnect/clock/latency tests, and secret scans.

**Depends:** WP-005, WP-007
**Gate:** GATE-005

### WP-011 — Recorded and mirrored adapters

Admit DJI original import, DJI Fly/controller recording, OS screen recording, HDMI/UVC/capture-card
paths, and source alignment where available.

**Depends:** WP-010
**Gate:** GATE-005

### WP-012 — Owner-authorized live adapter research

Characterize documented and owned-session live paths, read-only first, with state-machine parsers,
malformed campaigns, profile scope, and no control/bypass.

**Depends:** WP-010, WP-011
**Gate:** GATE-006

## 125. Workstream D — Reference and learned geometry

### WP-013 — Keyframe and quality engine

Deterministic quality observations, dynamic masks, coverage novelty, keyframe policy, set-cover
variants, and decision certificates.

**Depends:** WP-007, WP-008
**Gate:** GATE-007

### WP-014 — Reference feature/track/pose pipeline

Implement simple deterministic features, matches, robust geometry, tracks, pose graph, sparse
triangulation, and known failure classifications.

**Depends:** WP-008, WP-013
**Gate:** GATE-007

### WP-015 — Model-worker protocol

Implement artifact/profile registry, sealed request/result, sandbox/no-network policy, resource
budgets, finite/schema/coordinate validation, state checkpoints, and quarantine.

**Depends:** WP-002, WP-004
**Gate:** GATE-008

### WP-016 — MapAnything candidate lane

Integrate exact Apache artifacts and compare pose/depth/point proposals against reference and ground
truth on drone/home domains.

**Depends:** WP-013, WP-015
**Gate:** GATE-008

### WP-017 — Streaming geometry research lanes

Evaluate DA3 Streaming, CUT3R, and successors under exact license, hidden-state, latency, memory,
window-boundary, drift, and replay gates.

**Depends:** WP-013, WP-015
**Gate:** GATE-008

### WP-018 — Pose graph and bundle refinement

Robust deterministic optimization, loop closure, calibration variables, gauges, outlier policy,
state checkpoints, decision cards, and optimized/reference equivalence.

**Depends:** WP-014, WP-016
**Gate:** GATE-009

### WP-019 — Depth ensemble and uncertainty

Combine model/classical evidence with correlation-aware policy, calibrate uncertainty, preserve
edges/thin structures, and validate held-out views.

**Depends:** WP-016, WP-017, WP-018
**Gate:** GATE-009

### WP-020 — Geometry fusion

Publish surfel/TSDF/occupancy/mesh/point/appearance generations, spatial tiling, topology,
measurements, LOD, and authority classifications.

**Depends:** WP-009, WP-018, WP-019
**Gate:** GATE-010

### WP-021 — Coverage and next-best-view

Surface/detectability coverage certificates, provisional frustum coverage, information-gain
candidates, manual capture guidance, and deterministic optimization.

**Depends:** WP-013, WP-020
**Gate:** GATE-010

## 126. Workstream E — Semantic twin

### WP-022 — Ontology and semantic observations

Freeze property ontology and evidence semantics; integrate Qwen/segmentation/OCR/human observation
profiles without direct resolution authority.

**Depends:** WP-015, WP-020
**Gate:** GATE-011

### WP-023 — Resolver and scene graph

Multi-view geometry association, persistent asset hypotheses, relations, dimensions,
counterevidence, critical gates, absence witnesses, history, and reference graph.

**Depends:** WP-021, WP-022
**Gate:** GATE-011

## 127. Workstream F — Archive, surfaces, and operations

### WP-024 — Archive and cloud replication

Provider-neutral immutable S3 protocol, multipart resume/reconcile, own digests, B2/R2 profiles,
readback verification, encryption, retention, repair, and restore drills.

**Depends:** WP-004, WP-005
**Gate:** GATE-012

### WP-025 — Search, reports, and exports

Frankensearch projection, typed report IR, Franken Markdown, glTF/point/mesh/scene exports,
measurements, provenance links, and privacy-scoped derivatives.

**Depends:** WP-020, WP-023, WP-024
**Gate:** GATE-013

### WP-026 — Complete CLI and robot protocol

Stable commands, JSON/JSONL/TOON, sessions, plans, obligations, progress, continuations, doctor,
explain, migration, and shell completion.

**Depends:** WP-003, WP-025
**Gate:** GATE-013

### WP-027 — MCP presentation

Exact FastMCP pin/profile, narrow tools, session authority, task projection, cancellation,
pagination, multi-client/cache isolation, and conformance evidence.

**Depends:** WP-026
**Gate:** GATE-014

### WP-028 — Eidetic operational integration

Curated evidence/failure/anti-pattern export, anchor-preserving templates, privacy redaction, and
one-way memory doctrine.

**Depends:** WP-025, WP-026
**Gate:** GATE-014

## 128. Workstream G — Verification and release

### WP-029 — Deterministic laboratory

LabRuntime effect adapters, packet/clock/process/disk/cloud/model/cancellation fault schedules,
DPOR/model checking for critical state machines, and doctor-bundle replay.

**Depends:** WP-003, WP-006, WP-015, WP-024
**Gate:** GATE-015

### WP-030 — Ground-truth benchmark corpus

Synthetic and real homes/UAV, survey/LiDAR/measurements, semantics, held-out views, adversarial
materials, privacy controls, and artifact/version manifests.

**Depends:** WP-009, WP-020, WP-023
**Gate:** GATE-015

### WP-031 — Performance and economic optimizer

Profile real walls; same-binary alternatives; optimize decode, model windows, refinement, fusion,
tiling, archive, search, reports, and agent economy without semantic drift.

**Depends:** WP-024, WP-029, WP-030
**Gate:** GATE-016

### WP-032 — Security, privacy, and release qualification

Threat campaigns, compatibility matrix, privacy export/deletion, supply-chain/signing, recovery,
installer, release claims, local qualification, and evidence bundle.

**Depends:** WP-026, WP-027, WP-029, WP-030, WP-031
**Gate:** GATE-017

## 128A. Workstream H — Agent cognition, control, and accretion

The machine work-package registry is normative; this section provides the implementation narrative.

### WP-033 through WP-036 — Contract, packet, graph, and context

Freeze the agent semantic constitution; implement success/error/progress Agent Turn Packet
builders; build mission/objective/question/deficit state; and add deterministic context packs with
Pack DNA, mandatory safety classes, budget marginal value, and continuation.

### WP-037 through WP-039 — Candidates, watch, and pilot

Add common-basis candidate sets, counterfactual branches, deterministic Pareto frontiers,
obligation/watch/reconciliation projection, and manual-pilot evidence-gain guidance tied to
questions and stopping.

### WP-040 through WP-042 — Episodes, handoff, and multi-agent

Publish predicted-versus-observed episodes and surprises; produce authority-free sufficient
handoffs; and add semantic reservations, objective ownership, branch visibility, duplicate-work
detection, current leases/fences, and merge/rebase guidance.

### WP-043 through WP-044 — Self-description and qualification

Generate operation/capability/profile/schema/error/maturity manifests, help and robot-doc parity,
examples and repairs; then run the complete agent ergonomics/accretion scenario suite under local
Doodlestein lanes.

**Gates:** GATE-018 through GATE-023

## 129. Acceptance gates

### GATE-000 — Constitution frozen

Registries/schemas parse; stable IDs are unique; dependency and unsafe policy passes; plan/status
agree; no current claim exceeds evidence.

### GATE-001 — Reference identity and ledger

Canonical codec, hashing, capsule order, anchors, claims, and replay reproduce byte-identically
across the fixture corpus and independent oracle.

### GATE-002 — Crash-safe evidence publication

Original import and object roots survive the complete staged-publication kill matrix. Readers see
old/new complete roots only. Repair handles orphan staging without identity rewrite.

### GATE-003 — Media normalization qualified

Malformed media, timestamp accounting, deterministic/semantic profiles, bounds, cancellation,
child/descendant cleanup, and output validation pass.

### GATE-004 — Calibration and scale honest

Metric output is structurally impossible without scale witnesses. Calibration/scale residual and
uncertainty behavior passes synthetic and measured fixtures, including contradiction/demotion.

### GATE-005 — Recorded capture useful

At least one DJI Flip original/recorded path and one generic camera path produce complete anchored
frame generations with explicit clocks/calibration/source limitations.

### GATE-006 — Live observation admitted

A version-pinned owner-authorized live path runs read-only, accounts for gaps/config changes,
reconnects under policy, closes without leaked work/secrets, and degrades safely when unavailable.

### GATE-007 — Reference geometry oracle

A deterministic classical path produces cameras/sparse geometry or typed refusal on qualification
fixtures, with known failure classifications and replay.

### GATE-008 — Model lanes admitted

Exact artifacts pass license, offline provisioning, bounded worker, schema/numeric/coordinate,
reproducibility, UAV/home-domain, and failure gates. Source presence is insufficient.

### GATE-009 — Refined geometry stable

Pose/depth refinement improves or safely rejects priors; loops, calibration, outliers, gauges,
uncertainty, and optimized/reference equivalence pass.

### GATE-010 — Digital surface model useful

Fused geometry, topology, witnessed scale, held-out reprojection, ground-truth surface metrics,
uncertainty calibration, and coverage meet the named profile targets.

### GATE-011 — Semantic twin trustworthy

Ontology assets resolve with multi-view evidence, geometry association, counterevidence, critical
review, calibrated uncertainty, absence refusal, and temporal interpretation.

### GATE-012 — Archive recoverable

Local loss followed by B2 and/or R2 restore reconstructs every advertised retained root from
verified immutable objects, including multipart failure and repair scenarios.

### GATE-013 — Human and agent surfaces complete

CLI, schemas, reports, search, measurements, exports, progress, continuation, and explanations
preserve anchors/status/uncertainty/privacy and pass compatibility/migration tests.

### GATE-014 — Agent integration non-bypassable

MCP and Eidetic surfaces cannot mutate or redefine canonical state outside domain paths. Session,
capability, cache, task, cancellation, and multi-agent tests pass.

### GATE-015 — Fault and benchmark evidence

Named fault schedules and ground-truth/adversarial corpora cover required classes with reproducible
receipts and visible negative evidence.

### GATE-016 — Performance claims earned

Same-binary experiments establish semantic equality or explicit quality tradeoffs and statistically
defensible latency, memory, cost, energy, storage, and agent-economy results.

### GATE-017 — Release qualified

Every advertised platform/device/model/provider/privacy/recovery/performance dimension has current
positive evidence. Installer/provenance/self-test pass. No blocking negative evidence is hidden.

### GATE-018 — Agent semantics contract locked

The abstraction tower, loop, anchor vector, four ledgers, epistemic lattice, question graph,
profiles, packet, and authority rules have schemas, registries, and golden vectors.

### GATE-019 — Cold orientation and context packs qualified

Cold arrival, heartbeat, tactical/forensic expansion, gaps, omissions, continuations, Pack DNA,
and token-economy scenarios are deterministic and decision-sufficient.

### GATE-020 — Question-driven planning qualified

Questions, deficits, stopping, affordances, recommendations, candidate plans, counterfactuals,
value of information, and Pareto comparison replay and cannot bypass authority.

### GATE-021 — Active work and handoff qualified

Obligations, progress, confirmations, indeterminate effects, cancellation, reconciliation, and
handoff/resume remain sufficient across context and agent replacement.

### GATE-022 — Accretion qualified

Episodes, surprises, actual costs, feedback, lesson promotion, replay, shadow/canary policy,
monitoring, and rollback improve qualified workloads without weakening gates.

### GATE-023 — Agent-native release qualified

Self-description, first-try discovery, live/lab packet parity, multi-agent coordination, and the
full orient-to-handoff loop pass exact local Doodlestein qualification.

## 130. Release progression

### 0.0.x — Constitutional/reference

Contracts, scaffold, codec, reference ledger/CAS/publication, fixtures, validator. No reconstruction
claim.

### 0.1 — Recorded evidence custody

Original import, media probe/normalization, timeline, local custody, doctor. No metric geometry
claim.

### 0.2 — Relative reference geometry

Classical keyframes/tracks/poses/sparse/dense candidate, relative scale, explicit failure. No model
or semantic claim required.

### 0.3 — Model-assisted offline geometry

One admitted MapAnything lane, refinement, fusion, uncertainty, held-out validation. Metric only
where witnesses exist.

### 0.4 — Coverage and semantic hypotheses

Coverage, next views, ontology observations, scene graph, Qwen/SAM candidates. Critical assets
remain hypotheses unless gates pass.

### 0.5 — Verified archive and reports

B2/R2 replication, restore, reports, search, exports, measurements, privacy policy.

### 0.6 — Owner-authorized live observation

One exact DJI/generic live profile, provisional online geometry/coverage, later original alignment.
No autonomous control.

### 0.7 — Agent cognitive control plane

Agent Turn Packet, four ledgers, objective/question graph, context packs and Pack DNA, candidate
frontiers, obligations/watch/reconcile, pilot profile, handoff/resume, multi-agent coordination,
episodes/surprises, self-description, MCP, and Eidetic export under gates.

### 1.0 — Evidence-qualified digital twin

Ground-truth profile targets, security/privacy/recovery/performance qualification, exact support
matrix, installer/releases, and no aggregate claims beyond earned rows.

Version numbers are planning labels, not promises. Gates, not dates, determine promotion.

---

# Part XV — Risks, open questions, and research agenda

## 131. Principal risks

### RISK-001 — DJI live access remains unavailable or unstable

**Impact:** no direct live stream for the motivating device.
**Mitigation:** original-media import and recorded/mirrored capture are first-class; generic capture
adapters; protocol work optional; live assistance can use controller screen capture.
**Kill criterion:** none for project mission; only the exact live-profile claim is withheld.

### RISK-002 — Monocular metric scale is unreliable

**Impact:** plausible but wrong dimensions.
**Mitigation:** structural metric-output gate; scale witnesses; measured targets; telemetry as
bounded prior; contradiction/demotion; relative geometry remains useful.
**Kill criterion:** no metric claim without independent evidence.

### RISK-003 — Foundation models fail UAV/home domain

**Impact:** drift, wrong cameras/depth, poor fine structures, excessive GPU cost.
**Mitigation:** deterministic classical reference, multiple candidates, UAVFF3D/private corpus,
refinement, held-out views, refusal, exact model admission.
**Kill criterion:** model remains research-only or removed; architecture survives.

### RISK-004 — Semantics overclaim critical assets

**Impact:** unsafe or misleading utility/location information.
**Mitigation:** observation/hypothesis/resolved states; multi-view geometry; ontology-specific gates;
counterevidence; human confirmation; privacy export controls; measure false-resolution separately.
**Kill criterion:** concept remains hypothesis-only until precision target is earned.

### RISK-005 — Long videos overwhelm GPU/memory/storage

**Impact:** high cost or inability to finish.
**Mitigation:** progressive keyframes, streaming/windowed models, spatial tiling, checkpointing,
analysis renditions, content reuse, budgeted quality profiles, cloud tiering.
**Kill criterion:** degrade quality/latency explicitly rather than overcommit.

### RISK-006 — Cumulative complexity prevents implementation

**Impact:** plan becomes ceremony rather than software.
**Mitigation:** thin vertical slices, reference-before-adapter, work-package DAG, no crate before a
boundary, no full Franken integration at once, performance only after profile, status honesty.
**Kill criterion:** simplify implementation while preserving invariants; do not simplify truth.

### RISK-007 — Franken sibling APIs/readiness lag plan

**Impact:** integration blocked or semantics unavailable.
**Mitigation:** semantic traits and deterministic reference adapters; exact current-revision gates;
design reuse without dependency; no sibling as single point of failure.
**Kill criterion:** retain reference or replace adapter without changing domain contracts.

### RISK-008 — Strict no-unsafe/no-FFI limits performance

**Impact:** pure Rust core cannot match native kernels/codecs.
**Mitigation:** external process sidecars, GPU/model workers, safe owned abstractions, tiling,
algorithmic optimization, zero-copy safe APIs, same-binary measurement.
**Kill criterion:** performance claim narrows; safety invariant remains.

### RISK-009 — Cloud compatibility is deceptively incomplete

**Impact:** stalled multipart, corrupt restore, lifecycle loss, checksum confusion.
**Mitigation:** provider profiles; own digests; immutable keys; reconcile/list/head/readback; restore
drills; multi-provider/repair policies.
**Kill criterion:** provider remains unsupported for release claims.

### RISK-010 — Privacy value conflicts with rich semantics

**Impact:** detailed property data leaks or becomes unsafe to share.
**Mitigation:** local-first; privacy scope in anchor; no-network models; redacted exports; critical
asset controls; graph-aware deletion; support bundles exclude raw data.
**Kill criterion:** refuse lower-privacy export when proof is insufficient.

### RISK-011 — Determinism conflicts with GPU/model throughput

**Impact:** unstable roots or irreproducible benchmark.
**Mitigation:** classify determinism; raw output identities; exact profiles; canonical
postprocessing; numeric envelopes; deterministic reference; no logical ID pretending outputs are
identical.
**Kill criterion:** profile cannot earn strict claim but may earn bounded semantic claim.

### RISK-012 — Calibration/rolling shutter dominates error

**Impact:** sophisticated models cannot recover accurate geometry.
**Mitigation:** device/source-specific calibration, crop/stabilization modeling, quality/motion
warnings, rolling-shutter parameters, ground-truth tests, capture guidance.
**Kill criterion:** reject or narrow high-accuracy profile for unsupported motion/source.

### RISK-013 — Foliage/glass/water/repetitive texture defeats geometry

**Impact:** holes, floaters, false loops, wrong surfaces.
**Mitigation:** material/dynamic classification, conservative fusion, multi-view/angle capture,
counterevidence, uncertainty, inferred-surface tags, adversarial corpus.
**Kill criterion:** mark unknown/excluded; do not fill as observed.

### RISK-014 — Agent autonomy creates race or deletion hazards

**Impact:** duplicate work, stale publication, data loss.
**Mitigation:** plans, witnesses, idempotency, leases/fences, capabilities, obligation ownership,
branch isolation, critical deletion plans, no arbitrary commands.
**Kill criterion:** reduce agent authority to read-only until gates pass.

## 132. Open questions

### OPEN-001 — Canonical internal world frame

Should the default local world be gravity-aligned ENU, a camera-origin similarity frame, or a
building-aligned frame with explicit transform? Decision requires telemetry availability,
interchange needs, numeric stability, and repeated-capture alignment tests.

### OPEN-002 — Canonical binary codec

Use a fully custom compact codec, a tightly specified subset of an existing format, or an owned
Franken codec? Need independent implementation, evolution, zero-copy potential, bounds, and audit.

### OPEN-003 — Logical versus representation identity

How should exact logical media identity relate to different chunk/compression/encryption
representations while preserving verification and deduplication?

### OPEN-004 — Best analysis mezzanine

All-intra AV1/HEVC, FFV1, JPEG XL/PNG frame sequence, or multiple profiles? Measure decode random
access, determinism, quality, storage, and model throughput.

### OPEN-005 — DJI telemetry availability

Which exact Flip original files, app exports, controller sessions, or private protocols expose
camera/gimbal/GNSS/barometer telemetry, at what rate/clock/accuracy? Unknown until measured by
profile.

### OPEN-006 — Model execution portability

Container, Python environment lock, Nix/OCI, uv, or custom artifact bundle? Need offline,
reproducible GPU profiles without expanding the Rust trust domain.

### OPEN-007 — Geometry canonical representation

Should surfels, TSDF/occupancy tiles, or another probabilistic surface be the primary canonical
fused representation? Decision should optimize validation, incremental update, measurements,
semantics, storage, and export rather than rendering fashion.

### OPEN-008 — Uncertainty representation

Full covariance, diagonal/local approximations, ensembles, evidential distributions, spatial error
bounds, or multiple typed fields? Need calibration and computational feasibility.

### OPEN-009 — Cross-capture registration

How should repeated captures align under seasonal change, moved objects, new calibration, and weak
geodetic priors while distinguishing physical change from model reinterpretation?

### OPEN-010 — Semantic ontology granularity

How fine should utility/equipment taxonomy become before data and resolution gates support it? Too
coarse is not useful; too fine invites hallucinated specificity.

### OPEN-011 — Human confirmation UX

Which evidence views and questions let an owner confirm an asset accurately with minimal burden?
How is reviewer uncertainty represented?

### OPEN-012 — Agent query language

Typed JSON AST first is safe; when does a textual query language produce enough token/usability
benefit to justify a parser/optimizer?

### OPEN-013 — Archive transport implementation

Asupersync-native HTTP/TLS/S3 in Rust versus tightly scoped process sidecar versus ATP gateway. The
choice must satisfy closed-dependency, credential, cancellation, performance, and portability
requirements.

### OPEN-014 — RaptorQ allocation policy

Which object classes and repair ratios minimize expected loss/retrieval cost under local/B2/R2
failure models?

### OPEN-015 — Safe public sharing

Can useful geometry/coverage reports be shared while removing location, textures, neighboring
property, and critical assets? Need measurable re-identification/privacy tests.

### OPEN-016 — Live model architecture

DA3 Streaming, CUT3R, another successor, or a classical/learned hybrid? Exact decision should be
renewed at implementation time using current primary sources and FDGR benchmarks.

### OPEN-017 — Pure-Rust media hot paths

Which demux/decode/image operations are worth owning in safe Rust after profiling, and which remain
best isolated in ffmpeg?

### OPEN-018 — Formal methods scope

Which state machines merit TLA+/Lean/model checking first: publication, idempotency/effect
reconciliation, archive deletion, capability noninterference, or scale/claim transitions?

## 133. Research agenda

Near-term experiments that inform architecture without bypassing prerequisites:

1. Collect exact DJI Flip originals under controlled stationary/slow/orbit captures; inventory
   container metadata, frame rate, crop, stabilization, and telemetry.
2. Compare DJI original, DJI Fly/controller recording, and OS/capture-card preview pixel/timeline
   relationships.
3. Calibrate the Flip camera under several modes and quantify fixed-lens/crop/rolling-shutter
   stability.
4. Create a small measured facade/property fixture with targets and LiDAR/reference geometry.
5. Run MapAnything Apache, DA3 Streaming, classical SfM/MVS, and permitted challengers on the same
   immutable frame set.
6. Measure model pose/depth, classical refinement gain, scale behavior, thin structures, foliage,
   glass, repetitive siding, and compute.
7. Test Qwen3.8 and segmentation on a narrowly defined asset ontology; measure observation versus
   resolved precision and counterevidence.
8. Prototype content-addressed original/media/geometry manifests and B2/R2 multipart/readback.
9. Build the publication/effect state machines under virtual time before optimizing.
10. Measure agent token/round-trip savings from anchored summaries, search, and explanation versus
    raw media/frame dumps.

Research outputs enter the negative-evidence ledger even when they fail.

---

# Part XVI — Implementation order and vertical slices

## 134. First vertical slice: agent-driven evidence-grade recorded capture

The first useful slice deliberately contains no neural model and exercises the full semantic waist:

```text
fdgr.open_session
→ fdgr.orient (bootstrap four-ledger packet)
→ create objective: preserve and derive relative reference geometry from one video
→ questions: source completeness? clock continuity? calibration scope? relative pose connectivity?
→ fdgr.propose exact ingest/reference-reconstruction candidate
→ fdgr.commit
→ exact byte import and source receipt
→ ffprobe sidecar and packet/frame timeline
→ deterministic keyframes, tracks, relative cameras/points
→ fdgr.watch semantic progress and blockers
→ verify publication and relative-only export
→ publish episode, surprises, context pack, report, checkpoint, and handoff
→ cancellation/crash/replay tests
```

Acceptance:

- a cold agent understands mission, anchor, questions, active work, limitations, and safest step;
- exact original survives and every derived frame cites original time/range;
- no metric unit appears;
- malformed media, gaps, errors, and cancellation preserve the Agent Turn Packet;
- root publishes atomically;
- replay produces identical plan/manifest/reference result;
- context pack states omissions and report states limitations;
- a second agent resumes from handoff without transcript replay.

This slice proves both the truth architecture and agent operating model before sophistication.

## 135. Second vertical slice: witnessed metric exterior

Add:

- controlled calibration;
- measured fiducial/segment scale witness;
- MapAnything proposal lane;
- pose/depth refinement;
- surfel/mesh fusion;
- held-out and measured validation;
- coverage certificate;
- metric measurement with interval;
- B2/R2 verified replica and restore.

Acceptance is one small measured scene, not universal home support.

## 136. Third vertical slice: semantic asset

Choose one bounded concept, likely an exterior door/window or HVAC outdoor unit:

- ontology and gate;
- Qwen and segmentation observations;
- multi-view geometry association;
- counterexample corpus;
- human confirmation;
- resolved asset with dimensions/provenance;
- absence refusal outside coverage;
- agent query/explain/report.

Only after this works should the ontology widen to propane/electrical/water assets.

## 137. Fourth vertical slice: live preview to original convergence

Add one exact owner-authorized live/mirrored profile:

- live source health/timeline;
- provisional online pose/depth/coverage;
- manual next-view suggestion;
- capture drain;
- later exact original import;
- preview/original alignment;
- offline successor geometry;
- semantic revalidation;
- history comparison and agent notification.

This slice demonstrates the core product loop without autonomous flight.

## 138. Fifth vertical slice: multi-agent and repeated capture

Add:

- branch-per-agent/model experiments and common-basis candidate comparison;
- objective ownership, semantic reservations, leases/fences, and visible obligations;
- shared question bundles and duplicate-work detection;
- scene/search/context projections and Pack DNA;
- eleven-operation MCP narrow waist;
- sealed handoff/resume and Eidetic export;
- second-date capture registration;
- physical-change versus reinterpretation questions/claims;
- graph-aware retention/deletion.

## 138A. Sixth vertical slice: evidence-gated accretion

Run repeated capture and reconstruction episodes under stable workload classes:

- retain predicted and actual evidence gain, latency, resources, operator/flight cost, and outcome;
- create surprise and lesson candidates;
- replay and compare candidate ranking offline;
- shadow a proposed context/recommendation policy;
- canary a new policy epoch under hard clamps;
- prove rollback and no regression in scale, coverage, privacy, custody, or recovery;
- demonstrate lower total control cost on a held-out scenario set.

## 139. Do not optimize yet list

Until profiles show a wall, do not:

- write custom codecs merely to avoid ffmpeg;
- build a distributed service/control plane;
- add GPU inference in Rust;
- optimize mesh rendering before geometry validation;
- implement every model paper;
- build a custom textual query language;
- ingest graph/search before canonical claims exist;
- automate flight;
- shard the ledger;
- design arbitrary plugin ABI;
- claim 1.0 accuracy from a visually good private example.

The sophisticated part of FDGR is semantic composition, not premature platform breadth.

---

# Appendix A — Example evidence lineage

```text
CaptureLineage home-2026-08-30
├── Epoch 1: DJI controller preview
│   ├── source profile dji-flip/rc-n3/fly-X/ios-Y
│   ├── packet/frame/clock generation P1
│   ├── calibration C-preview-1
│   ├── online geometry G-live-7 (relative_only, provisional)
│   ├── coverage K-live-7
│   └── semantic hypothesis H-hvac-3
├── Epoch 2: aircraft original imported after flight
│   ├── raw object O-original (verified local + remote)
│   ├── packet/frame/clock generation P2
│   ├── calibration C-original-2
│   ├── preview/original alignment A1
│   ├── MapAnything proposal M1
│   ├── classical tracks/pose graph R1
│   ├── measured target scale witness S1
│   ├── refined geometry G-offline-12 (witnessed metric)
│   ├── coverage K-offline-12
│   ├── semantic observations Q1/SAM1/human1
│   ├── resolved HVAC outdoor asset H-hvac-8
│   └── digital twin T12
└── Later branch: calibration experiment C-original-3
    ├── geometry G-branch-13
    ├── comparison receipt
    └── rejected merge because held-out error worsened
```

The live branch remains inspectable. The offline branch supersedes selected claims but does not
rewrite history.

# Appendix B — Example measurement response

```json
{
  "schema": "fdgr.measurement.v1",
  "status": "complete",
  "anchorRoot": "<digest>",
  "geometry_root": "<digest>",
  "subject": {"assetId": "asset.window.17", "feature": "clear_opening_width"},
  "method": "robust_parallel_edge_fit",
  "coordinate_frame": "building.local.v1",
  "value": 1.214,
  "unit": "m",
  "interval95": [1.196, 1.233],
  "scale_status": "witnessed",
  "scale_witness_root": "<digest>",
  "supporting_evidence": ["<frame/mask/surface roots>"],
  "warnings": ["lower sill partially occluded"],
  "replay": "fdgr geometry measure --request-root <digest> --strict"
}
```

If scale is relative or estimated, `value` cannot be serialized with `unit: m`; the response is
blocked or uses relative units with an explicit status.

# Appendix C — Example semantic explanation

```text
Claim: asset H8 is a resolved hvac.outdoor_condenser on the east facade.
Basis: twin T12, geometry G12, ontology O3, policy P5.
Support:
  - 11 observations across 4 original frames and 3 view directions.
  - persistent segmented cabinet/fan region associated with surface tile E-22.
  - witnessed dimensions 0.91 × 0.88 × 0.86 m, interval shown.
  - visible fan grille and line-set observations.
  - Qwen profile Q38-27B-P2 and detector D4 agree at observation level.
  - owner confirmation receipt HCONF-2.
Counterevidence considered:
  - horizontal propane tank and generator alternatives rejected by geometry/visual cues.
  - one live-preview crop produced an inconsistent mask and was excluded after original alignment.
Limitations:
  - indoor HVAC equipment and functional status are unknown.
  - electrical/line-set connections are only partially visible.
What would change the claim:
  - contradictory close original view, geometry reassociation, ontology change, or human rejection.
```

# Appendix D — Model admission checklist

```text
[ ] exact code/weight/environment/driver roots
[ ] license and acceptable-use receipt per artifact
[ ] offline provisioning and no-network default
[ ] bounded process, mounts, environment, outputs, descendants
[ ] request/result schema and coordinate/crop tests
[ ] finite/numeric/size adversarial tests
[ ] repeated-run determinism envelope
[ ] UAV/home/rolling-shutter/foliage/glass/repetitive-texture corpus
[ ] held-out reprojection and ground-truth geometry
[ ] uncertainty/confidence calibration
[ ] correlation/lineage class
[ ] compute/memory/energy/economic receipt
[ ] typed degraded behavior and fallback
[ ] compatibility matrix row and expiry trigger
[ ] negative evidence retained
```

# Appendix E — Public claim template

```text
Claim ID:
Release/build/profile:
Device/source/model/provider scope:
Semantic statement:
Evidence receipt roots:
Positive dimensions earned:
Negative evidence and exclusions:
Determinism class:
Corpus and sample count:
Expiry/requalification trigger:
Reproduction command:
```

# Appendix F — Definition of “alien artifact” for this project

FDGR earns that description only when the mechanisms compose into behavior that ordinary pipelines
do not provide:

- a capture can crash midway and still explain exactly which bytes/frames exist;
- a live preview can guide a flight, then be superseded by original media without losing lineage;
- a model can be replaced, contradicted, or removed without corrupting evidence;
- metric dimensions are impossible without scale proof;
- semantic claims expose counterevidence and refuse hidden/absent assertions;
- geometry, semantics, reports, search, graph, and memory all cite the same immutable roots;
- cloud storage can be cheap because identity, repair, and restore are owned above the provider;
- cancellation cannot silently orphan work;
- every important failure can be replayed under virtual time;
- agents can operate economically without being granted arbitrary authority;
- new algorithms improve old captures through branches rather than destructive migration;
- public claims are limited by retained evidence rather than enthusiasm.

That is the standard. A fast point-cloud demo is the beginning of experimentation, not the end of
the project.

<!-- BEGIN GENERATED REGISTRY TRACEABILITY -->
# Appendix G — Machine Registry Traceability Index

This index is generated from the TOML registries. It proves that every published machine
identifier has a stable human-plan landing point; registry content remains normative when
a compact label below omits detail.

| Stable ID | Normative registry | Compact label |
|---|---|---|
| `ADR-0001` | `registries/adrs.toml` | Three-plane authority |
| `ADR-0002` | `registries/adrs.toml` | Process sidecars |
| `ADR-0003` | `registries/adrs.toml` | Metric scale witnesses |
| `ADR-0004` | `registries/adrs.toml` | Original evidence custody |
| `ADR-0005` | `registries/adrs.toml` | Agent operating model is the system center |
| `ADR-0006` | `registries/adrs.toml` | Question-first cognitive graph |
| `ADR-0007` | `registries/adrs.toml` | Canonical Agent Turn Packet |
| `ADR-0008` | `registries/adrs.toml` | Context packs require Pack DNA |
| `ADR-0009` | `registries/adrs.toml` | Episode and surprise based accretion |
| `ADR-0010` | `registries/adrs.toml` | Eleven-operation semantic narrow waist |
| `ADR-0011` | `registries/adrs.toml` | One canonical machine vocabulary |
| `BET-001` | `registries/doctrine.toml` | One evidence universe |
| `BET-002` | `registries/doctrine.toml` | Metric scale is a proof obligation |
| `BET-003` | `registries/doctrine.toml` | Live draft and offline convergence are one lineage |
| `BET-004` | `registries/doctrine.toml` | Models propose, geometry adjudicates |
| `BET-005` | `registries/doctrine.toml` | Semantics are resolved from an evidence graph |
| `BET-006` | `registries/doctrine.toml` | Coverage makes missing knowledge explicit |
| `BET-007` | `registries/doctrine.toml` | Immutable content addressing makes large evidence economical |
| `BET-008` | `registries/doctrine.toml` | Determinism is a product feature |
| `BET-009` | `registries/doctrine.toml` | Agent-native is an architectural constraint |
| `BET-010` | `registries/doctrine.toml` | Privacy scope travels with evidence |
| `BET-011` | `registries/doctrine.toml` | Questions are the cognitive control plane |
| `BET-012` | `registries/doctrine.toml` | One agent turn packet |
| `CAP-ADMIN-POLICY` | `registries/capabilities.toml` | modify registries/policies through a separately audited path |
| `CAP-AGENT-ORIENT` | `registries/capabilities.toml` | read an anchor-bound four-ledger orientation projection within privacy and token budgets |
| `CAP-AGENT-QUERY` | `registries/capabilities.toml` | read bounded canonical or certified-derived facts and expand typed handles |
| `CAP-ARCHIVE-DELETE` | `registries/capabilities.toml` | apply an independently sealed deletion plan |
| `CAP-ARCHIVE-READ` | `registries/capabilities.toml` | retrieve named immutable objects |
| `CAP-ARCHIVE-WRITE` | `registries/capabilities.toml` | create named immutable remote objects |
| `CAP-BRANCH-SPECULATE` | `registries/capabilities.toml` | create and evaluate authority-free counterfactual branches over a pinned anchor |
| `CAP-CAPTURE-LIVE-OBSERVE` | `registries/capabilities.toml` | open a read-only live source session for an admitted profile |
| `CAP-CAPTURE-READ` | `registries/capabilities.toml` | read named source files/streams within a sealed scope |
| `CAP-DEVICE-CONTROL` | `registries/capabilities.toml` | reserved; not admitted in the initial architecture |
| `CAP-DEVICE-OBSERVE` | `registries/capabilities.toml` | run bounded compatibility/read probes |
| `CAP-DIAGNOSTIC-FORENSIC` | `registries/capabilities.toml` | read bounded raw diagnostic and evidence surfaces within explicit privacy scope |
| `CAP-EPISODE-APPEND` | `registries/capabilities.toml` | append a validated immutable episode and surprise capsule to the operational evidence lineage |
| `CAP-EVIDENCE-APPEND` | `registries/capabilities.toml` | append validated capsule types to a lineage |
| `CAP-GENERATION-PUBLISH` | `registries/capabilities.toml` | publish a verified immutable root |
| `CAP-HANDOFF-PUBLISH` | `registries/capabilities.toml` | publish an authority-free sealed handoff capsule for a named session/campaign |
| `CAP-HUMAN-CONFIRM` | `registries/capabilities.toml` | attach a scoped human confirmation receipt |
| `CAP-PILOT-GUIDANCE` | `registries/capabilities.toml` | Human pilot guidance |
| `CAP-PLAN-COMMIT` | `registries/capabilities.toml` | commit a sealed plan after current witness, policy, capability, idempotency, lease, and fence validation |
| `CAP-PRIVACY-EXPORT` | `registries/capabilities.toml` | produce a named lower-sensitivity derivative |
| `CAP-PROCESS-MEDIA` | `registries/capabilities.toml` | spawn exact admitted media profiles |
| `CAP-PROCESS-MODEL` | `registries/capabilities.toml` | spawn exact admitted model profiles |
| `CAP-SPATIAL-EXPAND` | `registries/capabilities.toml` | Spatial handle expansion |
| `CLAIM-ABS-001` | `registries/claims.toml` | Asset absent from domain |
| `CLAIM-ARCH-001` | `registries/claims.toml` | Cloud replica retrievable |
| `CLAIM-CAL-001` | `registries/claims.toml` | Camera calibration admitted |
| `CLAIM-GEO-001` | `registries/claims.toml` | Surface geometry admitted |
| `CLAIM-POSE-001` | `registries/claims.toml` | Camera pose generation admitted |
| `CLAIM-RAW-001` | `registries/claims.toml` | Original bytes retained |
| `CLAIM-READY-001` | `registries/claims.toml` | Digital twin release qualified |
| `CLAIM-SCALE-001` | `registries/claims.toml` | Metric scale witnessed |
| `CLAIM-SEM-001` | `registries/claims.toml` | Semantic asset resolved |
| `CLAIM-TIME-001` | `registries/claims.toml` | Frame timeline continuous |
| `EFFECT-CLOUD-001` | `registries/effects.toml` | Upload immutable object |
| `EFFECT-CLOUD-002` | `registries/effects.toml` | Delete archive object |
| `EFFECT-DEVICE-001` | `registries/effects.toml` | Open owner-authorized device session |
| `EFFECT-DEVICE-002` | `registries/effects.toml` | Send aircraft control command |
| `EFFECT-EPISODE-001` | `registries/effects.toml` | Append episode capsule |
| `EFFECT-HANDOFF-001` | `registries/effects.toml` | Publish handoff capsule |
| `EFFECT-INGEST-001` | `registries/effects.toml` | Read source media |
| `EFFECT-MODEL-001` | `registries/effects.toml` | Spawn model worker |
| `EFFECT-PLAN-001` | `registries/effects.toml` | Commit sealed semantic plan |
| `EFFECT-PROCESS-001` | `registries/effects.toml` | Spawn media sidecar |
| `EFFECT-PUBLISH-001` | `registries/effects.toml` | Publish immutable generation |
| `ERR-AFFORDANCE-UNAVAILABLE` | `registries/errors.toml` | The requested action family is blocked or degraded; the response names unmet grants, evidence, compatibility, or confirmation requirements. |
| `ERR-ANCHOR-MIXED-GENERATION` | `registries/errors.toml` | Inputs came from incompatible capture, clock, calibration, model, ontology, or policy generations. |
| `ERR-ARCHIVE-UNVERIFIED` | `registries/errors.toml` | The remote object has not passed provider-independent retrieval and digest verification. |
| `ERR-BUDGET-EXHAUSTED` | `registries/errors.toml` | The operation exhausted a declared CPU, GPU, memory, I/O, network, storage, or time budget. |
| `ERR-CONTEXT-PACK-INCOMPLETE` | `registries/errors.toml` | The context budget admitted only a complete decision-useful prefix; omissions and an anchor-bound continuation are provided. |
| `ERR-CONTINUITY-GAP` | `registries/errors.toml` | The requested basis cannot be advanced to the target anchor without a gap or reset. |
| `ERR-COVERAGE-UNCERTIFIED` | `registries/errors.toml` | The requested completeness or absence claim lacks a sufficient coverage certificate. |
| `ERR-DEVICE-AUTHORITY` | `registries/errors.toml` | The operation lacks an owner-authorized device capability. |
| `ERR-DEVICE-PROFILE-UNKNOWN` | `registries/errors.toml` | The aircraft, controller, application, firmware, operating system, or region profile is unknown. |
| `ERR-EFFECT-INDETERMINATE` | `registries/errors.toml` | An external effect may have occurred but cannot yet be safely classified. |
| `ERR-HANDOFF-STALE` | `registries/errors.toml` | The handoff anchor is no longer directly resumable; current orientation and invalidated assumptions are required. |
| `ERR-IDENTITY-NONCANONICAL` | `registries/errors.toml` | An identity was not canonical lowercase domain-separated SHA-256 text. |
| `ERR-MODEL-OUTPUT-INVALID` | `registries/errors.toml` | A model output failed schema, numeric, coordinate, basis, or digest validation. |
| `ERR-MODEL-UNADMITTED` | `registries/errors.toml` | The requested model artifact, license, revision, or execution profile is not admitted. |
| `ERR-PLAN-STALE` | `registries/errors.toml` | A plan witness or policy basis changed before commit and the sealed plan must be replayed or rejected. |
| `ERR-PRIVACY-BLOCKED` | `registries/errors.toml` | Publication or model execution would exceed the admitted privacy scope. |
| `ERR-PUBLICATION-INCOMPLETE` | `registries/errors.toml` | A publication root referenced missing, unverified, or incompatible children. |
| `ERR-QUESTION-BLOCKED` | `registries/errors.toml` | The question cannot reach its terminal predicate under current evidence, grants, compatibility, or budget. |
| `ERR-RECOMMENDATION-NONE` | `registries/errors.toml` | No action has positive admissible value under the current objective, evidence, risk, and budget; waiting or stopping is intentional. |
| `ERR-SCALE-UNWITNESSED` | `registries/errors.toml` | A metric claim was requested without an admitted scale witness. |
| `ERR-TIME-DISCONTINUITY` | `registries/errors.toml` | A timestamp or packet discontinuity cannot be represented within the current clock epoch. |
| `GALG-001` | `registries/graph_algorithms.toml` | components-and-dynamic-connectivity |
| `GALG-002` | `registries/graph_algorithms.toml` | articulation-bridges-biconnectivity |
| `GALG-003` | `registries/graph_algorithms.toml` | scc-condensation-cycle-diagnosis |
| `GALG-004` | `registries/graph_algorithms.toml` | topological-critical-path-antichain |
| `GALG-005` | `registries/graph_algorithms.toml` | shortest-widest-minimax-k-shortest |
| `GALG-006` | `registries/graph_algorithms.toml` | dominators-postdominators |
| `GALG-007` | `registries/graph_algorithms.toml` | spanning-forests |
| `GALG-008` | `registries/graph_algorithms.toml` | cycle-bases-consistency |
| `GALG-009` | `registries/graph_algorithms.toml` | max-flow-min-cut-gomory-hu |
| `GALG-010` | `registries/graph_algorithms.toml` | min-cost-flow-circulation |
| `GALG-011` | `registries/graph_algorithms.toml` | bipartite-weighted-bottleneck-matching |
| `GALG-012` | `registries/graph_algorithms.toml` | compatibility-clique-independent-set-coloring |
| `GALG-013` | `registries/graph_algorithms.toml` | k-core-k-truss-degeneracy |
| `GALG-014` | `registries/graph_algorithms.toml` | centrality-pagerank-hits-ppr |
| `GALG-015` | `registries/graph_algorithms.toml` | community-partition-hypotheses |
| `GALG-016` | `registries/graph_algorithms.toml` | spectral-diagnostics |
| `GALG-017` | `registries/graph_algorithms.toml` | isomorphism-subgraph-motifs |
| `GALG-018` | `registries/graph_algorithms.toml` | graph-edit-change |
| `GALG-019` | `registries/graph_algorithms.toml` | treewidth-elimination-order |
| `GALG-020` | `registries/graph_algorithms.toml` | temporal-graphs |
| `GALG-021` | `registries/graph_algorithms.toml` | submodular-selection |
| `GATE-000` | `registries/gates.toml` | Constitution frozen |
| `GATE-001` | `registries/gates.toml` | Reference identity and ledger |
| `GATE-002` | `registries/gates.toml` | Crash-safe evidence publication |
| `GATE-003` | `registries/gates.toml` | Media normalization qualified |
| `GATE-004` | `registries/gates.toml` | Calibration and scale honest |
| `GATE-005` | `registries/gates.toml` | Recorded capture useful |
| `GATE-006` | `registries/gates.toml` | Live observation admitted |
| `GATE-007` | `registries/gates.toml` | Reference geometry oracle |
| `GATE-008` | `registries/gates.toml` | Model lanes admitted |
| `GATE-009` | `registries/gates.toml` | Refined geometry stable |
| `GATE-010` | `registries/gates.toml` | Digital surface model useful |
| `GATE-011` | `registries/gates.toml` | Semantic twin trustworthy |
| `GATE-012` | `registries/gates.toml` | Archive recoverable |
| `GATE-013` | `registries/gates.toml` | Human and agent surfaces complete |
| `GATE-014` | `registries/gates.toml` | Agent integration non-bypassable |
| `GATE-015` | `registries/gates.toml` | Fault and benchmark evidence |
| `GATE-016` | `registries/gates.toml` | Performance claims earned |
| `GATE-017` | `registries/gates.toml` | Release qualified |
| `GATE-018` | `registries/gates.toml` | Agent semantics contract locked |
| `GATE-019` | `registries/gates.toml` | Cold orientation and context packs qualified |
| `GATE-020` | `registries/gates.toml` | Question-driven planning qualified |
| `GATE-021` | `registries/gates.toml` | Active work and handoff qualified |
| `GATE-022` | `registries/gates.toml` | Accretion qualified |
| `GATE-023` | `registries/gates.toml` | Agent-native release qualified |
| `GATE-024` | `registries/gates.toml` | Shared cockpit and pilot protocol qualified |
| `GEOM-001` | `registries/geometry_algorithms.toml` | clock-map-estimation |
| `GEOM-002` | `registries/geometry_algorithms.toml` | calibration |
| `GEOM-003` | `registries/geometry_algorithms.toml` | frame-quality |
| `GEOM-004` | `registries/geometry_algorithms.toml` | keyframe-selection |
| `GEOM-005` | `registries/geometry_algorithms.toml` | features-lines-planes |
| `GEOM-006` | `registries/geometry_algorithms.toml` | association-and-tracks |
| `GEOM-007` | `registries/geometry_algorithms.toml` | robust-model-selection |
| `GEOM-008` | `registries/geometry_algorithms.toml` | local-pose |
| `GEOM-009` | `registries/geometry_algorithms.toml` | loop-closure |
| `GEOM-010` | `registries/geometry_algorithms.toml` | global-pose-bundle-adjustment |
| `GEOM-011` | `registries/geometry_algorithms.toml` | scale-resolution |
| `GEOM-012` | `registries/geometry_algorithms.toml` | depth-multiview |
| `GEOM-013` | `registries/geometry_algorithms.toml` | spatial-fusion |
| `GEOM-014` | `registries/geometry_algorithms.toml` | mesh-extraction |
| `GEOM-015` | `registries/geometry_algorithms.toml` | structural-topology |
| `GEOM-016` | `registries/geometry_algorithms.toml` | change-registration |
| `GEOM-017` | `registries/geometry_algorithms.toml` | coverage-detectability |
| `GEOM-018` | `registries/geometry_algorithms.toml` | next-best-view |
| `GOAL-001` | `registries/doctrine.toml` | Source-agnostic exact ingest |
| `GOAL-002` | `registries/doctrine.toml` | Fast useful feedback |
| `GOAL-003` | `registries/doctrine.toml` | High-quality converged geometry |
| `GOAL-004` | `registries/doctrine.toml` | Honest metric scale |
| `GOAL-005` | `registries/doctrine.toml` | Semantic digital twin |
| `GOAL-006` | `registries/doctrine.toml` | Explicit incompleteness |
| `GOAL-007` | `registries/doctrine.toml` | Durable and economical custody |
| `GOAL-008` | `registries/doctrine.toml` | Crash and cancellation correctness |
| `GOAL-009` | `registries/doctrine.toml` | Agent efficiency |
| `GOAL-010` | `registries/doctrine.toml` | Deterministic diagnosis |
| `GOAL-011` | `registries/doctrine.toml` | Closed, safe, auditable Rust core |
| `GOAL-012` | `registries/doctrine.toml` | Continual reinterpretation |
| `GOAL-013` | `registries/doctrine.toml` | First-try agent operation |
| `GOAL-014` | `registries/doctrine.toml` | Evidence-gated accretion |
| `INV-001` | `registries/invariants.toml` | Anchored claims only |
| `INV-002` | `registries/invariants.toml` | Original evidence is immutable |
| `INV-003` | `registries/invariants.toml` | One version universe |
| `INV-004` | `registries/invariants.toml` | No mixed generations |
| `INV-005` | `registries/invariants.toml` | Metric claims require scale witnesses |
| `INV-006` | `registries/invariants.toml` | Model output is a proposal |
| `INV-007` | `registries/invariants.toml` | Cognition cannot dispatch effects |
| `INV-008` | `registries/invariants.toml` | Acknowledgement is not completion |
| `INV-009` | `registries/invariants.toml` | Reserve materialize verify publish |
| `INV-010` | `registries/invariants.toml` | Cancellation drains |
| `INV-011` | `registries/invariants.toml` | No orphan work |
| `INV-012` | `registries/invariants.toml` | Deterministic decision replay |
| `INV-013` | `registries/invariants.toml` | Negative claims are witnessed |
| `INV-014` | `registries/invariants.toml` | Original capture retained |
| `INV-015` | `registries/invariants.toml` | External executables are untrusted |
| `INV-016` | `registries/invariants.toml` | Secrets never enter evidence |
| `INV-017` | `registries/invariants.toml` | Local qualification is authoritative |
| `INV-018` | `registries/invariants.toml` | Safe Rust trust domain |
| `INV-019` | `registries/invariants.toml` | Closed dependency universe |
| `INV-020` | `registries/invariants.toml` | Adaptivity cannot weaken proof |
| `INV-021` | `registries/invariants.toml` | Cloud is not authoritative before verification |
| `INV-022` | `registries/invariants.toml` | Appearance is not canonical geometry |
| `INV-023` | `registries/invariants.toml` | Agent memory is advisory |
| `INV-024` | `registries/invariants.toml` | Critical assets require elevated evidence |
| `INV-025` | `registries/invariants.toml` | Owned-device authorization |
| `INV-026` | `registries/invariants.toml` | Coordinate conventions are explicit |
| `INV-027` | `registries/invariants.toml` | Clock discontinuities create epochs |
| `INV-028` | `registries/invariants.toml` | License admission is per artifact |
| `INV-029` | `registries/invariants.toml` | Finite numeric domain |
| `INV-030` | `registries/invariants.toml` | Authority narrows across seams |
| `INV-031` | `registries/invariants.toml` | One agent operating loop |
| `INV-032` | `registries/invariants.toml` | Agent turn spine everywhere |
| `INV-033` | `registries/invariants.toml` | Questions name proof obligations |
| `INV-034` | `registries/invariants.toml` | Four ledgers are synchronized projections |
| `INV-035` | `registries/invariants.toml` | Active work survives context loss |
| `INV-036` | `registries/invariants.toml` | Recommendations are authority free |
| `INV-037` | `registries/invariants.toml` | Context packs explain themselves |
| `INV-038` | `registries/invariants.toml` | Budget preserves safety |
| `INV-039` | `registries/invariants.toml` | Plan alternatives share a basis |
| `INV-040` | `registries/invariants.toml` | Surprise is explicit |
| `INV-041` | `registries/invariants.toml` | Learning is promoted not improvised |
| `INV-042` | `registries/invariants.toml` | Handoff is sufficient and authority free |
| `INV-043` | `registries/invariants.toml` | Self-description matches reality |
| `INV-044` | `registries/invariants.toml` | Pilot guidance states evidence purpose |
| `INV-045` | `registries/invariants.toml` | Do nothing is a valid recommendation |
| `INV-046` | `registries/invariants.toml` | One Decision Frame per decision |
| `INV-047` | `registries/invariants.toml` | Epistemic debt is explicit |
| `INV-048` | `registries/invariants.toml` | Attention is stable |
| `INV-049` | `registries/invariants.toml` | Spatial handles are frame complete |
| `INV-050` | `registries/invariants.toml` | Pilot guidance is human mediated |
| `INV-051` | `registries/invariants.toml` | One canonical wire vocabulary |
| `INV-052` | `registries/invariants.toml` | Outcome and recovery are explicit |
| `INV-053` | `registries/invariants.toml` | One complete anchor schema |
| `MODEL-GEO-CUT3R` | `registries/models.toml` | CUT3R |
| `MODEL-GEO-DA3-STREAMING` | `registries/models.toml` | Depth Anything 3 |
| `MODEL-GEO-MAPANYTHING-APACHE` | `registries/models.toml` | MapAnything |
| `MODEL-GEO-VGGT-OMEGA-1B` | `registries/models.toml` | VGGT-Omega |
| `MODEL-SEG-SAM31` | `registries/models.toml` | SAM 3.1 |
| `MODEL-SEM-QWEN38-27B` | `registries/models.toml` | Qwen3.8 |
| `NONGOAL-001` | `registries/doctrine.toml` | Autonomous flight in the initial architecture |
| `NONGOAL-002` | `registries/doctrine.toml` | Bypassing vendor controls |
| `NONGOAL-003` | `registries/doctrine.toml` | Survey/legal certainty by default |
| `NONGOAL-004` | `registries/doctrine.toml` | Inferring hidden utilities as observed facts |
| `NONGOAL-005` | `registries/doctrine.toml` | One-model monoculture |
| `NONGOAL-006` | `registries/doctrine.toml` | One-vendor capture lock-in |
| `NONGOAL-007` | `registries/doctrine.toml` | Cloud requirement |
| `NONGOAL-008` | `registries/doctrine.toml` | Photorealism as geometry proof |
| `NONGOAL-009` | `registries/doctrine.toml` | Hidden omniscience for agents |
| `NONGOAL-010` | `registries/doctrine.toml` | General-purpose media/robotics platform |
| `OP-AGENT-CANCEL` | `registries/agent_operations.toml` | Request cancellation and report drain, compensation, and reconciliation. |
| `OP-AGENT-COMMIT` | `registries/agent_operations.toml` | Revalidate and commit one sealed plan, creating owned obligations. |
| `OP-AGENT-COMPARE` | `registries/agent_operations.toml` | Compare candidates or branches on one basis and expose a Pareto frontier. |
| `OP-AGENT-DOCTOR` | `registries/agent_operations.toml` | Diagnose compatibility, custody, compute, model, policy, and qualification posture. |
| `OP-AGENT-EXPLAIN` | `registries/agent_operations.toml` | Traverse claims, decisions, scores, witnesses, omissions, and evidence. |
| `OP-AGENT-HANDOFF` | `registries/agent_operations.toml` | Create or resume a compact sealed authority-free continuity capsule. |
| `OP-AGENT-OPEN-SESSION` | `registries/agent_operations.toml` | Negotiate protocol, lineage, grants, profiles, budgets, and continuity basis. |
| `OP-AGENT-ORIENT` | `registries/agent_operations.toml` | Return the smallest sufficient four-ledger briefing and ranked attention. |
| `OP-AGENT-PROPOSE` | `registries/agent_operations.toml` | Compile an objective or next-step template into sealed candidate plans. |
| `OP-AGENT-QUERY` | `registries/agent_operations.toml` | Answer a bounded question or expand typed handles with coverage and provenance. |
| `OP-AGENT-WATCH` | `registries/agent_operations.toml` | Return semantic progress and terminal transitions for active work. |
| `OP-ATTENTION-REFRESH` | `registries/operation_costs.toml` | Attention and epistemic-debt refresh |
| `OP-BUNDLE-ADJUST` | `registries/operation_costs.toml` | operation |
| `OP-CLOUD-REPLICATE` | `registries/operation_costs.toml` | operation |
| `OP-CONTEXT-PACK` | `registries/operation_costs.toml` | operation |
| `OP-DECISION-FRAME` | `registries/operation_costs.toml` | Decision Frame build |
| `OP-DECODE-GOP` | `registries/operation_costs.toml` | operation |
| `OP-EPISODE-CLOSE` | `registries/operation_costs.toml` | operation |
| `OP-FUSE-GEOMETRY` | `registries/operation_costs.toml` | operation |
| `OP-HANDOFF` | `registries/operation_costs.toml` | operation |
| `OP-INGEST-CHUNK` | `registries/operation_costs.toml` | operation |
| `OP-KEYFRAME-SELECT` | `registries/operation_costs.toml` | operation |
| `OP-MODEL-INFER` | `registries/operation_costs.toml` | operation |
| `OP-ORIENT-COST` | `registries/operation_costs.toml` | operation |
| `OP-PILOT-CARD` | `registries/operation_costs.toml` | Human pilot card |
| `OP-PILOT-GUIDE` | `registries/operation_costs.toml` | operation |
| `OP-PLAN-COMPARE` | `registries/operation_costs.toml` | operation |
| `OP-PROFILE-BRIEFING` | `registries/agent_profiles.toml` | briefing |
| `OP-PROFILE-CUSTOM` | `registries/agent_profiles.toml` | custom |
| `OP-PROFILE-FORENSIC` | `registries/agent_profiles.toml` | forensic |
| `OP-PROFILE-PILOT` | `registries/agent_profiles.toml` | pilot |
| `OP-PROFILE-PULSE` | `registries/agent_profiles.toml` | pulse |
| `OP-PROFILE-TACTICAL` | `registries/agent_profiles.toml` | tactical |
| `OP-QUESTION-EVALUATE` | `registries/operation_costs.toml` | operation |
| `OP-SEMANTIC-RESOLVE` | `registries/operation_costs.toml` | operation |
| `OP-SPATIAL-EXPAND` | `registries/operation_costs.toml` | Spatial handle expansion |
| `OPEN-001` | `registries/open_questions.toml` | Canonical internal world frame |
| `OPEN-002` | `registries/open_questions.toml` | Canonical binary codec |
| `OPEN-003` | `registries/open_questions.toml` | Logical versus representation identity |
| `OPEN-004` | `registries/open_questions.toml` | Best analysis mezzanine |
| `OPEN-005` | `registries/open_questions.toml` | DJI telemetry availability |
| `OPEN-006` | `registries/open_questions.toml` | Model execution portability |
| `OPEN-007` | `registries/open_questions.toml` | Geometry canonical representation |
| `OPEN-008` | `registries/open_questions.toml` | Uncertainty representation |
| `OPEN-009` | `registries/open_questions.toml` | Cross-capture registration |
| `OPEN-010` | `registries/open_questions.toml` | Semantic ontology granularity |
| `OPEN-011` | `registries/open_questions.toml` | Human confirmation UX |
| `OPEN-012` | `registries/open_questions.toml` | Agent query language |
| `OPEN-013` | `registries/open_questions.toml` | Archive transport implementation |
| `OPEN-014` | `registries/open_questions.toml` | RaptorQ allocation policy |
| `OPEN-015` | `registries/open_questions.toml` | Safe public sharing |
| `OPEN-016` | `registries/open_questions.toml` | Live model architecture |
| `OPEN-017` | `registries/open_questions.toml` | Pure-Rust media hot paths |
| `OPEN-018` | `registries/open_questions.toml` | Formal methods scope |
| `OPEN-019` | `registries/open_questions.toml` | Question graph granularity |
| `OPEN-020` | `registries/open_questions.toml` | Context-pack objective |
| `OPEN-021` | `registries/open_questions.toml` | Recommendation utility representation |
| `OPEN-022` | `registries/open_questions.toml` | Episode causal evidence |
| `OPEN-023` | `registries/open_questions.toml` | Multi-agent branch economics |
| `OPEN-024` | `registries/open_questions.toml` | Pilot guidance presentation |
| `OPEN-025` | `registries/open_questions.toml` | Agent ergonomics benchmark |
| `OPEN-026` | `registries/open_questions.toml` | open_question |
| `OPEN-027` | `registries/open_questions.toml` | open_question |
| `OPEN-028` | `registries/open_questions.toml` | open_question |
| `RISK-001` | `registries/risks.toml` | DJI live access remains unavailable or unstable |
| `RISK-002` | `registries/risks.toml` | Monocular metric scale is unreliable |
| `RISK-003` | `registries/risks.toml` | Foundation models fail UAV/home domain |
| `RISK-004` | `registries/risks.toml` | Semantics overclaim critical assets |
| `RISK-005` | `registries/risks.toml` | Long videos overwhelm GPU/memory/storage |
| `RISK-006` | `registries/risks.toml` | Cumulative complexity prevents implementation |
| `RISK-007` | `registries/risks.toml` | Franken sibling APIs/readiness lag plan |
| `RISK-008` | `registries/risks.toml` | Strict no-unsafe/no-FFI limits performance |
| `RISK-009` | `registries/risks.toml` | Cloud compatibility is deceptively incomplete |
| `RISK-010` | `registries/risks.toml` | Privacy value conflicts with rich semantics |
| `RISK-011` | `registries/risks.toml` | Determinism conflicts with GPU/model throughput |
| `RISK-012` | `registries/risks.toml` | Calibration/rolling shutter dominates error |
| `RISK-013` | `registries/risks.toml` | Foliage/glass/water/repetitive texture defeats geometry |
| `RISK-014` | `registries/risks.toml` | Agent autonomy creates race or deletion hazards |
| `RISK-015` | `registries/risks.toml` | Agent packet becomes a second mutable truth |
| `RISK-016` | `registries/risks.toml` | Question graph creates planning ceremony |
| `RISK-017` | `registries/risks.toml` | Recommendation engine over-directs the operator |
| `RISK-018` | `registries/risks.toml` | Context compression hides decisive counterevidence |
| `RISK-019` | `registries/risks.toml` | Adaptive memory amplifies a local mistake |
| `RISK-020` | `registries/risks.toml` | Multi-agent optimization creates hidden ownership races |
| `RISK-021` | `registries/risks.toml` | Attention thrash |
| `RISK-022` | `registries/risks.toml` | Pilot cognitive overload |
| `RISK-023` | `registries/risks.toml` | Spatial handle drift |
| `SCHEMA-AGENT-TURN` | `registries/schemas.toml` | schemas/agent_turn.schema.json |
| `SCHEMA-ANCHOR-VECTOR` | `registries/schemas.toml` | schemas/anchor_vector.schema.json |
| `SCHEMA-ATTENTION-ITEM` | `registries/schemas.toml` | schemas/attention_item.schema.json |
| `SCHEMA-CAPTURE-ANCHOR` | `registries/schemas.toml` | schemas/capture_anchor.schema.json |
| `SCHEMA-CONTEXT-PACK` | `registries/schemas.toml` | schemas/context_pack.schema.json |
| `SCHEMA-DECISION-FRAME` | `registries/schemas.toml` | schemas/decision_frame.schema.json |
| `SCHEMA-DIGITAL-TWIN-MANIFEST` | `registries/schemas.toml` | schemas/digital_twin_manifest.schema.json |
| `SCHEMA-EPISODE-CAPSULE` | `registries/schemas.toml` | schemas/episode_capsule.schema.json |
| `SCHEMA-EVIDENCE-EVENT` | `registries/schemas.toml` | schemas/evidence_event.schema.json |
| `SCHEMA-GEOMETRY-GENERATION` | `registries/schemas.toml` | schemas/geometry_generation.schema.json |
| `SCHEMA-HANDOFF-CAPSULE` | `registries/schemas.toml` | schemas/handoff_capsule.schema.json |
| `SCHEMA-MODEL-WORKER-REQUEST` | `registries/schemas.toml` | schemas/model_worker_request.schema.json |
| `SCHEMA-OBJECTIVE` | `registries/schemas.toml` | schemas/objective.schema.json |
| `SCHEMA-OBLIGATION-PROGRESS` | `registries/schemas.toml` | schemas/obligation_progress.schema.json |
| `SCHEMA-PILOT-INSTRUCTION` | `registries/schemas.toml` | schemas/pilot_instruction.schema.json |
| `SCHEMA-PLAN-CANDIDATE` | `registries/schemas.toml` | schemas/plan_candidate.schema.json |
| `SCHEMA-QUESTION` | `registries/schemas.toml` | schemas/question.schema.json |
| `SCHEMA-SEMANTIC-OBSERVATION` | `registries/schemas.toml` | schemas/semantic_observation.schema.json |
| `SCHEMA-SPATIAL-HANDLE` | `registries/schemas.toml` | schemas/spatial_handle.schema.json |
| `SLO-001` | `registries/slos.toml` | Capture accounting |
| `SLO-002` | `registries/slos.toml` | Online feedback |
| `SLO-003` | `registries/slos.toml` | Publication atomicity |
| `SLO-004` | `registries/slos.toml` | Cancellation |
| `SLO-005` | `registries/slos.toml` | Exterior geometry standard profile |
| `SLO-006` | `registries/slos.toml` | High-quality profile |
| `SLO-007` | `registries/slos.toml` | Critical semantic precision |
| `SLO-008` | `registries/slos.toml` | Archive recovery |
| `SLO-009` | `registries/slos.toml` | Deterministic reference replay |
| `SLO-010` | `registries/slos.toml` | Agent economy |
| `SLO-011` | `registries/slos.toml` | Cold-agent orientation |
| `SLO-012` | `registries/slos.toml` | Heartbeat economy |
| `SLO-013` | `registries/slos.toml` | Context value density |
| `SLO-014` | `registries/slos.toml` | Handoff sufficiency |
| `SLO-015` | `registries/slos.toml` | Recommendation calibration |
| `SLO-016` | `registries/slos.toml` | First-try discoverability |
| `SLO-017` | `registries/slos.toml` | Accretive control cost |
| `SLO-018` | `registries/slos.toml` | Decision Frame sufficiency |
| `SLO-019` | `registries/slos.toml` | Attention stability |
| `SLO-020` | `registries/slos.toml` | Pilot cognitive economy |
| `TEST-001` | `registries/tests.toml` | canonical codec vectors, malformed bounds, roundtrip, cross-implementation |
| `TEST-002` | `registries/tests.toml` | identity/domain-separation and collision/substitution handling |
| `TEST-003` | `registries/tests.toml` | anchor snapshot isolation and mixed-generation rejection |
| `TEST-004` | `registries/tests.toml` | clock discontinuity, drift, packet/frame accounting |
| `TEST-005` | `registries/tests.toml` | coordinate conversion and known-camera projection/unprojection |
| `TEST-006` | `registries/tests.toml` | scale witness admission, conflict, demotion, and metric-output refusal |
| `TEST-007` | `registries/tests.toml` | publication kill-point crash matrix and orphan staging repair |
| `TEST-008` | `registries/tests.toml` | cancellation request/drain/reconcile/finalize at every boundary |
| `TEST-009` | `registries/tests.toml` | idempotency and indeterminate external effect reconciliation |
| `TEST-010` | `registries/tests.toml` | source/device compatibility and secret leak scans |
| `TEST-011` | `registries/tests.toml` | media sidecar malformed input/output, descendant cleanup, timeline equivalence |
| `TEST-012` | `registries/tests.toml` | model artifact/license/profile and output validation |
| `TEST-013` | `registries/tests.toml` | reference/optimized/model geometry differential tests |
| `TEST-014` | `registries/tests.toml` | loop closure, refinement, fusion, topology, and uncertainty |
| `TEST-015` | `registries/tests.toml` | semantic presence, counterevidence, critical resolution, absence refusal |
| `TEST-016` | `registries/tests.toml` | coverage certificate and next-best-view deterministic policy |
| `TEST-017` | `registries/tests.toml` | archive multipart, checksum, readback, repair, and restore |
| `TEST-018` | `registries/tests.toml` | capability noninterference and privacy export |
| `TEST-019` | `registries/tests.toml` | graph/search/report rebuild and provenance equivalence |
| `TEST-020` | `registries/tests.toml` | CLI/MCP schemas, pagination, cancellation, tasks, and multi-client isolation |
| `TEST-021` | `registries/tests.toml` | migration, backward/forward compatibility, and old-capture reinterpretation |
| `TEST-022` | `registries/tests.toml` | local release qualification and evidence bundle self-verification |
| `TEST-023` | `registries/tests.toml` | Agent Turn Packet shape, field order, success/error/progress parity, and schema golden vectors |
| `TEST-024` | `registries/tests.toml` | cold arrival obtains sufficient mission, anchor, four-ledger, authority, and next-step orientation in one call |
| `TEST-025` | `registries/tests.toml` | pulse heartbeat stays compact, delta-correct, and preserves critical continuity and active work |
| `TEST-026` | `registries/tests.toml` | gap, reset, stale, partial, and indeterminate continuity vectors fail closed with exact recovery |
| `TEST-027` | `registries/tests.toml` | question terminal predicates, evidence deficits, counterevidence, stopping, and reopening under new evidence |
| `TEST-028` | `registries/tests.toml` | context-pack deterministic selection, mandatory safety items, Pack DNA, budget marginal value, and continuation |
| `TEST-029` | `registries/tests.toml` | affordance enabled/degraded/blocked reasons match state, grants, compatibility, risk, and confirmation |
| `TEST-030` | `registries/tests.toml` | candidate plans share a basis and Pareto ordering is deterministic under adversarial equivalent options |
| `TEST-031` | `registries/tests.toml` | active obligations, confirmations, drains, and indeterminate effects remain visible after context loss |
| `TEST-032` | `registries/tests.toml` | pilot guidance identifies evidence purpose, quality/abort rules, and observed question-resolution outcome |
| `TEST-033` | `registries/tests.toml` | episode and surprise capsules compare prediction, observation, cost, regret, and affected assumptions |
| `TEST-034` | `registries/tests.toml` | memory/policy promotion requires independent evidence, replay, shadow, canary, monitor, and rollback |
| `TEST-035` | `registries/tests.toml` | handoff capsule allows a fresh agent to resume the minimum safe step without transcript replay |
| `TEST-036` | `registries/tests.toml` | multi-agent branch, reservation, lease/fence, duplicate-work, merge, and stale-worker campaigns |
| `TEST-037` | `registries/tests.toml` | capability/help/schema/robot-doc/MCP manifest parity, typo repair, valid-value discovery, and maturity honesty |
| `TEST-038` | `registries/tests.toml` | Decision Frame sufficiency, singularity, stale/rebase, do-nothing candidate, and one-basis candidate-frontier scenarios |
| `TEST-039` | `registries/tests.toml` | attention class ordering, deterministic tie breaks, hysteresis, acknowledgement, suppression, expiry, and material-change re-entry |
| `TEST-040` | `registries/tests.toml` | spatial-handle coordinate declaration, semantic zoom, access control, historical supersession, ambiguity, and round-trip fixtures |
| `TEST-041` | `registries/tests.toml` | human-pilot acknowledge, skip, refusal, observed compliance, quality failure, stop, safety preemption, and abort scenarios |
| `TEST-042` | `registries/tests.toml` | CLI, MCP, NDJSON, and cockpit projections produce semantically equivalent Decision Frames and Agent Turn Packets |
| `TEST-043` | `registries/tests.toml` | all public schemas, CLI JSON, MCP, NDJSON, receipts, examples, migrations, and self-description use one canonical snake_case vocabulary and fdgr.<name>/1 payload identity |
| `TEST-044` | `registries/tests.toml` | anchor-vector canonical encoding, digest, cross-schema reuse, per-generation high-water compatibility, gap/reset/stale transitions, and mixed-generation rejection |
| `WP-000` | `registries/work_packages.toml` | Constitution and registries |
| `WP-001` | `registries/work_packages.toml` | Workspace and qualification scaffold |
| `WP-002` | `registries/work_packages.toml` | Canonical identity and codec |
| `WP-003` | `registries/work_packages.toml` | Reference evidence ledger |
| `WP-004` | `registries/work_packages.toml` | Content-addressed publication |
| `WP-005` | `registries/work_packages.toml` | Original media import |
| `WP-006` | `registries/work_packages.toml` | Media sidecar protocol |
| `WP-007` | `registries/work_packages.toml` | Packet frame and clock ledger |
| `WP-008` | `registries/work_packages.toml` | Calibration registry |
| `WP-009` | `registries/work_packages.toml` | Scale witness system |
| `WP-010` | `registries/work_packages.toml` | DJI compatibility laboratory |
| `WP-011` | `registries/work_packages.toml` | Recorded and mirrored capture adapters |
| `WP-012` | `registries/work_packages.toml` | Owner-authorized live adapter research |
| `WP-013` | `registries/work_packages.toml` | Keyframe and image-quality engine |
| `WP-014` | `registries/work_packages.toml` | Reference feature and track pipeline |
| `WP-015` | `registries/work_packages.toml` | Model-worker protocol |
| `WP-016` | `registries/work_packages.toml` | MapAnything candidate lane |
| `WP-017` | `registries/work_packages.toml` | Streaming geometry research lanes |
| `WP-018` | `registries/work_packages.toml` | Pose graph and bundle refinement |
| `WP-019` | `registries/work_packages.toml` | Depth ensemble and uncertainty |
| `WP-020` | `registries/work_packages.toml` | Geometry fusion |
| `WP-021` | `registries/work_packages.toml` | Coverage and next-best-view |
| `WP-022` | `registries/work_packages.toml` | Semantic ontology and observations |
| `WP-023` | `registries/work_packages.toml` | Semantic resolver and scene graph |
| `WP-024` | `registries/work_packages.toml` | Archive and cloud replication |
| `WP-025` | `registries/work_packages.toml` | Search reports and exports |
| `WP-026` | `registries/work_packages.toml` | CLI and robot protocol |
| `WP-027` | `registries/work_packages.toml` | MCP presentation plane |
| `WP-028` | `registries/work_packages.toml` | Eidetic operational integration |
| `WP-029` | `registries/work_packages.toml` | Deterministic laboratory |
| `WP-030` | `registries/work_packages.toml` | Ground-truth benchmark corpus |
| `WP-031` | `registries/work_packages.toml` | Performance and economic optimizer |
| `WP-032` | `registries/work_packages.toml` | Security privacy and release qualification |
| `WP-033` | `registries/work_packages.toml` | Agent semantic constitution |
| `WP-034` | `registries/work_packages.toml` | Agent Turn Packet and orientation |
| `WP-035` | `registries/work_packages.toml` | Mission objective and question graph |
| `WP-036` | `registries/work_packages.toml` | Context packs and Pack DNA |
| `WP-037` | `registries/work_packages.toml` | Candidate frontier and counterfactual planning |
| `WP-038` | `registries/work_packages.toml` | Semantic watch and reconciliation |
| `WP-039` | `registries/work_packages.toml` | Pilot profile and evidence-gain guidance |
| `WP-040` | `registries/work_packages.toml` | Episode surprise and cost ledger |
| `WP-041` | `registries/work_packages.toml` | Handoff and resume |
| `WP-042` | `registries/work_packages.toml` | Multi-agent semantic coordination |
| `WP-043` | `registries/work_packages.toml` | Self-description and first-try inevitability |
| `WP-044` | `registries/work_packages.toml` | Agent ergonomics and accretion qualification |
| `WP-045` | `registries/work_packages.toml` | Decision Frame and shared cockpit |
| `WP-046` | `registries/work_packages.toml` | Epistemic debt and stable attention |
| `WP-047` | `registries/work_packages.toml` | Spatial handles and human-agent flight |

<!-- END GENERATED REGISTRY TRACEABILITY -->
