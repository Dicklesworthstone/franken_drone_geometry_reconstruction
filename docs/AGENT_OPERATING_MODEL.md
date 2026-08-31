# FDGR Agent Operating Model

**Document class:** normative agent-control architecture
**Status:** target contract for public iteration
**Applies to:** CLI, MCP, NDJSON streams, local daemon, viewers, errors, task updates, reports, handoffs, and future pilot-assistance surfaces

`franken_drone_geometry_reconstruction` is not primarily a collection of capture, geometry, graph,
model, and storage components. It is a **closed-loop cognitive and control substrate** for an agent
that must build and maintain an evidence-grade model of a physical place while minimizing tokens,
GPU time, storage, network transfer, pilot attention, battery consumption, privacy exposure,
physical risk, reflight burden, and epistemic error.

Every subsystem exists to make one shared loop more truthful, economical, safe, replayable, and
accretive. No subsystem may expose a competing lifecycle or force the agent to reconstruct hidden
protocol state from unrelated outputs.

---

## 1. The driver's-seat contract

After every successful or failed operation, an agent must be able to answer these questions without
re-reading a transcript or issuing a full-state dump:

1. **What physical-world propositions are currently admitted, and at what authority?**
2. **What changed since the exact anchor I last understood?**
3. **What remains unknown, contradicted, stale, weakly constrained, or uncovered?**
4. **What work is active, blocked, awaiting confirmation, draining, or indeterminate?**
5. **What can I legally and safely do next with my current capabilities?**
6. **Which next steps have the highest expected decision value per total control cost?**
7. **What evidence, assumption, policy, model, and tie-break produced each recommendation?**
8. **What would falsify the current view or change the preferred plan?**
9. **What did prior episodes teach, and where does that lesson stop applying?**
10. **Can another agent resume safely from a compact machine packet alone?**

The interface fails the driver's-seat test if the agent must:

- infer whether raw evidence, geometry, semantic, archive, or policy continuity was broken;
- guess whether a statement is observed, calibrated, certified-derived, model-proposed, inferred,
  predicted, assumed, stale, contradicted, unknown, or indeterminate;
- remember opaque handles from an earlier context window to discover unfinished work;
- inspect implementation-level ffmpeg, model, storage, graph, or DJI details to learn what semantic
  actions are available;
- ask for all frames, all points, or the entire scene graph to learn what matters;
- trust an attractive mesh or a high model score without coverage, uncertainty, and counterevidence;
- select a capture or compute action without cost, risk, reversibility, and expected information
  value;
- manually connect a user goal to claims, evidence deficits, acquisition steps, reconstruction jobs,
  and terminal predicates;
- retry an external effect whose outcome is indeterminate;
- spend tokens restating stable context that FDGR can retain structurally;
- lose failed strategies, surprises, and applicability boundaries between sessions;
- inherit another agent's assumptions without their basis anchor and expiry conditions.

The high-level success criterion is simple:

> The agent spends cognition on understanding the property and choosing evidence-efficient actions,
> not on reconstructing FDGR's control plane.

---

## 2. Optimization objective: decision quality per total control cost

FDGR does not optimize latency or token count in isolation. It optimizes expected decision quality
under a multidimensional cost and risk envelope:

```text
total control cost = agent input/output tokens
                   + canonical and derived reads
                   + CPU and GPU work
                   + memory pressure
                   + local I/O and retained bytes
                   + network transfer and egress
                   + model invocations
                   + wall-clock delay
                   + pilot attention
                   + aircraft battery and flight time
                   + physical and regulatory exposure
                   + privacy exposure
                   + reflight and recovery burden
                   + future control complexity created
```

A cheap answer that causes a bad flight, an unnecessary full reconstruction, an unverified deletion,
or an overclaimed utility location is expensive. A more detailed answer is wasteful when every
feasible refinement leads to the same safe decision.

The governing quantity is therefore:

```text
agent efficiency = expected objective progress and uncertainty reduction
                   ----------------------------------------------------
                               total control cost
```

Safety, authority, privacy, continuity, and proof requirements are hard constraints, not terms that
can be traded away in this ratio.

---

## 3. One synthetic operating loop

Every human- or agent-directed activity maps onto one loop:

```text
BOOTSTRAP
  negotiate semantic version, campaign, authority, policy, privacy, budgets, profiles
      ↓
ORIENT
  receive anchor vector, continuity, four ledgers, semantic delta, attention, active work
      ↓
FOCUS
  select objective, question, spatial/temporal scope, branch, and decision horizon
      ↓
INSPECT
  expand only evidence whose expected value can change a decision boundary
      ↓
FORMULATE
  declare desired predicates, forbidden states, utility, evidence standard, stop conditions
      ↓
PROPOSE
  compile bounded candidate plans with assumptions, witnesses, predicted deltas, costs, risks
      ↓
COMPARE
  evaluate a Pareto frontier and counterfactual branches under one basis and policy epoch
      ↓
COMMIT
  revalidate evidence, capability, leases, privacy, budgets, and effect preconditions; dispatch once
      ↓
WATCH
  consume semantic progress deltas rather than polling implementation details
      ↓
VERIFY / RECONCILE
  observe authoritative outcomes, prove terminal predicates, resolve ambiguous external effects
      ↓
LEARN
  seal episode, surprise, actual cost, regret evidence, and bounded lesson candidates
      ↓
HANDOFF / RESUME
  transfer a compact state of understanding and required next protocol step
```

A tool may span multiple phases internally, but every response states the current phase, the
completed transition, and the minimum safe next phase. No capture, reconstruction, archive, model,
or graph subsystem may invent another user-visible lifecycle.

---

## 4. The canonical abstraction tower

The agent sees one tower of linked abstractions. Each layer references lower layers by immutable
identity and may be expanded without semantic translation:

```text
L9  Campaign / mission / policy
L8  Objective graph and success predicates
L7  Question graph, uncertainty, coverage, and evidence deficits
L6  Candidate plans, counterfactual branches, and decision cards
L5  Obligations, effects, progress, verification, and surprise
L4  Scene claims: geometry, topology, semantics, measurements, change
L3  Constraint fabric: poses, tracks, scale, calibration, visibility, residuals
L2  Observation capsules and versioned evidence history
L1  Immutable objects, source bytes, clocks, custody, transfer, and repair
L0  External physical/device/process effects
```

The presentation layer offers **semantic zoom**, not disconnected APIs:

```text
campaign → objective → question → claim/deficit → plan → obligation → evidence → raw object
```

Every summarized item supplies typed expansion handles. The agent may zoom down to raw frames,
rays, residuals, packet intervals, or receipts, then return to the same higher-level object without
losing identity or anchor.

### 4.1 No sideways semantic ownership

- Capture adapters produce observations, never semantic asset truth.
- Models produce proposals, never admitted geometry or resolved assets.
- Graph/search/attention produce derived views, never effects.
- Plans reference claims and witnesses, but cannot rewrite them.
- Memory may influence proposal ranking, but cannot satisfy a precondition.
- Presentation may rename or compact fields, but cannot invent state.

---

## 5. The four ledgers

The default orientation packet separates four kinds of state that agents often confuse.

### 5.1 World ledger

What FDGR currently claims about the physical place:

- admitted geometry and topology generations;
- scale status and measurement authority;
- resolved and unresolved assets;
- detected change across captures;
- branch and historical context;
- exact spatial and temporal scope.

### 5.2 Epistemic ledger

What is known about the quality and limits of those claims:

- evidence basis and provenance;
- question states and candidate answers;
- coverage and detectability;
- uncertainty decomposition;
- contradictions and negative evidence;
- stale assumptions and invalidated witnesses;
- missing observations that could change a decision.

### 5.3 Work ledger

What the system and agents are doing:

- missions and objectives;
- pending plan candidates and prepared plans;
- branches and comparisons;
- committed obligations and semantic progress;
- leases, confirmations, drains, and reconciliation;
- exact minimum safe next step for every nonterminal item.

### 5.4 System ledger

What the platform can presently support:

- device/source compatibility and continuity;
- model and algorithm availability;
- local and remote custody posture;
- compute/storage/network pressure;
- capability grants and privacy scope;
- degraded features, qualification state, and repair actions.

A response may omit low-salience entries under budget, but it may never collapse these ledgers into
one ambiguous prose summary.

---

## 6. The Agent Turn Packet

Every success, partial result, error, progress event, and terminal receipt carries the same
`agent_turn` spine. Tool-specific data is additive. The packet is a certified orientation view, not
a second source of truth.

Canonical field order:

```text
schema
operation
phase
status
error
recovery
session_id
turn_id
request_id
anchor
continuity
profile
focus
decision_frame
ledgers.world
ledgers.epistemic
ledgers.work
ledgers.system
changes
attention
affordances
recommendations
uncertainty
coverage
budget
references
continuation
```

`status` is semantic rather than transport-level. `error` is always present and nullable; when
non-null it declares a stable code, whether state changed, retryability, and evidence references.
`recovery` is always present and nullable; blocked, failed, cancelled, or indeterminate turns carry
a typed recovery class and minimum safe next step. An agent never infers retry safety from prose,
HTTP/JSON-RPC success, or the absence of a payload.

### 6.1 Identity

`turn_id` identifies an authority-free presentation turn. `request_id` identifies a semantic
operation that may own authority, budgets, and effects. They are distinct and are never silently
aliased. A response created before a semantic request exists uses `request_id: null`.

### 6.2 Anchor vector

A useful FDGR state is multi-dimensional. The packet binds one coherent vector:

```text
campaign lineage
capture epoch and evidence high-water
clock generation
calibration generation
scale generation
geometry branch and generation
scene/ontology generation
coverage generation
search/index generation
archive/custody generation
schema and policy epochs
```

Every public occurrence uses `schemas/anchor_vector.schema.json`; transport-specific partial anchor objects are forbidden. Each derived member names the evidence high-water it consumed. A packet cannot combine a scene
claim from one geometry branch with a measurement from another unless the relation is explicitly
comparative.

### 6.3 Continuity

Continuity is reported per relevant lineage and summarized as:

- `bootstrap`: no acknowledged basis;
- `continuous`: exact basis-to-target continuity proved;
- `heartbeat`: no meaningful change under the selected profile;
- `partial`: complete only for named scope and continuation;
- `gap`: a required delta interval is unavailable;
- `reset`: capture, clock, calibration, geometry, scene, or policy epoch changed incompatibly;
- `stale`: valid at its anchor but a fresher compatible state exists;
- `indeterminate`: external effect or evidence continuity cannot yet be classified.

FDGR never hides a reset by silently presenting a new “latest” state.

### 6.4 Changes

Changes are semantically ordered, not arrival ordered. Each item states:

- subject and change kind;
- before/after or event semantics;
- epistemic class and evidence;
- spatial/temporal scope;
- salience and downstream consequence;
- plans, questions, claims, or recommendations invalidated;
- whether the change creates a surprise record.

Ordering is continuity hazard, custody/irreversibility hazard, safety/privacy hazard, active-work
invalidation, objective impact, information value, then canonical identity.

### 6.5 Attention

Attention answers “what deserves cognition now?” Each item carries:

```text
attention_id
category and subject
severity, urgency, expiry
objective and question impact
confidence and epistemic class
coverage/uncertainty contribution
likely consequence if ignored
candidate information or control responses
score ledger and canonical tie-break
```

Attention is authority-free. It can propose an inspection or objective but cannot commit work.

### 6.6 Affordances

An affordance is a currently expressible semantic action template, never a raw shell command,
ffmpeg argument list, SQL statement, vendor packet, or model prompt. It declares:

- operation family and typed parameter schema;
- target scope and branch;
- capability, privacy, risk, and profile requirements;
- already witnessed preconditions;
- unresolved preconditions and the cheapest ways to establish them;
- estimated cost vector and resource lane;
- reversibility, checkpoint, confirmation, and compensation policy;
- predicted evidence/world/work deltas;
- reasons disabled or degraded.

Affordance visibility reduces hallucinated actions and wasted planning calls. It does not promise
that commit-time revalidation will succeed.

### 6.7 Recommendations

Recommendations are ranked **next protocol steps**, not free-form advice. Types include:

- inspect or query;
- acquire evidence;
- formulate or refine objective;
- compare candidate plans;
- commit, wait, cancel, or reconcile;
- request human measurement or confirmation;
- repair, restore, qualify, or stop;
- deliberately do nothing.

Each recommendation includes:

```text
recommendation_id
operation template
objective/question advanced
reason and decision boundary
expected objective progress
expected information value
cost vector
risk and reversibility
prerequisites and invalidators
confidence, evidence, and policy epoch
confirmation/operator requirement
counterfactual: what happens if skipped
```

Ranking is lexicographic and safety-first:

1. continuity, custody, authorization, privacy, or indeterminate-effect hazards;
2. imminent loss of original evidence or unsafe physical operation;
3. required reconciliation or confirmation for active work;
4. hard objective and deadline blockers;
5. high-value information that can change the preferred plan;
6. expected objective progress;
7. future control-cost and uncertainty reduction;
8. lower risk, lower cost, and greater reversibility among equivalent choices;
9. canonical tie-break.

The system may return no recommendation. It must never invent work merely to appear active.

### 6.8 Budget and omissions

Every packet reports requested, admitted, consumed, and remaining budget by category. When a soft
or hard limit is reached, the response remains structurally valid and names:

- which optional sections were omitted;
- why each omission was lower priority;
- whether the omitted information could change the current recommendation;
- a sealed continuation for deterministic expansion.

Safety, continuity, active indeterminate work, and coverage limits are never sacrificed to save
output tokens.

---

## 7. Observation and context profiles

Profiles are stable contracts, not vague verbosity levels.

### `pulse`

Cheapest safe heartbeat. Target roughly 150–400 output tokens when nothing exceptional changed.
Contains continuity, critical changes, active-work transitions, indeterminate effects, top one to
three recommendations, critical uncertainty, and budget. No unchanged inventory.

### `briefing`

Default cold-start or context-reset orientation. Target roughly 500–1,500 tokens. Adds the four
ledger summaries, objective progress, top questions, bounded affordances, and important unknowns.

### `tactical`

Decision-specific working set for one objective, question, region, asset, plan, or obligation. Adds
causal/provenance neighborhoods, witnesses, blockers, candidate alternatives, and finer geometry or
coverage detail.

### `pilot`

Low-distraction live-capture profile. It returns:

- one primary maneuver or hold/stop instruction;
- one fallback instruction;
- safety and continuity state;
- target region and why it matters;
- expected evidence gain;
- whether the previous instruction achieved its predicted gain;
- battery/link/pilot-attention cost;
- no dense technical diagnostics unless safety requires them.

Pilot guidance is relative when metric pose is not witnessed. Operator acknowledgment is not
coverage success; the next observation proves whether the requested evidence was obtained.

### `forensic`

Evidence-complete, bounded diagnosis or replay for an indeterminate effect, geometry defect,
semantic dispute, archive problem, or policy decision. Requires explicit scope and may require a
diagnostic capability.

### `custom`

Explicit union of registered projections under hard bounds. Unknown projection names fail closed.

---

## 8. Epistemic model

Confidence is not authority. A 0.99 model prediction remains a model prediction.

Each proposition carries an epistemic class:

- `raw_observed`: present in immutable acquired source bytes or authenticated device observation;
- `normalized_observed`: deterministically decoded/normalized with complete source mapping;
- `certified_derived`: produced by an admitted deterministic derivation with complete provenance;
- `model_proposed`: produced by an identified model worker and not yet admitted;
- `inferred`: supported but not entailed by admitted evidence;
- `predicted`: output of a prospective or counterfactual model;
- `assumed`: supplied as an unverified planning premise;
- `stale`: formerly valid but outside freshness, epoch, or applicability bounds;
- `unknown`: not established;
- `contradicted`: materially refuted by current evidence;
- `indeterminate`: available evidence cannot distinguish consequential alternatives.

A separate disposition records claim lifecycle (`Observation`, `Hypothesis`, `Resolved`,
`Rejected`, `Indeterminate`). A separate confidence/uncertainty structure records quantitative
belief. These axes must not be collapsed.

Only eligible observed and certified-derived facts may satisfy an effect precondition. Model
proposals, inferences, predictions, assumptions, and memories can motivate investigation or
candidate generation but cannot issue authority.

### 8.1 Uncertainty vector

FDGR never compresses all uncertainty into one number. Components may include:

- source integrity and timing;
- calibration and rolling shutter;
- pose and loop closure;
- scale and coordinate registration;
- depth/surface geometry;
- topology;
- dynamic/reflective/non-Lambertian behavior;
- semantic identity and association;
- coverage and detectability;
- temporal freshness/change;
- model and policy uncertainty.

Each component names a resolution path or states why none is currently available.

---

## 9. Question-first control

The central bridge between human intent and system machinery is a first-class **Question**.

Examples:

- Is metric scale witnessed for the north facade?
- Does the east exterior wall contain a visible water spigot?
- Which geometry branch best explains the garage opening?
- Is the roof edge topologically connected or only visually fused?
- Can the original capture be restored from verified remote custody?
- Which 60-second flight would most reduce uncertainty relevant to the user's goal?

A question contains:

```text
question_id
proposition or bounded answer space
objective and decision impact
spatial, temporal, branch, and privacy scope
resolution policy and evidence standard
candidate answers and current support/counterevidence
coverage/detectability requirements
unknowns, assumptions, and expiry
possible evidence-acquisition actions
status and terminal predicate
```

Question states are:

```text
open → investigating → answer_proposed → resolved | rejected | indeterminate | suspended
```

### 9.1 The unifying graph

```text
Mission
  → Objectives
    → Questions
      → Evidence deficits
        → Candidate observations/computations
          → Plans
            → Obligations
              → New evidence
                → Claims and answers
                  → Objective progress
```

This graph connects every subsystem. A model run, graph algorithm, follow-up flight, manual
measurement, archive verification, and semantic review is justified by the question it can answer
and the decision boundary it can change.

### 9.2 Dominating questions

Graph algorithms identify questions whose resolution dominates many downstream objectives,
minimal question cuts that unblock a campaign, contradiction clusters, and high-value independent
evidence sets. This prevents locally attractive but globally irrelevant work.

---

## 10. Missions, objectives, and stop conditions

A mission is durable user intent across sessions and captures. An objective is a typed,
inspectable component of that mission. It declares:

- desired terminal predicates;
- hard invariants and forbidden states;
- soft utility terms and policy epoch;
- priority, urgency, and horizon;
- acceptable risk, privacy, and resource envelopes;
- evidence standard and readiness dimensions;
- dependencies and conflicts;
- completion, failure, suspension, abandonment, and review predicates;
- owner and delegation scope.

The planner may decompose an objective but cannot silently rewrite it. Every decomposition has a
digest, rationale, question graph, and provenance. A goal such as “create a reliable home twin” is
not terminal until the selected readiness dimensions, uncertainty bounds, and coverage conditions
are explicit.

Stop conditions are first-class. FDGR must recognize when:

- the mission's evidence standard has been met;
- further observation cannot change a decision enough to justify cost;
- available capture conditions make additional work low-value;
- a hard constraint prevents safe progress;
- a question is fundamentally unobservable from the admitted source;
- operator intervention is required.

Endless capture and endless refinement are failures of planning, not signs of thoroughness.

---

## 11. Context packs and semantic zoom

An agent should receive the smallest sufficient working set, not a generic summary or arbitrary
top-k retrieval.

A context pack is sealed to:

- anchor vector;
- focus (campaign/objective/question/region/branch/time);
- observation profile;
- capability and privacy projection;
- policy and retrieval generations;
- token/byte budget;
- exact selection algorithm and tie-break.

### 11.1 Pack structure

```text
identity and continuity
mission/objective/task frame
four-ledger situation
semantic delta since basis
top questions and attention
active work and required next steps
affordances and recommendation frontier
selected evidence and counterevidence
uncertainty, coverage, assumptions, and omissions
typed expansion/challenge/compare/plan handles
pack DNA and digest
```

### 11.2 Pack DNA

The pack explains its own composition:

- why each item was included;
- which objective/question it supports;
- novelty, authority, freshness, graph/causal role, and safety contribution;
- which relevant candidates were omitted and why;
- whether the pack is sufficient for the named decision;
- what expansion would have the highest marginal value.

Selection uses deterministic constrained submodular coverage over question, evidence, risk,
contradiction, and provenance dimensions. Retrieval score alone is not enough.

### 11.3 Stable handles

Every item has a typed semantic address such as:

```text
campaign:<id>
objective:<id>
question:<id>
claim:<id>
region:<id>
asset:<id>
plan:<digest>
obligation:<id>
evidence:<digest>
frame:<source>/<epoch>/<index>
branch:<id>
```

Session aliases may shorten display, but canonical identities remain in the packet. Handles support
`expand`, `why`, `challenge`, `compare`, `measure`, `history`, and `propose` operations without raw
paths or arbitrary executable strings.

---

## 12. Proposal and candidate planning

`propose` compiles one typed objective or question-resolution request against one anchor vector.
When materially different safe strategies exist, it returns a bounded candidate set rather than
prematurely choosing one.

Each candidate includes:

- objective/question basis;
- assumptions and their epistemic classes;
- read, write, negative, spatial, and generation witnesses;
- predicted world, evidence, epistemic, work, and system deltas;
- expected objective progress and information gain;
- full cost vector and resource schedule;
- risk, reversibility, checkpoint, confirmation, and compensation classes;
- privacy and export implications;
- conflict probability and witness breadth;
- stop and terminal predicates;
- recovery path for each material failure mode;
- plan and decision-path digests.

### 12.1 Pareto frontier

FDGR preserves nondominated alternatives across quality, information gain, latency, compute,
storage, pilot burden, battery, privacy, risk, and reversibility. A named policy may recommend one,
but the raw frontier and dominance reasons remain inspectable.

Examples:

- quick low-resolution coverage pass versus slower high-detail orbit;
- classical reference reconstruction versus model-assisted refinement;
- local CPU-only convergence versus GPU worker dispatch;
- immediate full archive replication versus staged hot/cold custody;
- request a measured scale marker versus retain relative geometry.

### 12.2 Counterfactual branches

Candidate simulation occurs on immutable structurally shared branches. Predicted evidence and world
states are visibly marked and can never become canonical merely by branch merge. Merge means
produce a candidate intent, conflict report, and revalidated plan against live state.

---

## 13. Commit, watch, verification, and reconciliation

Commit is a short, explicit transition:

```text
revalidate anchor and witnesses
→ validate capability, policy, privacy, lease, and budget
→ checkpoint when required
→ reserve idempotency/effect identity
→ issue short-lived effect ticket
→ dispatch once
→ record attempt durably
```

The agent surface distinguishes:

- plan prepared;
- commit accepted;
- external request dispatched;
- output/effect observed;
- output verified;
- generation published;
- terminal predicate satisfied;
- objective advanced/completed;
- stable completion confirmed.

### 13.1 Progress without polling noise

`watch` streams semantic transitions and bounded heartbeats. Progress is expressed through stage,
high-water, unresolved potential, active children, latest evidence, resource use, blocker, and next
expected semantic event. A universal percent complete is forbidden when total work is unknown.

### 13.2 Reconciliation

If a device, process, filesystem, or cloud effect may have occurred but cannot be classified, the
obligation becomes `indeterminate`. Reconciliation dominates recommendations. Blind retry is
forbidden until lookup, readback, or authoritative observation resolves the outcome.

---

## 14. Surprise, regret, and agent accretion

Execution compares predicted and observed deltas. A **surprise record** is emitted when:

- an expected frame, geometry improvement, semantic observation, archive state, or effect is absent;
- an unexpected material delta appears;
- actual resource cost materially differs from the candidate estimate;
- a question remains unresolved despite predicted information gain;
- a precondition or coverage assumption changes;
- a recommendation would have ranked differently with newly observed facts;
- compensation or cancellation fails to restore predicted state.

Surprise is the atomic unit of useful learning. Silent prediction error prevents accretion.

### 14.1 Episode capsule

Every closed loop produces an immutable episode capsule containing:

```text
mission/objective/question
orientation and context-pack digests
candidate set and decision card
selected plan and witnesses
predicted deltas and cost
actual effects, evidence, outcomes, and cost
surprises and unresolved confounders
terminal predicates and objective progress
counterfactual/regret evaluation when available
lesson candidates and applicability fingerprint
```

### 14.2 Knowledge strata

- **Operational episode:** authoritative record of what FDGR attempted and observed.
- **Case model:** derived similarity fingerprint for source/device/scene/model/algorithm conditions.
- **Procedural lesson:** reusable plan/decomposition template with applicability predicates.
- **Negative lesson:** failed approach, refuted assumption, or known unsafe shortcut.
- **Policy candidate:** ranking/budget parameter change supported by episodes.
- **Agent memory:** external advisory context in Eidetic Engine.

The episode is canonical operational history. Lessons and policies are derived and authority-free.

### 14.3 Promotion ladder

```text
raw episode
→ lesson candidate
→ independent support and contradiction review
→ bounded applicability fingerprint
→ shadow recommendation evaluation
→ same-basis counterfactual replay
→ canary policy epoch
→ admitted monitored policy
→ rollback or retirement on adverse evidence
```

No single anecdote silently changes production policy. Adaptive policy can improve efficiency but
cannot weaken evidence, scale, privacy, authority, continuity, or safety gates.

### 14.4 Eidetic integration

Eidetic Engine receives curated candidates and compact evidence pointers. It provides cross-session
orientation, procedural recall, anti-patterns, and handoff context. It never becomes canonical
physical state, a capability source, or a mutation precondition. Current FDGR evidence always wins.

---

## 15. Active perception and human pilot collaboration

Active perception is question-directed. FDGR does not merely seek uncovered pixels; it seeks the
least costly observations that can change a mission-relevant decision.

For each candidate viewpoint or capture maneuver, the system estimates:

- questions and claims affected;
- expected visibility, resolution, parallax, and detectability;
- expected uncertainty reduction and downstream objective value;
- route, battery, time, pilot attention, privacy, and safety cost;
- robustness to pose/scale uncertainty;
- redundancy with already available evidence;
- fallback and stop conditions.

The planner chooses a diverse bounded set, not many near-duplicate views. After execution, observed
information gain is compared with prediction and contributes to the episode/surprise ledger.

Manual pilot guidance is an obligation involving two actors: FDGR proposes and observes; the pilot
decides and flies. The system distinguishes recommendation, operator acknowledgment, maneuver
observation, evidence acquisition, and question resolution.

---

## 16. Multi-agent coherence

Multiple agents may inspect and branch concurrently. Shared coordination is represented in the
work ledger, not private prompts or mutable scratch files.

The packet exposes:

- objective and question ownership/delegation;
- active focus regions and work leases;
- prepared-plan overlap and conflict summaries;
- branch basis and merge/rebase status;
- publication/device/archive/confirmation fences;
- shared versus private speculative artifacts;
- exact handoff and escalation state;
- QoS/resource reservations.

A lease does not make stale knowledge true. A valid witness does not confer ownership. Mutation
requires both current knowledge and authority fencing.

Agents may delegate bounded subgoals with independent budgets and output schemas. Child results are
merged through typed evidence or candidate-plan contracts, not prose consensus. Competing
hypotheses are retained when evidence does not justify collapse.

---

## 17. Error and recovery ergonomics

Every error is a valid Agent Turn Packet. It states:

- which anchors and handles remain valid;
- whether any external effect may have occurred;
- active work that still exists;
- exact recovery class;
- minimum safe next protocol step;
- evidence or operator action needed to resolve it;
- whether retry with identical content is safe;
- whether rebase, reconciliation, confirmation, or new authority is required.

Recovery classes:

- `do_not_retry_unchanged`;
- `safe_read_retry`;
- `refresh_and_retry`;
- `resume_continuation`;
- `rebase_required`;
- `backoff`;
- `reconciliation_required`;
- `confirmation_required`;
- `operator_action_required`.

A typo or invalid field includes the exact corrected operation/template when unambiguous. Unknown
enum values never fall through to a dangerous default. An error does not make the agent start over
unless continuity is genuinely lost.

---

## 18. Self-description and first-try inevitability

An agent must be able to discover the system without reading implementation source. FDGR exposes:

- complete versioned capability manifest;
- exact operation and intent schemas;
- observation/context profiles;
- stable error and recovery dictionary;
- model/device/archive profile maturity;
- cost and qualification posture;
- canonical examples and first executable slice;
- aliases and typo repair where unambiguous;
- `why unavailable` for every omitted or disabled affordance;
- schema status distinguishing shipped, scaffolded, planned, and retired surfaces.

`capabilities`, `describe`, `schema`, `doctor`, and `orient` are read-only and cheap. Help text and
machine manifests are generated from the same registry to prevent drift.

No command documentation may hide valid enum values behind “etc.”, require guessing a nested path,
or expose a machine capability that is absent from the human/agent manifest.

---

## 19. Narrow public waist

The logical agent protocol is intentionally small:

| Operation | Meaning |
|---|---|
| `fdgr.open_session` | Negotiate campaign, versions, authority, profiles, privacy, and budgets. |
| `fdgr.orient` | Return a pulse, briefing, tactical, pilot, forensic, or custom Agent Turn Packet. |
| `fdgr.query` | Execute an exact/ranked/graph/spatial/temporal/provenance query against one anchor vector. |
| `fdgr.propose` | Compile a typed objective or question-resolution request into candidate plans. |
| `fdgr.compare` | Compare plans, branches, generations, captures, or claims under one policy and basis. |
| `fdgr.commit` | Revalidate and start one sealed plan. |
| `fdgr.watch` | Stream obligation progress, evidence deltas, verification, and reconciliation. |
| `fdgr.cancel` | Request cancellation and observe drain/reconcile/finalize. |
| `fdgr.explain` | Explain a claim, question, recommendation, plan, decision, result, error, or policy. |
| `fdgr.handoff` | Create or resume a sealed handoff capsule. |
| `fdgr.doctor` | Diagnose system, evidence, compatibility, custody, and qualification posture. |

Domain-specific CLI verbs such as `capture`, `reconstruct`, `measure`, `archive`, `repair`, and
`export` are ergonomic sugar that compile to `propose → commit → watch`. MCP remains at the narrow
waist. No giant “do everything” operation and no one-tool-per-ffmpeg/model/DJI command surface is
admitted.

---

## 20. Handoff and resume

A handoff capsule is sufficient to resume safely without the full transcript. It includes:

- campaign/session identity and last acknowledged anchor vector;
- active mission, objectives, questions, and focus;
- four-ledger summary and semantic delta basis;
- active plans, obligations, branches, confirmations, drains, and indeterminate effects;
- capability, privacy, budget, and system posture;
- decisions and alternatives already rejected, with reason/evidence;
- next required protocol step for every active item;
- context-pack and advisory memory references;
- content digests, expiry, redaction, and signature/seal state.

Resume always revalidates live anchors, leases, capabilities, and obligations. Handoff context is a
starting point, not authority.

---

## 21. Determinism and semantic stability

Given the same anchor vector, focus, policy, seed, capability/privacy projection, and budget, the
agent surface is deterministic in:

- field and item order;
- context-pack composition and Pack DNA;
- attention and recommendation order;
- candidate and Pareto-frontier order;
- omission priority and continuation boundaries;
- explanations and decision paths;
- typed handles and aliases.

A deeper profile may add evidence or revise a lower-authority hypothesis, but it cannot silently
contradict an admitted observation. Revisions are explicit events carrying old state, new state,
reason, and evidence.

---

## 22. Non-goals and rejected shortcuts

The agent operating model rejects:

- one giant state dump;
- one giant end-to-end autonomous “map my house” call with hidden phases;
- dozens of top-level tools mirroring implementation components;
- free-form executable recommendations;
- hidden server-side goals or silently rewritten objectives;
- confidence presented without epistemic class and uncertainty decomposition;
- recommendation without alternatives, cost, risk, evidence, invalidators, and reversibility;
- context savings obtained by omitting continuity, active work, or safety/privacy state;
- model rationale or private chain-of-thought stored as operational evidence;
- automatic policy promotion from one episode;
- private agent memory used for coordination or authority;
- progress percentages with unknown denominator;
- polling that repeats unchanged state instead of a heartbeat/delta;
- forcing an agent to remember protocol facts FDGR can represent structurally.

---

## 23. Implementation sequence

### Agent Gate A0 — contract lock

- adopt the abstraction tower, four ledgers, question graph, and narrow waist;
- publish Agent Turn Packet, context pack, objective, question, plan candidate, obligation, episode,
  and handoff schemas;
- add stable IDs, golden field order, and deterministic examples.

### Agent Gate A1 — scaffold orientation

- `open_session` and `orient` return truthful scaffold packets;
- pulse and briefing profiles work against the in-memory reference state;
- errors share the same packet spine;
- active work and safe next steps are always visible.

### Agent Gate A2 — epistemic and context semantics

- every claim carries epistemic class, authority, uncertainty, provenance, and scope;
- question/coverage/absence semantics are implemented;
- context packs have deterministic Pack DNA and continuations;
- stable semantic handles support reversible zoom.

### Agent Gate A3 — objective and candidate planning

- objectives decompose into question and evidence-deficit graphs;
- candidate plans expose assumptions, witnesses, predicted deltas, Pareto dimensions, and costs;
- compare uses immutable branches and canonical ordering;
- affordability/unavailability reasons are explicit.

### Agent Gate A4 — obligation loop and active perception

- commit/watch/cancel/reconcile provide full semantic progress;
- recorded capture and one manual active-perception cycle close the loop;
- prediction-versus-observation creates surprise and episode records;
- pilot profile remains compact and non-authoritative.

### Agent Gate A5 — accretion and multi-agent

- handoff/resume works without transcript replay;
- episode-derived lesson candidates and negative evidence are exportable;
- shadow/canary policy promotion is gated;
- multi-agent branches, leases, delegation, QoS, and merge evidence are qualified.

---

## 24. Agent-perspective acceptance scenarios

1. **Cold arrival:** one briefing identifies campaign state, continuity, scale/coverage limits,
   active work, authority, and safest next step.
2. **Cheap heartbeat:** unchanged state emits a compact pulse with no repeated inventory.
3. **Question-directed inspection:** the agent asks whether a spigot exists and receives the exact
   evidence/coverage deficit and highest-value acquisition option, not generic search results.
4. **Plan frontier:** a follow-up capture objective returns materially distinct nondominated
   alternatives with pilot/battery/privacy/quality tradeoffs.
5. **Model disagreement:** competing geometry or semantic proposals remain separate and the packet
   explains what independent evidence could resolve them.
6. **Budget pressure:** the agent receives a complete decision-useful prefix, omissions, Pack DNA,
   and continuation without losing safety or active-work state.
7. **Live guidance:** pilot profile gives one bounded instruction and later reports whether the
   predicted evidence gain occurred.
8. **Context loss:** a fresh agent resumes an active reconstruction or upload from a handoff packet
   without replaying the transcript.
9. **Continuity reset:** camera/app restart or capture-clock reset is unmistakable and stale plans
   are invalidated explicitly.
10. **Indeterminate effect:** blind retry is absent; reconciliation is the top recommendation.
11. **Absence refusal:** no result is reported as unresolved rather than physical absence when
   coverage/detectability is incomplete.
12. **Surprise and learning:** an unexpectedly poor capture creates an episode and lesson candidate
   but does not silently mutate production policy.
13. **Multi-agent conflict:** overlapping prepared plans are visible, branches remain isolated, and
   merge/rebase choices are deterministic.
14. **No useful action:** the packet recommends stopping or waiting rather than generating busywork.
15. **Live/reference parity:** equivalent semantic state yields the same packet shape and decisions
   independent of adapter implementation, except declared provenance/compatibility fields.

## 19. The singular Decision Frame

Every material decision is represented by one anchor-bound Decision Frame that joins objective,
questions, decision-changing evidence deficits, admitted facts, constraints, affordances, candidate
frontier, active obligations, cost/risk envelopes, stopping rule, invalidators, and next protocol
steps. A heartbeat may have no frame. A decision-bearing packet cannot make the agent join several
partial frames.

## 20. Epistemic debt and attention stability

The epistemic ledger names the expected decision loss of unresolved questions. Attention uses
class priority, hysteresis, acknowledgement, suppression, expiry, and material-change re-entry.
Unchanged model outputs cannot repeatedly interrupt the agent; unresolved debt cannot disappear
without a terminal disposition.

## 21. Physical-world legibility and the pilot loop

All spatial references are frame-complete handles with human aliases, coverage, scale authority,
privacy, and semantic-zoom affordances. Manual-flight guidance emits one bounded card at a time and
separates recommendation, operator acknowledgement/refusal, observed motion, usable evidence, and
question closure. Safety preemption and abort always dominate evidence acquisition.

## 21. One machine vocabulary

The abstraction tower is not coherent if every level renames the same concepts. All public FDGR
machine surfaces therefore emit lower `snake_case` fields and enum values and identify payloads as
`fdgr.<name>/1`. CLI JSON, NDJSON, MCP, receipts, model-worker messages, context packs, and reports
share registry-derived names. Compatibility aliases, when a future migration requires them, are
accepted only at ingress under an explicit epoch and are never re-emitted. An agent should learn a
concept once and use the same spelling everywhere.

## 22. Explicit agent feedback without a new tool family

Closed-loop episodes are automatic. When an agent has additional evidence about the usefulness of
a context item, recommendation, explanation, candidate, or pilot card, orientation exposes a
`feedback.record` affordance. The feedback is proposed and committed through the same narrow waist
and becomes an immutable advisory receipt tied to the exact turn, item, episode, reason, and
observed consequence. This gives the system a direct learning signal without granting memory or
feedback the power to rewrite history or mutate policy.
