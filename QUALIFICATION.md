# Qualification Status

**Snapshot:** 2026-09-04

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
- globally unique stable IDs and generated traceability;
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

The full local qualifier invokes the current public-path campaigns in dependency order:

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
bash scripts/e2e/global_pose_singleton.sh
bash scripts/e2e/pose_refine.sh
bash scripts/e2e/bundle_problem_build.sh
bash scripts/e2e/bundle_admission_audit.sh
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
| global-pose singleton | identity-bearing empty edge-scale continuity and fixed zero-origin singleton semantics | multi-camera consistency or reconstruction accuracy |
| pose refine | deterministic translation-only robust relaxation, fixed roots, factor dispositions, and do-nothing dominance | rotation or landmark optimization, metric pose, or held-out reprojection improvement |
| bundle-problem build | exact structural camera/landmark support core, bipartite topology, bridges, root reachability, candidate held-out counts, and typed structural decisions | exact image-domain validity, seed independence, numerical conditioning, or optimization |
| bundle-admission audit | exact image-domain binding, half-open coordinate checks, optimize-only seed provenance, surviving seed support, active-camera held-out independence, and recomputed admission | calibration accuracy, reprojection minimization, numerical rank, optimized poses/landmarks, metric scale, or geometry publication |

## Bundle qualification boundary

The structural and audited bundle generations are separate qualification obligations:

```text
fdgr.bundle_problem/1
  → proves structural support and topology

fdgr.bundle_admission/1
  → proves exact image domains, seed partition, and held-out usability
```

A structural `admit` is not enough to invoke a future optimizer. The audit campaign specifically protects against:

- an active or candidate-held-out coordinate outside the exact image bounds;
- a frame or effective-calibration identity substituted beneath an unchanged camera node;
- a held-out observation reused during landmark-seed initialization;
- a seed whose declared support did not survive into the final optimize core;
- candidate held-out evidence from a camera pruned out of that core;
- a stale raw file accepted under a previously computed digest;
- a partial audit emitted after operation-budget exhaustion;
- semantic identity changing merely because a larger successful operation ceiling was supplied.

A positive audit still proves only that the problem is eligible for **bounded optimization evaluation**. It does not prove the optimizer’s result or the physical scene.

## Current evidence boundary

The exact current source contains:

- the 28-member safe-Rust workspace;
- schemas and registries through bundle admission;
- focused unit fixtures for the bundle-admission oracle;
- public exact-byte structural and audit commands;
- a full local qualifier that places the audit after structural compilation;
- a Doodlestein job graph that makes the audit receipt a predecessor of promotion.

Those facts are necessary preconditions, not a retained current-head qualification receipt.

This execution environment does not provide the pinned `cargo`, `rustc`, or `rustfmt` toolchain and therefore cannot honestly establish that the current exact head passes formatting, locked compilation, Clippy, all workspace tests, or every public-path campaign. No such claim is made here.

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

## Doodlestein dependency closure

The local job graph now makes promotion depend on:

```text
repository, registry, dependency, and generated-contract checks
  → complete locked Rust lane
    → agent-contract and agent-scenario lanes
      → keyframe and correspondence evidence
        → epipolar and relative-pose evidence
          → pose graph and edge-scale evidence
            → global-pose and singleton continuity
              → translation refinement
                → structural bundle compilation
                  → bundle-admission audit
                    → promotion identity seal
```

No later job may infer a missing predecessor receipt from source presence or a hosted status.

## `WP-018` qualification boundary

Current reference evidence covers graph topology, orientation composition, relative edge-scale reconciliation, arbitrary-gauge camera-center initialization, translation-only refinement, structural bundle compilation, and image-domain/seed-provenance/held-out auditing. `WP-018` and `GATE-009` remain open because they also require, among other things:

- calibrated reprojection functions and residual-family semantics;
- robust joint rotation, translation, and landmark optimization;
- deterministic factor admission, downweighting, retraction, and branch decisions;
- gauge conditioning, numerical-rank evidence, and convergence/refusal semantics;
- resumable checkpoints and stale-checkpoint handling;
- cancellation, crash, restart, recovery, and indeterminate outcomes;
- agent-readable decision cards and exact continuation;
- scalar/reference versus optimized differential equivalence;
- independently held-out or ground-truth evidence that optimization improves or safely rejects priors;
- retained exact-head local receipts and visible negative evidence.

An audited input problem cannot close those obligations merely because it is a necessary substrate.

## Promotion rule

No partial lane may publish a release root. Successful work may be cached by exact identity, but promotion requires the complete registered predecessor receipt closure. Negative evidence remains visible; it is not averaged into a readiness percentage.
