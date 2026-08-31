# ADR-0011 — One canonical machine vocabulary

**Status:** accepted design

## Decision

Every public FDGR JSON, JSONL, MCP, CLI-JSON, receipt, manifest, registry projection, and model-worker
message uses one canonical vocabulary:

- field and `$defs` names are lower `snake_case` ASCII;
- schema payload identities use `fdgr.<name>/1`;
- enum values are lower `snake_case` ASCII unless a cited external standard requires otherwise;
- digests are lower hexadecimal with an algorithm/domain supplied by the enclosing contract;
- units, coordinate frames, time bases, scale authority, and privacy scope are explicit fields;
- aliases may be accepted only at ingress under a versioned migration profile and are never emitted;
- CLI, MCP, NDJSON, context packs, receipts, and reports derive names from the same registry entries.

## Why

A mixed naming surface imposes hidden translation state on every agent and adapter. Translation
creates first-try failures, duplicated schemas, lossy handoffs, inconsistent examples, and subtle
cache/signature differences. FDGR is still pre-implementation, so compatibility debt has no value.
The wire form should be made singular before external consumers exist.

## Consequences

The initial camelCase draft fields and `.v1` payload identifiers are replaced rather than retained
as permanent aliases. Golden schema fixtures, self-description, typo repair, migration tests, and
registry parity enforce the canonical form. A future breaking vocabulary change requires a new
schema major, explicit migration, and dual-read/single-write transition plan.
