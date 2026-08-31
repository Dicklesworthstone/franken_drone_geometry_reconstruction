# Deep Dive 01 — `asupersync`

**Import decision:** foundational and exclusive runtime substrate
**FDGR authority:** structural; no second asynchronous runtime is admitted

## Why the runtime is part of reconstruction correctness

A drone reconstruction system is an unusually hostile concurrency problem. A capture session owns
an authenticated device link, a lossy live stream, an exact original-media spool, multiple clock
domains, decoding, keyframe selection, online pose estimation, archival upload, model workers,
operator commands, and long-running offline refinement. Cancellation can occur while any one of
those has partially affected the outside world. A conventional collection of spawned tasks and
channels can easily report “stopped” while a writer is still flushing, an upload is still
publishing, a camera command has an unknown outcome, or a model worker owns the only copy of an
unpublished result.

The most important lesson from `asupersync` is therefore not API preference. It is that ownership,
authority, cancellation, time, and effect commitment are one programming model.

## Mechanism 1 — region ownership and quiescence

### Source mechanism

Work belongs to a region tree. Region close is not handle-dropping; it is a transition toward
quiescence in which all children terminate and registered obligations are resolved.

### FDGR import

The production process has this supervised topology:

```text
fdgr-process
├── authority-root
├── device-supervisor
│   ├── discovery-and-compatibility
│   ├── authenticated-session
│   │   ├── packet-reader
│   │   ├── telemetry-reader
│   │   ├── command-lane
│   │   └── liveness-and-reconciliation
│   └── profile-laboratory
├── evidence-supervisor
│   ├── original-byte-spool
│   ├── capsule-publication-coordinator
│   ├── clock-and-calibration-ledger
│   └── evidence-sealer
├── live-reconstruction-supervisor
│   ├── native-demux-and-decode
│   ├── frame-quality-and-keyframes
│   ├── tracking-and-local-pose
│   ├── preview-depth-and-fusion
│   └── coverage-guidance
├── convergence-supervisor
│   ├── loop-closure
│   ├── global-pose-and-bundle-optimization
│   ├── multiview-depth
│   ├── surface-and-topology-fusion
│   └── semantic-resolution
├── archive-supervisor
│   ├── local-custody
│   ├── ATP-transfer
│   ├── provider-replication
│   └── scrub-repair-and-retrievability
├── query-and-agent-supervisor
│   ├── MCP-sessions
│   ├── snapshot-queries
│   ├── planning-branches
│   └── obligation-watchers
└── laboratory-supervisor
    ├── virtual-device
    ├── fault-schedule
    ├── deterministic-replay
    └── evidence-comparator
```

A capture session cannot become terminal while an original packet is only in volatile memory, a
child can still publish under the old session, an archive root is half promoted, or a device effect
is unresolved. Region closure emits a receipt naming every child, last published anchor, remaining
external effect, cancellation reason, and any `Indeterminate` outcome.

### Superficial imitation rejected

Storing `JoinHandle`s in a vector is not structured ownership. It does not prevent a helper from
spawning outside the vector, establish a close protocol, or prove that effects were reconciled.

### Admission evidence

- task-tree inventory is mechanically complete;
- every production scenario runs under both real and laboratory time;
- forced cancellation at every registered yield point leaves no child alive;
- region close cannot publish success with an unresolved external effect;
- leak and quiescence oracles cover normal, cancelled, failed, and panicked outcomes.

## Mechanism 2 — `Cx` as authority, budget, time, and replay identity

### FDGR import

Every function that can block, allocate shared memory, read time, perform I/O, dispatch device
control, publish a generation, start a child, invoke a model, or move an object graph accepts an
explicit context. The FDGR context narrows the asupersync substrate with:

- property, capture-session, device, and reconstruction-lineage identity;
- evidence anchor and allowed staleness;
- device compatibility, calibration, schema, model, and policy epochs;
- wall-clock and device-clock deadlines;
- CPU-poll, memory, I/O-byte, packet, frame, model-token, GPU-worker, network, archive, retry, and
  output-token budgets;
- capability grants scoped by device, effect family, property region, semantic class, and cloud
  destination;
- cancellation reason chain;
- pressure observations and admission lane;
- deterministic seed, trace identity, and replay schedule.

Read-only geometry queries receive no command capability. A semantic model worker receives image
and prompt bytes but no device, filesystem-root, credential, or publication capability. A cloud
uploader receives sealed object identities but no authority to create semantic claims.

### Superficial imitation rejected

A global runtime handle plus thread-local request metadata restores ambient authority. A helper can
then read the real clock, allocate without budget, or reach a write-capable service from a
read-only path.

### Admission evidence

Static dependency checks and dynamic capability-denial tests prove that narrower children cannot
recover broader handles. Laboratory replay must fail if any production code consults hidden time,
randomness, environment state, or a global service singleton.

## Mechanism 3 — cancellation as request, drain, reconcile, finalize

### FDGR state model

```text
Running
  → CancelRequested
  → AdmissionClosed
  → DrainingInternalWork
  → ReconcilingExternalEffects
  → SealingEvidence
  → Finalizing
  → Terminal(Completed | Failed | Cancelled | Indeterminate | Panicked)
```

Cancellation never means “drop the future.” It first prevents new effects, then lets owned work
reach declared boundaries, persists enough evidence to resume, looks up or observes external
operations, and publishes a terminal receipt.

### Potential functions and progress certificates

Every nontrivial drain declares a nonnegative potential:

| Subsystem | Example potential |
|---|---|
| packet spool | accepted packet bytes not yet durably sealed |
| decoder | complete access units accepted but not terminally classified |
| pose pipeline | keyframes with unresolved track/pose obligations |
| fusion | dirty spatial blocks not published or rolled back |
| archive | sealed objects without verified destination receipts |
| DJI command lane | dispatched operations without lookup/observation resolution |
| model worker | admitted jobs without persisted result/refusal |
| region close | nonterminal children plus unresolved obligations |

A progress certificate records samples, monotonicity exceptions, active masks, bounded critical
sections, expected remaining work, and the reason a safe bound can or cannot be asserted. A stalled
certificate is a diagnosis signal, never permission to discard work.

### Superficial imitation rejected

A fixed timeout followed by worker kill can lose the only original packet copy, leave a cloud
multipart upload ambiguously committed, or duplicate a camera command on retry.

## Mechanism 4 — two-phase effects

FDGR uses reserve → materialize → validate → commit for:

- accepting packet bytes into the authoritative spool;
- publishing observation capsules;
- promoting clock and calibration generations;
- activating track, pose, depth, geometry, semantic, coverage, and search generations;
- reserving idempotency identities for device commands;
- sealing export sibling sets;
- promoting cloud manifests;
- recording model artifacts;
- publishing qualification receipts.

The reservation fixes basis identities, owner, destination, bounds, and digest recipe. Children are
created under an unpublished root. Commit is a short, masked publication step after closure and
validation. Before commit, cancellation aborts or quarantines the reservation. After commit,
rollback means publishing a successor or executing a registered compensation; committed history
is never silently rewritten.

Two-phase channels are used where cancellation between queue admission and payload ownership would
otherwise lose or duplicate work.

## Mechanism 5 — four-valued outcomes and indeterminacy

FDGR preserves at least:

```text
Ok(value)
Err(domain_error)
Cancelled(reason)
Panicked(payload_digest)
```

At external effect boundaries, domain values additionally distinguish `Indeterminate`. A network
error after DJI command dispatch is not ordinary failure. An interrupted object-store response is
not proof that no object exists. Reconciliation and operation lookup own these states.

Flattening cancellation, panic, expected rejection, and unknown external outcome into one error
string is forbidden because each requires a different retry and evidence policy.

## Mechanism 6 — pressure, admission, and deterministic QoS

FDGR defines stable lanes:

1. **P0 Evidence custody:** original bytes, clock anchors, command receipts. Never shed after
   acceptance.
2. **P1 Device safety and liveness:** control reconciliation, link health, return-to-safe-state
   obligations.
3. **P2 Live pose and coverage:** bounded-latency draft geometry and operator guidance.
4. **P3 Local sealing and archive replication:** resumable and backpressured.
5. **P4 Semantic refinement:** degradable in resolution/model count, never allowed to starve P0–P2.
6. **P5 Global convergence and optional analytics:** opportunistic, checkpointable, cancellable.

Pressure may reduce preview frame rate, semantic candidate count, or refinement depth. It may not
silently drop accepted original evidence, weaken a safety precondition, publish mixed generations,
or call an uncertified result complete.

Adaptive scheduling is permitted only at deterministic policy epochs. Every choice emits a
decision card with candidate arms, observations, clamps, chosen arm, and replay identity. The
stable baseline remains valid when the adaptive policy is cold, reset, or uncertain.

## Mechanism 7 — deterministic laboratory, schedule exploration, and fault injection

The same capture, publication, reconstruction, transfer, and query logic must run against:

- virtual time and deterministic timers;
- a virtual DJI/profile adapter;
- packet duplication, loss, truncation, reordering, and delay;
- clock jumps, drift changes, and discontinuity epochs;
- device reconnects and firmware/profile changes;
- model worker delay, malformed output, crash, and cancellation blindness;
- filesystem short writes, fsync failures, corruption, and process death;
- object-store partial multipart state, stale listings, and read-after-write anomalies;
- memory and CPU pressure;
- cancellation at every yield and publication boundary.

DPOR or equivalent schedule reduction focuses exploration on causally distinct interleavings.
Failure artifacts retain seed, schedule, virtual-time trace, inputs, source identity, and exact
replay command. A production-only timer, thread, or background loop is a verification escape hatch
and is forbidden.

## Mechanism 8 — ATP as the immutable object-graph movement plane

ATP is not “use a faster uploader.” It is the transport semantics for immutable graphs:

```text
root identity
  → manifest
    → child object identities
      → chunks and optional repair symbols
```

The receiver stages objects, verifies each identity, verifies graph closure, persists resume
state, and publishes the root last. Paths may race across local network, R2, B2, removable media,
or peer nodes. Losing paths are drained. Multi-donor seeding is allowed because identity, not
sender trust, determines acceptance. RaptorQ repair symbols may make partial donors useful and
support self-healing custody.

FDGR uses ATP for:

- original-media and packet-capture roots;
- evidence capsule runs;
- calibration packages;
- track/pose/depth/spatial generations;
- model weights and admitted model artifacts;
- reconstruction branches;
- exports, crashpacks, benchmark corpora, and qualification bundles;
- remote read replicas and disaster recovery.

Mutation authority, credentials, and DJI commands never ride ATP. Effects use a fenced,
idempotent command protocol with lookup and observation semantics.

## Mechanism 9 — anytime-valid monitoring without correctness laundering

Sequential evidence processes are useful for detecting:

- link-quality regime changes;
- packet-loss bursts;
- clock-residual drift;
- track survival degradation;
- reprojection-error shifts;
- loop-closure outlier regimes;
- depth-consistency collapse;
- archive corruption/retrievability changes;
- performance regressions during long local campaigns.

They may adjust sampling, alerting, or compute allocation. They may never certify geometry,
metric scale, semantic identity, or safety. If a statistical monitor is unavailable, the hard
reference policy remains sufficient.

## Dependency and boundary decision

`asupersync` is pinned to an exact clean revision and copied into the local release source closure.
No Tokio, async-std, smol, Rayon, detached standard threads, or foreign callback runtime enters
the production process. Blocking foreign or bootstrap oracles run as supervised child processes
with bounded pipes, explicit cleanup, and untrusted outputs.

## Final FDGR admission gate

The asupersync import is operationally qualified only when:

1. all effectful public APIs are context-first;
2. no owned task survives region close;
3. all cancellation paths produce terminal or explicit indeterminate receipts;
4. reserve/materialize/publish survives kill-point campaigns;
5. real and laboratory runs agree semantically;
6. ATP corruption, resume, path-race, donor-loss, and repair campaigns preserve root identity;
7. pressure tests prove P0/P1 invariants under overload;
8. deterministic decision cards replay exactly;
9. dependency policy proves there is one runtime and one cancellation model.

---

## Agent-native synthesis

### Agent control-loop ownership

Asupersync regions own sessions, context-pack builders, candidate branches, committed obligations, watchers, reconciliation, episode sealing, and handoff publication. `Cx` carries both effect authority and total-control-cost budgets. The agent can therefore see exactly which work exists, cancel it without orphaning descendants, and receive progress certificates instead of inferred status.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
