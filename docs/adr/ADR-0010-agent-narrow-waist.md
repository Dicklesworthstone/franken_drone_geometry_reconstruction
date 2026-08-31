# ADR-0010 — Eleven-operation semantic narrow waist

**Status:** Accepted design

## Context

FDGR must remain coherent for an agent operating across capture, reconstruction, semantics, storage, verification, and recovery. A component-specific interface would force the agent to reconstruct system state and would undermine token economy and safety.

## Decision

All CLI, MCP, NDJSON, TUI, and UI actions SHALL compile into the eleven registered semantic operations. Domain verbs remain ergonomic sugar.

## Consequences

Surface area stays discoverable while semantic capability remains broad and non-bypassable.

All affected schemas, registries, tests, work packages, and public documentation must remain traceable to this decision.
