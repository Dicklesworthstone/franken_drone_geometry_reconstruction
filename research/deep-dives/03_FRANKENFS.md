# Deep Dive 03 — `frankenfs`

**Import decision:** custody, coherent publication, repair, and filesystem effect doctrine are foundational
**FDGR authority:** original evidence spool, immutable object store, checkpoint/export custody, and restore

## Why filesystem semantics are part of scientific truth

FDGR will manipulate very large and heterogeneous artifacts: packet captures, original camera
files, decoded frame tiles, calibration packages, pose graphs, depth pyramids, spatial bricks,
meshes, semantic indexes, model weights, reports, cloud staging state, and replay corpora. A
correct geometry algorithm is useless if a root is published before its children, if a crash
leaves an export that looks complete, if path traversal crosses a property boundary, or if the
last local original is deleted after an unverified upload.

The FrankenFS import is a custody protocol, not a convenience wrapper around `std::fs`.

## Mechanism 1 — staged, visible, and durable are different states

FDGR uses the monotone state relation:

```text
staged_generation ≥ visible_generation ≥ durable_generation
```

- **Staged:** bytes exist in private temporary custody and may still be incomplete.
- **Visible:** a coherent root is discoverable by readers in the current process/profile.
- **Durable:** the registered storage policy has reached its declared persistence boundary.

An object can be staged but not visible. A root cannot become visible while naming missing or
unverified children. A visible generation can be newer than the known durable generation only
when the API and receipt explicitly expose that distinction. `flush` is never silently promoted
to `fsync` semantics.

This state applies to local packet runs, evidence capsules, spatial generations, model caches,
exports, and remote manifests.

## Mechanism 2 — coherent root-last publication

Every multi-artifact publication follows:

1. freeze the intended sibling set and identities;
2. preflight destination capability, quotas, path policy, and existing roots;
3. create children under an unpublished staging generation;
4. stream and validate each child;
5. sync each child according to the durability class;
6. compute the manifest, closure digest, and optional repair metadata;
7. independently reopen/read back critical children when policy requires;
8. publish the root pointer atomically;
9. sync the root directory or equivalent provider boundary;
10. emit a receipt naming staged, visible, and durable epochs;
11. retire or retain the previous root according to policy.

A directory containing 99% of the files is not a digital twin. A cloud listing containing a
manifest object is not proof that every referenced part is retrievable. A generated human report,
machine manifest, checksums, citation map, and repair symbols are one sibling transaction when
they describe one release.

## Mechanism 3 — content-addressed custody with a tiny mutable root

The preferred store contains immutable objects addressed by domain-separated digest. Mutable state
is minimized to small root/lease records. Logical names are manifests, not authoritative file
paths. This gives FDGR:

- deduplication across branches and successor generations;
- safe resume and multi-provider replication;
- immutable evidence references;
- cheap snapshots and historical retention;
- repair and corruption detection by identity;
- deterministic packaging;
- the ability to rebuild all derived forms from original roots.

Digest identity includes format/version domain and canonical encoding rules. Hash equality across
unrelated object classes is never assumed meaningful.

## Mechanism 4 — rooted path capabilities

Filesystem access is granted through a capability describing:

- exact allowed roots;
- read/write/create/delete/rename operations;
- logical path classes;
- symlink and mount-crossing policy;
- maximum path, file, directory-entry, and total-byte budgets;
- allowed object types;
- staging and durability policy;
- ownership/lease incarnation;
- expected destination generation.

Untrusted names from DJI media, ZIP/MP4 metadata, model packages, MCP arguments, and reports never
become raw host paths. They are normalized into bounded logical names and mapped through a rooted
capability. Unknown Unicode normalization, device names, alternate streams, symlink races, hard
links, and cross-device rename semantics are explicit compatibility surfaces.

Until a platform can establish the required race-free confinement, high-risk mutations are
disabled or delegated to a dedicated least-authority helper with its own evidence receipt. The
safe-Rust core never overclaims what a path-string check proves.

## Mechanism 5 — capture spool as a crash-consistency state machine

Original live evidence has the highest write priority. A packet/access-unit spool uses bounded
segments:

```text
Reserved
→ Receiving
→ SealedPayload
→ Indexed
→ RootReferenced
→ Durable
→ Replicated
```

Each segment records sequence and clock ranges, byte count, packet integrity, truncation state,
codec/container profile, and predecessor identity. On restart:

- complete sealed segments are revalidated and adopted;
- partial segments are retained as explicit truncated evidence, not silently discarded;
- an index is rebuilt from sealed payload if its publication was interrupted;
- duplicate ingress is recognized by identity/idempotency;
- sequence gaps become durable findings;
- no derived preview can outrank an incomplete original segment.

The spool reserves local capacity before accepting a capture contract. Under pressure, it sheds
recomputable previews before accepted original bytes.

## Mechanism 6 — copy-on-write checkpoints and branches

A project checkpoint is a root over immutable objects, generation manifests, policies, and
receipts. Branching is O(root-manifest) rather than copying terabytes. This supports:

- solver/model experiments;
- alternative loop-closure or scale hypotheses;
- redacted/public exports;
- agent branches;
- pre-migration snapshots;
- disaster-recovery test restores.

Restoring a root creates a new active epoch. It does not pretend time moved backward. Stale workers
and leases from the previous epoch cannot publish into the restored lineage.

## Mechanism 7 — repair is plan, seal, revalidate, apply

`doctor` is read-only. It produces findings such as:

- missing child object;
- digest mismatch;
- truncated spool segment;
- stale or inconsistent root pointer;
- provider receipt without successful readback;
- insufficient repair symbols;
- retention policy at risk;
- orphan staged objects;
- branch pin preventing reclamation;
- model cache identity mismatch;
- incompatible schema or codec generation.

Repair planning turns findings into an immutable plan sealed to the current root and custody
state. `repair.apply` reopens and revalidates the seal immediately before mutation. If the basis
changed, it refuses and replans. Repairs never silently rewrite authoritative evidence; they
recover an object with the same identity or publish a successor finding/manifest.

## Mechanism 8 — RaptorQ repair and adaptive refresh

Large immutable object families may carry fountain-coded repair symbols. The design benefits are:

- a partial local disk, B2 object, R2 object, peer, or removable backup can contribute symbols;
- repair does not require the original uploader;
- damaged/corrupt chunks can be reconstructed offline;
- ATP can seed from multiple donors;
- storage overhead can be allocated by evidence value and measured risk.

Repair overhead policy is explicit. A Bayesian or sequential risk model may recommend symbol
counts and refresh timing from observed corruption and retrievability, but hard minimums protect
irreplaceable original evidence. Statistical confidence is not a substitute for actual restore
campaigns.

RaptorQ metadata and implementation are versioned. “Has repair symbols” is not equivalent to “has
passed end-to-end reconstruction and byte-identity proof.”

## Mechanism 9 — scrub, proof of retrievability, and custody grades

FDGR separates custody grades:

1. `LocalStaged`
2. `LocalDurable`
3. `ReplicaReceiptOnly`
4. `ReplicaReadbackVerified`
5. `MultiFailureDomainVerified`
6. `RepairQualified`
7. `RestoreCampaignQualified`

Periodic scrub reads object samples or complete critical roots, verifies identity, updates a
bounded evidence ledger, and schedules repair/re-replication. Proof-of-retrievability sampling is
useful for large cold archives but cannot replace periodic full restore of representative roots.

Deletion of the final local original requires a retention plan proving an adequate successor
custody grade, exact root closure, and no unresolved legal/operational hold.

## Mechanism 10 — temperature-aware storage and cache policy

Artifacts have different temperatures and regeneration costs:

| Tier | Examples | Policy |
|---|---|---|
| T0 volatile compute | feature scratch, solver workspace | bounded, disposable |
| T1 hot durable | active packet spool, recent keyframes, pose deltas | local low-latency |
| T2 warm derived | depth tiles, spatial deltas, semantic candidates | local/cache + replica |
| T3 sealed derived | converged geometry, indexes, exports | content-addressed, compact |
| T4 irreplaceable cold | original media, calibration evidence, receipts | multi-failure-domain + repair |
| T5 reproducible cold | models, benchmark corpora, generated reports | fetch/rebuild by manifest |

Cache keys include full generation and policy identity. Unpublished children never enter a shared
visible cache. Request coalescing joins only requests with identical authority, freshness,
calibration/model policy, and output contract.

S3-FIFO, ARC-like, segmented LRU, or learned policies are candidates, not doctrine. Same-binary
workloads must show a benefit under hit rate, bytes read, tail latency, write amplification, and
rebuild cost while preserving deterministic outputs.

## Mechanism 11 — a virtual twin filesystem as a derived view

A future `fdgr mount` may expose a human-friendly virtual hierarchy:

```text
/property/
  captures/
  timelines/
  cameras/
  geometry/current/
  geometry/history/
  rooms/
  utilities/
  coverage/
  evidence/
  reports/
```

This is a projection over immutable manifests, not the source of truth. Renaming a mounted file
does not mutate canonical identity. Writes, if ever supported, compile to typed intents with
preconditions and publication semantics. Read-only operation is the default and first qualified
mode.

## Mechanism 12 — same-binary experiments and explicit evidence dimensions

Filesystem and archive optimizations are compared inside one binary with runtime-selected arms.
Both arms receive the same input root and must produce the same semantic output digest and receipt
schema before timing is considered. Required controls include A/A null runs, warm/cold cache
labels, host/filesystem identity, sample distributions, and replay commands.

Readiness dimensions remain separate:

- contract implemented;
- deterministic reference verified;
- filesystem fault verified;
- path-security verified;
- live capture verified;
- provider integration verified;
- repair verified;
- restore verified;
- performance verified.

A malformed-path rejection test is valuable negative evidence, not proof that successful safe
publication works.

## Crash matrix

At minimum, kill/fault injection covers before and after:

- segment reservation;
- first and last payload write;
- payload sync;
- segment seal;
- index write/sync;
- manifest write/sync;
- root rename/promotion;
- root-directory sync;
- provider multipart completion request;
- provider receipt persistence;
- provider readback;
- repair-symbol publication;
- local-retention deletion.

Every restart result maps to a stable state: recover/adopt, resume, quarantine, repair, roll back
visibility, or explicit indeterminate provider effect.

## Superficial imports rejected

- “write temp then rename” without child closure and directory durability;
- one `uploaded=true` flag;
- treating an object-store ETag as a universal content digest;
- publishing a manifest before multipart readback;
- path-prefix string checks described as race-free confinement;
- deleting local originals after a successful HTTP response;
- in-place mutation of content-addressed objects;
- repair that edits evidence without a sealed plan;
- cache entries keyed only by pathname;
- readiness collapsed into one percentage.

## FDGR admission gate

1. The reference custody state machine is executable and deterministic.
2. Every root reconstructs independently from its manifest.
3. Root-last publication survives the complete kill matrix.
4. Path attacks cover traversal, symlink/hard-link races, Unicode, devices, and mount boundaries.
5. Restore always creates a fenced successor epoch.
6. Provider uploads require readback/closure receipts before custody promotion.
7. RaptorQ campaigns reconstruct exact bytes under corruption, truncation, and donor loss.
8. Scrub and repair emit complete evidence and never silently bless an object.
9. Local retention deletion is capability- and plan-gated.
10. Performance claims include same-binary semantic receipts and local host manifests.

---

## Agent-native synthesis

### Agent-visible custody semantics

The system ledger exposes staged, visible, durable, replicated, retrievable, and restorable as distinct states. Context packs cite roots instead of paths. Handoff and episode capsules publish root last. Doctor and repair surfaces return sealed plans and exact safe next steps rather than free-form “fix” advice.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
