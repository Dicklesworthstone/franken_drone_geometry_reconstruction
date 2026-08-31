# Deep Dive 10 — `eidetic_engine_cli`

**Import decision:** adopt as the external advisory memory and context-packing layer; never link into canonical FDGR state
**FDGR authority:** none; memory is explicitly speculative/advisory

## Why reconstruction agents need durable memory

An agent working on a home over months will otherwise rediscover:

- which DJI firmware/profile experiments worked;
- which flight path created motion blur or poor overlap;
- where GNSS/link degradation occurs around the property;
- which calibration rig and measurements were used;
- which model/checkpoint failed on reflective windows or foliage;
- which geometry branches were rejected and why;
- owner terminology and privacy/export conventions;
- recurring archive/release failures;
- unresolved hypotheses worth revisiting after new evidence.

This information is valuable, but it is not current physical truth. Eidetic Engine fits because it
stores explainable, provenance-carrying memory while preserving a strict boundary from canonical
state.

## The constitutional rule

> Eidetic memory may cite FDGR evidence, anchors, plans, and receipts. FDGR authoritative evidence
> never cites an Eidetic memory as proof.

When a memory says “the propane tank is behind the west garage wall” and a current observation
contradicts it, current evidence wins. The memory receives harmful/stale feedback or a tombstone;
it does not override the scene.

## Memory classes useful to FDGR

- procedural capture rules;
- device/profile compatibility lessons;
- protocol hypotheses and experiment notes;
- calibration conventions;
- geometry/model failure patterns;
- semantic review anti-patterns;
- property-specific owner terminology;
- archive/release procedures;
- historical decisions and rejected alternatives;
- unresolved questions with revival conditions;
- human preferences for exports, privacy, and visualizations.

Memories about physical state include exact FDGR anchor/evidence pointers and expiration/reverify
conditions. Generic procedures can remain broader.

## Session workflow

At task start an agent can run:

```text
ee resume
→ recent campaigns and unresolved obligations
ee pack "plan a follow-up exterior capture"
→ compact relevant rules, failures, profile notes, and owner conventions
ee recall --path ...
→ code/document-specific lessons before edits
ee search / ask / why
→ evidence-backed memory investigation
```

During and after work, structured FDGR artifacts can be curated into candidate memories:

- doctor findings;
- failed protocol experiments;
- geometry benchmark results;
- obligation outcomes;
- qualification receipts;
- human review decisions.

Promotion is deliberate. Raw logs or model outputs do not automatically become durable procedural
rules.

## Explainable context packing

The memory layer can use hybrid lexical/semantic/graph retrieval and submodular or profile-based
packing to provide a small diverse context. Every selected item explains:

- why it matched;
- source/provenance;
- confidence and freshness;
- helpful/harmful feedback;
- relation to other memories;
- what would cause revalidation or revival.

FDGR can emit a task primer with canonical current state and optionally invite the agent harness to
append an external memory pack. The two sections remain labeled and machine-separable.

## Confidence, decay, and outcome feedback

Memory confidence is not sensor confidence. It reflects utility and support of the remembered
lesson. Rules decay, especially when tied to firmware, model, toolchain, or property state. Harmful
feedback demotes faster than helpful feedback promotes. A memory tied to a superseded device
profile is automatically low-priority or revived only when that profile reappears.

Agents record outcomes when a memory helped or misled them. The feedback event cites the task and,
where appropriate, FDGR evidence showing the consequence.

## Revival conditions

Useful retired hypotheses can carry conditions such as:

- new capture overlaps the blind north roof region;
- firmware/profile changes to a named epoch;
- a metric scale witness becomes available;
- a native decoder reaches a qualification gate;
- a model with a compatible license/profile is admitted;
- a missing utility component is observed;
- a release worker for a target comes online.

Revival checks propose context; they do not mutate FDGR or memory trust automatically.

## Graph-aware memory

Memory relations can represent:

- supports/contradicts;
- supersedes;
- derived-from artifact;
- applies-to device/profile/model/property;
- caused failure;
- mitigated by procedure;
- prerequisite/revival condition.

PageRank/PPR, dominance, causal paths, skyline views, and structural health help retrieve compact
context. These graphs remain advisory and separate from the canonical scene/provenance graph.

## Swarm and crowded-checkout use

Multiple agents can use memory for orientation and handoff, but authoritative coordination belongs
to FDGR leases, branches, Beads/work-package state, and repository/source control. A private memory
cannot reserve a DJI command lane, claim a file, or publish a reconstruction.

Handoff capsules may include:

- current git/source identities;
- active FDGR branch and anchor;
- completed local qualification receipts;
- unresolved questions;
- candidate memory IDs;
- exact next commands.

The receiving agent rechecks live canonical state.

## Automatic export templates from FDGR

FDGR may provide read-only commands that transform selected artifacts into `ee remember --batch`
candidates:

- anti-pattern candidate from a failed flight/coverage plan;
- procedural candidate from a repeated successful compatibility sequence;
- risk candidate from a geometry contradiction;
- decision candidate from an accepted ADR or model-profile selection;
- command/rule candidate from local qualification.

The export marks every item `candidate`, includes provenance, and omits secrets/home imagery. Human
or agent curation owns final promotion.

## Privacy and isolation

One memory workspace per property/campaign is the safe default. Shared memory needs explicit
redaction and team policy. Sensitive coordinates, images, credentials, and detailed home layouts
are not copied into prose memories when a digest/evidence pointer suffices.

Memory indexes are derived and rebuildable. Export/support bundles have explicit inclusion and
redaction manifests.

## Superficial imports rejected

- treating memory as a cache of current scene state;
- using remembered device authority or credentials;
- letting memory grant leases/capabilities;
- automatic promotion of model captions or raw logs;
- hiding stale profile-specific advice in a generic rule;
- coordination by private memory instead of FDGR fencing;
- copying sensitive evidence into every memory;
- canonical evidence depending on an `ee` database or index;
- skipping current observation because “it worked last time.”

## FDGR admission gate

1. There is no Rust dependency from FDGR core to Eidetic Engine.
2. Memory and canonical sections are labeled and machine-separable.
3. Physical-state memories require evidence pointers and reverify conditions.
4. Contradiction tests prove current FDGR evidence always wins.
5. Capabilities, leases, and effects cannot be sourced from memory.
6. Export templates produce reviewable candidates, never silent promotion.
7. Property/campaign isolation and redaction are explicit.
8. Outcome feedback and decay handle stale firmware/model lessons.
9. Memory-index loss has no effect on FDGR correctness.
10. Handoffs require live anchor/source revalidation.

---

## Agent-native synthesis

### Evidence-linked accretion

Eidetic indexes handoffs, episodes, surprises, negative lessons, procedural candidates, and qualification receipts. Pack/why/outcome ideas inform FDGR Pack DNA and feedback. Memory remains advisory: it can suggest a question or plan but cannot close the question or authorize commit.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
