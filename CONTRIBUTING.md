
# Contributing

Read `AGENTS.md`, `ARCHITECTURE.md`, the comprehensive plan, and the relevant machine registries
before changing behavior. FDGR is plan-first because errors in identity, timing, coordinates,
scale, publication, and evidence become impossible to repair after large datasets exist.

## Required local qualification

```bash
python3 scripts/validate_repo.py
cargo fmt --all --check
cargo check --workspace --all-targets --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --all-targets --locked
```

A pull request that adds a model, dependency, executable, durable field, coordinate convention,
claim, effect, capability, or readiness statement must update the relevant registry and usually an
ADR. Do not delete or renumber published stable IDs; supersede them with tombstones.

## Change evidence

Describe:

- the semantic contract changed;
- exact positive and negative tests;
- deterministic replay impact;
- cancellation/crash boundary;
- compatibility and migration behavior;
- dependency/model/license impact;
- privacy and authority impact;
- performance evidence, if any;
- rollback path.

A benchmark improvement without semantic equivalence is not an improvement. A negative test does
not prove the success path. Source code for a feature does not justify marking it implemented.
