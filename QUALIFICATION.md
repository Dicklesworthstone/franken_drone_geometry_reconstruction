# Qualification Receipt

**Receipt date:** 2026-08-31
**Artifact status:** agent-native design corpus structurally validated; Rust execution lanes pending

This receipt records checks actually executed in the authoring environment. It is not a blanket
implementation, performance, device, model, or production-readiness claim. GitHub workflow YAML is
a portable job specification for local Doodlestein execution; hosted GitHub Actions is not release
authority.

## Static evidence executed here

```bash
python3 scripts/generate_traceability.py --check
python3 scripts/export_beads_bootstrap.py --output .beads/bootstrap.jsonl --check
python3 scripts/check_dependency_policy.py
python3 scripts/validate_repo.py
python3 scripts/validate_agent_contracts.py
python3 -m py_compile scripts/*.py
bash -n scripts/*.sh
git diff --check
```

The retained checks cover registry/JSON parsing, stable identities, work-package and qualification/job
DAG acyclicity, schema/ADR/reference paths, exact research-source manifests, dependency policy,
agent contracts, generated traceability and Beads bootstrap, Markdown links, script modes, static
forbidden Rust patterns, lockfile/workspace agreement, canonical machine vocabulary, canonical
anchor reuse, and local-only qualification policy. The final static run parsed **21 TOML
registries** and **19 JSON Schemas**, proved **453** globally unique plan-traceable normative IDs,
verified **48** acyclic work packages, and checked **11** operations, **6** context profiles, and
**23** agent invariants.

## Evidence not earned here

The environment did not contain `rustc`, `cargo`, or `rustup`. An explicit `./scripts/qualify.sh
--mode full` attempt completed every static lane and then failed closed with exit code 3 at
`required tool is unavailable: cargo`. Consequently these lanes were **not executed here**:

```bash
cargo fmt --all --check
cargo check --workspace --all-targets --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --all-targets --locked
```

No compile, unit-test, live DJI, geometry, model, archive, agent-ergonomics, or performance result
may be inferred from the design corpus. A pinned-toolchain compile receipt remains pending.

## Complete intended local qualification

```bash
rustup toolchain install nightly-2026-08-31 --profile minimal --component rustfmt,clippy
./scripts/qualify.sh --mode full
```

Doodlestein must retain exact FDGR source, actual admitted sibling closure, toolchain, feature set,
host profile, input roots, command logs, outcomes, and artifact readback before promotion.

The exact dated pin follows the requested current-date nightly policy, but its upstream availability
was not independently fetched in this network-restricted authoring environment. Native local
qualification is the authority that confirms installation and compiler identity.
