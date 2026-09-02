# Qualification Status

**Snapshot:** 2026-09-02

FDGR treats qualification as retained, exact-identity evidence. Source presence, a schema, a unit fixture, a process exit, an E2E script, or a hosted badge is never a blanket implementation or production-readiness claim.

GitHub-hosted Actions are non-authoritative. The repository workflow is only a portable description for local/self-hosted execution. Doodlestein or a direct local invocation must retain the actual source, toolchain, host, command, logs, outputs, and readback identities.

## Repository-owned static lane

```bash
python3 scripts/generate_traceability.py --check
python3 scripts/export_beads_bootstrap.py --output .beads/bootstrap.jsonl --check
python3 scripts/check_dependency_policy.py
python3 scripts/test_registry_contracts.py
python3 scripts/validate_repo.py
python3 scripts/validate_agent_contracts.py
bash -n scripts/*.sh scripts/e2e/*.sh
git diff --check
```

The static lane covers, among other things:

- TOML and JSON parsing;
- globally unique stable IDs and traceability;
- work-package and local-job DAG acyclicity;
- schema and ADR paths;
- closed dependency policy and exact research-source identities;
- forbidden Rust/source patterns;
- workspace and lockfile agreement;
- canonical machine vocabulary;
- Agent Turn Packet and anchor-vector contract structure;
- generated Beads drift;
- local-only release authority.

Static success is not Rust compilation, numeric validation, device compatibility, measured accuracy, recovery, security, or production admission.

## Repository-owned native lane

```bash
cargo fmt --all --check
cargo check --workspace --all-targets --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --all-targets --locked
```

The full local qualifier also invokes the current public-path campaigns:

```bash
bash scripts/e2e/recorded_media_ingest_and_verify.sh
bash scripts/e2e/recorded_media_timeline.sh
bash scripts/e2e/clock_fit.sh
bash scripts/e2e/keyframe_select.sh
bash scripts/e2e/correspondence_build.sh
bash scripts/e2e/relative_pose_verify.sh
```

Each campaign proves only its named fixture and refusal semantics at the exact source and toolchain identities. In particular:

- recorded-media ingest/verify does not prove arbitrary filesystem recovery;
- timeline replay does not prove live clock synchronization;
- clock fitting does not prove telemetry continuity;
- keyframe selection does not prove surface coverage;
- correspondence construction does not prove epipolar correctness;
- relative-pose adjudication does not prove candidate completeness, global pose, metric pose, or reconstruction accuracy.

## Current evidence boundary

The latest correspondence and relative-pose implementation wave was authored in an environment without `cargo`, `rustc`, or `rustfmt`. Consequently, this document does **not** claim that those newest commits passed formatting, compilation, Clippy, unit tests, or public-path E2E in that environment.

Those claims become earned only when a retained local receipt names:

```text
FDGR commit and tree
Cargo.lock digest
pinned rustc/cargo/rustfmt identities
host and target profile
feature closure
input and fixture roots
command lines
exit status and logs
output and receipt digests
```

The authoritative direct command is:

```bash
./scripts/qualify.sh --mode full
```

A release candidate additionally requires a clean checkout, exact production-admitted sibling closure, and a final local identity seal:

```bash
./scripts/qualify.sh --mode release \
  --sibling-root /path/to/exact/checkouts \
  --receipt-out qualification/fdgr-local-qualification.json
```

## Promotion rule

No partial lane may publish a release root. Successful work may be cached by exact identity, but promotion requires the complete registered predecessor receipt closure. Negative evidence remains visible; it is not averaged into a readiness percentage.
