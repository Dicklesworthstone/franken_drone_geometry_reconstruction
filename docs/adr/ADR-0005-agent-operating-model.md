# ADR-0005 — Agent operating model is the system center

**Status:** Accepted design

## Context

FDGR must remain coherent for an agent operating across capture, reconstruction, semantics, storage, verification, and recovery. A component-specific interface would force the agent to reconstruct system state and would undermine token economy and safety.

## Decision

FDGR SHALL be organized around one agent control loop and abstraction tower. Subsystems are implementations beneath that center and may not expose competing lifecycles.

## Consequences

A fresh agent can orient and act without mentally joining subsystem-specific status models.

All affected schemas, registries, tests, work packages, and public documentation must remain traceable to this decision.
