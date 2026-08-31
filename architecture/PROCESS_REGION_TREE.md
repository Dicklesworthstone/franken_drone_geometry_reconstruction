# Asupersync Process and Region Tree

Asupersync is the exclusive asynchronous programming model. The region tree mirrors the agent abstraction tower so ownership, budgets, cancellation, and evidence remain legible.

```text
process
├── authority-root
├── ledger-supervisor
│   ├── capsule-ingest
│   ├── publication-coordinator
│   ├── evidence-sealer
│   └── migration/recovery
├── custody-supervisor
│   ├── local-object-store
│   ├── ATP-transfer
│   ├── cloud-replication
│   ├── retrievability-scrub
│   └── repair/restore
├── source-supervisor
│   ├── DJI-profile-lab
│   ├── live-read-pump
│   ├── recorded-import
│   └── media-sidecar
├── cognition-supervisor
│   ├── constraint/pose branches
│   ├── geometry builders
│   ├── scene/semantic resolver
│   ├── graph/search generations
│   ├── question/objective projector
│   └── context-pack builder
├── qualification-supervisor
│   ├── lab campaigns
│   ├── benchmark lanes
│   └── Doodlestein receipts
└── session
    ├── orientation/watch stream
    ├── query/explain scope
    ├── counterfactual branches
    └── committed plan
        ├── lease/fence renewer
        ├── checkpoint prerequisite
        ├── step obligations
        │   ├── external effect
        │   ├── observation/reconciliation
        │   └── terminal proof
        └── episode/handoff publication
```

## Context-carried authority

Every operation that can block, allocate shared resources, publish, invoke a process/device/provider, or create child work receives `&Cx`. Context carries identity, anchor, capability set, privacy scope, deadline, CPU/GPU/memory/I/O/network/token/operator budgets, cancellation, pressure, policy epochs, trace, and replay identity.

## Cancellation

Cancellation follows:

```text
request → prevent new effects → drain cooperative work
        → reconcile or compensate external effects
        → seal evidence and progress certificate
        → finalize region
```

Long drains expose a nonnegative potential such as remaining frames, pending factors, unpublished objects, unverified upload parts, active children, or unresolved effects. Failure to prove safe quiescence returns `indeterminate` rather than silently dropping work.

## Pressure and admission

Pressure signals flow upward; budgets and admission decisions flow downward. A model worker may be throttled or cancelled without starving evidence sealing. Safety, continuity, and reconciliation lanes have priority over speculative refinement.

## Deterministic laboratory

The same region topology runs under virtual time and controlled scheduling. The lab explores cancellation points, delayed/reordered/duplicate messages, process death, clock jumps, partial files, disk faults, stale leases, transfer corruption, provider ambiguity, and publication kill points.
