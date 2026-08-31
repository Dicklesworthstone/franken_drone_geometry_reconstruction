# Self-Description and First-Try Inevitability

An agent should discover the correct operation without reading source code or deliberately causing errors.

## Required surfaces

FDGR exposes stable machine-readable manifests for operations, capabilities, affordances, schemas, fields, profiles, statuses, errors, recovery classes, model/device maturity, effect classes, and qualification lanes.

CLI `--help`, `help <path>`, `--help-json`, robot docs, MCP tool schemas, examples, and compatibility reports are generated from or parity-tested against those manifests.

## Error pedagogy

Usage and domain errors preserve the actual failing field, valid alternatives, closest safe correction, exact repair operation, anchor validity, possible external-effect status, and minimum safe next step. Unknown values never map to a dangerous default.

## Intent inference

The CLI may suggest corrections for close command/flag/profile names, aliases inherited from sibling tools, and common argument-position errors. It must not automatically correct an effectful request unless the resulting sealed plan is shown for explicit commit.

## Maturity honesty

Capabilities distinguish implemented, scaffolded, research-only, blocked, degraded, qualified, and retired states. Source presence, model download, successful subprocess exit, or a negative rejection test is not promoted as feature readiness.

## Canonical vocabulary

Self-description publishes the exact lower `snake_case` field, enum, operation, profile, recovery,
and schema names emitted on every machine surface. It never advertises an alias as canonical. The
payload schema identity convention is `fdgr.<name>/1`; examples and help are parity-tested against
the same registry source.
