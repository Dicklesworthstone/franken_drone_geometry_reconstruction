# Deep Dive 08 — `dwarf_fortress_mcp`

**Import decision:** the semantic-control-plane, witnessed-intent, obligation, compatibility, and non-bypassability doctrines map almost exactly
**FDGR authority:** agent interface and every DJI/device effect boundary

## Why the domains have the same control shape

Dwarf Fortress and a DJI reconstruction session look unrelated until their failure topology is
examined. Both are externally changing, version-sensitive systems where:

- observation is partial and delayed;
- raw UI/transport acknowledgments are not semantic completion;
- actions can begin now and complete later;
- retries can duplicate irreversible effects;
- compatibility changes across versions;
- agents need compact semantic state rather than raw dumps;
- long operations need durable identity and later proof;
- external state cannot participate in the local transaction;
- cancellation must reconcile what may already have happened.

FDGR therefore imports the architecture of a semantic operating substrate over a fenced external
adapter, not a catalog of low-level commands.

## Mechanism 1 — canonical semantic state above device layout

DJI packet fields, application endpoints, firmware structures, controller UI labels, and media
container metadata are adapter facts. FDGR exposes stable semantic concepts:

- device and camera identity;
- capture session and stream;
- frame/access unit and telemetry sample;
- clock relation;
- camera state and calibration;
- flight/camera operation;
- observation capsule;
- reconstruction generation;
- coverage target;
- archive obligation;
- semantic claim and evidence.

A firmware/profile adapter maps raw structures into these concepts and records provenance. It
cannot redefine identity, units, success, or claim resolution.

Unknown or partially mapped fields remain explicit opaque evidence. The semantic layer never
hallucinates a mapping to make an adapter look complete.

## Mechanism 2 — owner-authorized profile laboratory

FDGR uses a compatibility profile keyed by:

```text
aircraft model and serial/pseudonymous device identity
aircraft firmware
controller model and firmware
DJI Fly / host application version
phone/host OS and architecture
region and radio mode
pairing/authentication method
stream/media mode
observed protocol grammar epoch
```

Research is limited to equipment, networks, accounts, and credentials the operator controls. It
begins read-only and passive:

1. exact media import;
2. passive network/USB/host observation;
3. stream discovery and packet capture;
4. authenticated session replay using legitimate pairing/account state;
5. telemetry decoding;
6. camera/gimbal command experiments;
7. assisted navigation only after separate safety/effect gates.

The design does not bypass authentication, pairing, encryption, account controls, geofencing, or
another operator's system. Secrets are capability-held and redacted from evidence bundles.
Protocol research artifacts preserve packet bytes, timing, one-variable-at-a-time experiment
metadata, and hypotheses. A generated emulator/replayer supports deterministic adapter tests.

## Mechanism 3 — observation capsules and exact anchors

A device observation is normalized into an immutable capsule containing:

- profile and bridge generation;
- capture and clock epochs;
- raw object/packet identities;
- sequence/time ranges and gaps;
- normalized semantic records;
- completeness and uncertainty;
- parser/schema identities;
- predecessor and root digest.

Publication is reserve → normalize/validate → materialize children → verify semantic root → publish
root/high-water mark → notify derived consumers. Readers see old or new, never a partial mixture.
A delta applies only to its exact basis and reconstructs the successor root.

Reconnect, restore, ambiguous bridge restart, or profile incompatibility creates an epoch change.
Old plans and commands are fenced.

## Mechanism 4 — the narrow agent waist

FDGR does not expose one MCP tool per packet, ffmpeg option, graph algorithm, or DJI command. The
target logical tool set is small:

| Tool | Semantic purpose |
|---|---|
| `fdgr.open_session` | negotiate property, device, capability, freshness, and budgets |
| `fdgr.discover` | devices, profiles, captures, and supported/degraded surfaces |
| `fdgr.observe` | compact anchored situation/health/coverage projection |
| `fdgr.query` | typed temporal/spatial/graph/evidence query |
| `fdgr.plan_capture` | compile capture or reconstruction intent into a sealed plan |
| `fdgr.commit` | revalidate and authorize a prepared plan by digest |
| `fdgr.wait` | observe obligation progress and terminal proof |
| `fdgr.cancel` | request drain/reconciliation/finalization |
| `fdgr.explain` | reconstruct evidence, decision, result, or failure provenance |
| `fdgr.export` | prepare/publish a versioned export sibling set |
| `fdgr.checkpoint` | seal a restorable project root |
| `fdgr.restore` | activate a checkpoint as a new fenced epoch |
| `fdgr.doctor` | read-only compatibility, custody, geometry, semantic, and release diagnosis |

Streaming/resources/prompts can expose large read-only artifacts without expanding mutation
authority. The MCP plane is replaceable; the semantic contracts live below it.

## Mechanism 5 — semantic intent compilation

A request such as:

> Capture the north and west exterior so the system can resolve all doors, windows, utility
> penetrations, and walking paths to confidence policy `home-utilities-v1`.

is not translated directly into waypoints. Planning:

1. pins an evidence/scene anchor;
2. resolves property/device/capability scope;
3. expands the target ontology and coverage requirements;
4. computes current deficits and detectability;
5. generates candidate viewpoints and routes;
6. applies airspace, obstacle, battery, link, privacy, and human-control constraints;
7. predicts cost and information gain;
8. records read/write/negative witnesses;
9. produces a deterministic plan DAG and digest;
10. classifies every step by risk, reversibility, and confirmation policy.

The plan is immutable. Editing intent means compiling a new plan. Commit revalidates against a
fresh anchor and current capability/lease epochs.

## Mechanism 6 — witnessed mutation and final device preconditions

Before dispatch, the coordinator validates:

- plan digest and idempotency identity;
- exact device/profile/session epoch;
- capability scope and owner authorization;
- battery/link/home-point/geofence and registered safety state;
- current camera/gimbal/flight mode;
- route/coverage basis and changed obstacles;
- leases and fencing token;
- required checkpoint/custody state;
- rate, retry, and effect budgets.

The adapter performs a final bounded precondition check as close to the device command as the
profile permits. Passing local MVCC validation proves the plan was valid against FDGR state; it
does not freeze the physical world. The external check and subsequent observation remain required.

Manual-control plans can stop at guidance: recommended heading, altitude, distance, camera angle,
speed, and target coverage. Assisted/autonomous control is a separate capability family and later
qualification tier, not an accidental consequence of having decoded commands.

## Mechanism 7 — effect stages and honest completion

A device or cloud effect progresses through distinct states:

```text
Prepared
→ CommittedLocally
→ DispatchAttemptPersisted
→ Dispatched
→ TransportAccepted?
→ Device/ProviderAccepted?
→ EffectObserved?
→ TerminalPredicateProved
→ ObligationDischarged
```

Failures between stages can yield `Indeterminate`. Examples:

- the network drops after a gimbal command was sent;
- DJI accepts a recording command but the stream confirmation is delayed;
- multipart completion returns no response;
- a camera setting changes but telemetry is stale.

Transport acknowledgment is never semantic success. Completion is proved by registered predicates
over authoritative observation, such as recording state plus new frame sequence, camera parameter
observation, reached waypoint with tolerances, or cloud root readback.

## Mechanism 8 — idempotency, lookup, and retry

Every effect has an idempotency key bound to:

- operation family and canonical parameters;
- plan and step digest;
- device/profile/epoch;
- capability/lease incarnation;
- caller/session identity;
- allowed retry semantics.

Repeating the same identity returns or reconciles the existing operation. Reusing an identity with
different content fails. Blindly retrying an indeterminate non-idempotent command is forbidden.
The adapter must provide operation lookup or FDGR must reconcile through observed state before a
new plan can proceed.

## Mechanism 9 — leases and fencing for multi-agent/device safety

A resource lease covers domains such as:

- active aircraft command authority;
- camera/gimbal control;
- capture-session ownership;
- calibration publication;
- trunk reconstruction publication;
- export destination;
- archive retention mutation.

Leases carry incarnation and monotonic fence. A restarted or delayed worker cannot publish or
command after a successor acquires the resource. Read branches and advisory analyses need no broad
write lease.

Multiple agents coordinate through branches, intents, and leases rather than private assumptions.
Human/manual control can preempt automation according to explicit policy; preemption triggers
request/drain/reconcile, not detached abandonment.

## Mechanism 10 — obligations for long-running work

A committed plan owns obligations such as:

- maintain device link or enter a safe degraded state;
- durably spool accepted evidence;
- reach specified coverage predicates;
- reconcile each command;
- complete local seal before retention transitions;
- publish derived generations;
- upload/read back archive roots;
- produce final evidence and report bundles.

Obligations expose progress, blockers, potential functions, deadlines, budgets, and terminal
predicates. `wait` is not polling an opaque job status; it returns anchored evidence about what has
completed and why the remainder is blocked.

## Mechanism 11 — compact token-budgeted observation

Agents should not receive thousands of frame rows by default. `fdgr.observe` returns a tiered
projection:

- active devices/captures and critical health;
- last complete evidence and scene anchors;
- geometry maturity and disconnected components;
- scale authority;
- highest-impact coverage deficits;
- semantic contradictions/high-value unresolved assets;
- archive custody grade;
- active plans/obligations and blockers;
- degraded capabilities and next recommended queries.

Continuation seals retrieve details without changing the pinned anchor. Output budgets are part of
the request context. Compactness never removes provenance or completeness status.

## Mechanism 12 — compatibility certification and degraded modes

An adapter/profile can independently qualify:

- exact media import;
- passive live-stream ingest;
- packet integrity and frame sequence;
- telemetry classes;
- camera settings observation;
- recording control;
- gimbal control;
- position/attitude observation;
- assisted flight effect families;
- operation lookup and reconciliation;
- reconnect and profile transition behavior.

Unknown firmware fails closed to the highest previously proven read-only surface or exact media
import. “Mostly the same version” is not a compatibility argument. Qualification receipts name
physical devices, sanitized profile identities, experiment corpus, source commit, and date.

## Mechanism 13 — doctor and sealed repair plans

`fdgr.doctor` is read-only and cross-plane. It can diagnose:

- device/profile mismatch;
- missing original sequence ranges;
- clock/calibration inconsistency;
- disconnected pose graph;
- scale conflict;
- geometry/semantic generation lag;
- unresolved device effects;
- archive custody weakness;
- dependency/source-closure drift;
- failed local qualification lanes.

Where a mutation can repair local state, doctor proposes a sealed plan bound to current roots.
Apply revalidates. Device-side repairs compile through normal intent/effect paths; doctor never
becomes an arbitrary command shell.

## Mechanism 14 — untrusted content and no arbitrary execution

Forbidden production surfaces include:

- arbitrary shell command tools;
- arbitrary DJI SDK method invocation;
- arbitrary packet injection;
- arbitrary Lua/Python evaluation;
- user-supplied ffmpeg command strings;
- model-generated command execution;
- raw credential export;
- text instructions that become capabilities.

Diagnostic laboratories can expose bounded fixture replay to developers under explicit local
capabilities. The public agent waist remains typed and narrow.

## Mechanism 15 — advisory Eidetic memory outside canonical state

Agents may use Eidetic Engine for prior flight lessons, property conventions, protocol research,
model failures, and review notes. Memories cite FDGR anchors/evidence. FDGR evidence never cites
memory as authority. When memory and current observation disagree, current evidence wins and the
contradiction can feed memory outcome/decay.

## Non-bypassability rules

An FDGR implementation is invalid if any path can:

- dispatch a device effect without plan, capability, idempotency, and fence checks;
- call a transport ACK completed work;
- retry an indeterminate effect blindly;
- publish a scene root before child closure;
- mix generations in a query;
- apply a delta across a basis gap;
- treat negative cache lookup as physical absence;
- let cognition/model output directly command a device;
- restore without a new epoch;
- expose arbitrary packet/shell/model execution;
- close a session while owned work or external effects are silently abandoned;
- let adaptive policy weaken a hard condition.

These are checked through dependency architecture, trait visibility, capability types, source
audits, and behavioral tests—not prose alone.

## Superficial imports rejected

- a huge MCP catalog mirroring every internal method;
- screenshot/video-only observation with no canonical identity;
- a mutable global “current reconstruction” cache;
- planning and dispatch in one tool call for consequential effects;
- accepted command = completed effect;
- optimistic blind retries;
- firmware version string treated as compatibility proof;
- multi-agent coordination through conventions instead of fencing;
- job status without terminal predicates/evidence;
- raw model captions as semantic truth;
- arbitrary ffmpeg/DJI command strings.

## FDGR admission gate

1. Canonical semantics are independent of raw adapter structures.
2. Observation capsule publication is atomic and epoch-safe.
3. The narrow tool waist covers complete workflows without command escape hatches.
4. Plans carry positive, negative, and write witnesses and revalidate at commit.
5. Every effect family has idempotency, lookup/reconciliation, and terminal predicates.
6. Indeterminate outcomes survive crash/restart and cannot be blindly retried.
7. Lease fencing blocks stale workers/agents.
8. Compatibility is feature/profile certified with honest degradation.
9. Agent projections are compact, anchored, and continuation-safe.
10. Non-bypassability is enforced structurally and by adversarial tests.

---

## Agent-native synthesis

### Transfer of the agent operating model

FDGR adopts and specializes the Agent Turn Packet, epistemic lattice, attention/affordance/recommendation separation, active-work continuity, surprise, and handoff. It extends the pattern with anchor vectors, question-first physical evidence, Pack DNA, pilot guidance, and total-control-cost accounting.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
