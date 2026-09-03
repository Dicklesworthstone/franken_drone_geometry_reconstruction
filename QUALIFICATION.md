# Qualification Status

**Snapshot:** 2026-09-03

FDGR treats qualification as retained exact-identity evidence. Source presence, a schema, a unit fixture, a process exit, an E2E script, a queued workflow, or a hosted badge is never a blanket implementation or production-readiness claim.

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

The full local qualifier invokes the current public-path campaigns:

```bash
bash scripts/e2e/recorded_media_ingest_and_verify.sh
bash scripts/e2e/recorded_media_timeline.sh
bash scripts/e2e/clock_fit.sh
bash scripts/e2e/keyframe_select.sh
bash scripts/e2e/correspondence_build.sh
bash scripts/e2e/epipolar_verify.sh
bash scripts/e2e/relative_pose_verify.sh
bash scripts/e2e/pose_graph_build.sh
bash scripts/e2e/edge_scale_resolve.sh
bash scripts/e2e/global_pose_initialize.sh
```

Each campaign proves only its named fixture and refusal semantics at exact source, toolchain, host, and policy identities.

| Campaign | Evidence it can establish | Evidence it cannot establish |
|---|---|---|
| recorded-media ingest/verify | exact local publication and independent root closure for the fixture | arbitrary filesystem crash recovery or cloud restore |
| recorded-media timeline | exact bounded DTS/PTS/duration/byte-span semantics and explicit partial coverage | live telemetry synchronization |
| clock fit | robust affine mapping, exact support interval, outlier evidence, and no extrapolation | source continuity outside the admitted epochs |
| keyframe selection | deterministic quality/diversity selection and rejection evidence | geometric or semantic surface coverage |
| correspondence build | deterministic descriptor hypotheses and collision-safe tracks | epipolar correctness or physical motion |
| epipolar verify | exact essential-matrix proposal residual/inlier adjudication | rotation, translation, or pose authority |
| relative-pose verify | fixed-point physical-candidate parallax/cheirality adjudication | candidate completeness, baseline magnitude, or global pose |
| pose-graph build | deterministic graph topology, component-local orientations, and rotation-cycle status | camera centers or synchronized translation scale |
| edge-scale resolve | correlation-aware relative baseline gauges and cycle evidence | meters or comparability across disconnected gauges |
| global-pose initialize | deterministic component-relative camera centers, parent-edge provenance, and translation-cycle status | bundle adjustment, landmarks, metric pose, covariance, or trajectory publication |

## Current evidence boundary

The exact current head contains source, schemas, focused unit fixtures, and public-path scripts through component-relative global-pose initialization. The `fdgr-cli` lock closure was repaired after adding the global-pose dependency. Those facts are necessary preconditions, not a retained qualification receipt.

At this snapshot, the self-hosted workflow for the current head is queued and has not produced a terminal result. That state is explicitly **not** success. The repository therefore does not claim that the current exact head passed formatting, locked compilation, Clippy, all workspace tests, or every public-path campaign.

Those claims become earned only when one retained receipt binds:

```text
FDGR commit and tree
Cargo.lock digest
pinned rustc, cargo, and rustfmt identities
host, target, and hardware profile
feature and dependency closure
input and fixture roots
all command lines and ordered predecessor receipts
exit status and complete bounded logs
output and semantic receipt digests
negative cases, omissions, and unsupported dimensions
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

## `WP-018` qualification boundary

Current reference evidence covers graph topology, orientation composition, relative edge-scale reconciliation, and arbitrary-gauge camera-center initialization. `WP-018` and `GATE-009` remain open because they also require, among other things:

- robust nonlinear pose/landmark objective and residual semantics;
- deterministic outlier and loop-closure branch decisions;
- gauge conditioning and convergence/refusal evidence;
- resumable checkpoints and stale-checkpoint handling;
- cancellation, crash, restart, recovery, and indeterminate outcomes;
- agent-readable decision cards and exact continuation;
- scalar/reference versus optimized differential equivalence;
- held-out or ground-truth evidence that refinement improves or safely rejects priors;
- retained current local receipts and visible negative evidence.

Component-relative initialization cannot close those obligations merely because it is a useful substrate.

## Promotion rule

No partial lane may publish a release root. Successful work may be cached by exact identity, but promotion requires the complete registered predecessor receipt closure. Negative evidence remains visible; it is not averaged into a readiness percentage.
