# ADR-0009 — Episode and surprise based accretion

**Status:** Accepted design

## Context

FDGR must remain coherent for an agent operating across capture, reconstruction, semantics, storage, verification, and recovery. A component-specific interface would force the agent to reconstruct system state and would undermine token economy and safety.

## Decision

Learning SHALL begin from immutable episode and surprise capsules and reach production policy only through replay, shadow, canary, monitoring, and rollback.

## Consequences

The system improves over time without memory becoming hidden authority.

All affected schemas, registries, tests, work packages, and public documentation must remain traceable to this decision.
