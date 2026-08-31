# ADR-0008 — Context packs require Pack DNA

**Status:** Accepted design

## Context

FDGR must remain coherent for an agent operating across capture, reconstruction, semantics, storage, verification, and recovery. A component-specific interface would force the agent to reconstruct system state and would undermine token economy and safety.

## Decision

Every bounded context pack SHALL explain mandatory inclusion, marginal gain, redundancy, omissions, coverage, and continuation.

## Consequences

Token economy becomes auditable rather than an opaque summarizer choice.

All affected schemas, registries, tests, work packages, and public documentation must remain traceable to this decision.
