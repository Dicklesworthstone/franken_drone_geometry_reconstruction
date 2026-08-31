# ADR-0007 — Canonical Agent Turn Packet

**Status:** Accepted design

## Context

FDGR must remain coherent for an agent operating across capture, reconstruction, semantics, storage, verification, and recovery. A component-specific interface would force the agent to reconstruct system state and would undermine token economy and safety.

## Decision

Every success, progress record, and error SHALL include the same anchor-bound orientation spine and synchronized four ledgers.

## Consequences

Continuity, active work, uncertainty, coverage, and next protocol state survive context loss and errors.

All affected schemas, registries, tests, work packages, and public documentation must remain traceable to this decision.
