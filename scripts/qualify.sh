#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: ./scripts/qualify.sh [--mode static|full|release] [--sibling-root PATH] [--receipt-out PATH]

static   Validate design, registries, schemas, mutation controls, generated artifacts, scripts,
         and diff hygiene.
full     Run static checks plus the pinned Rust format/check/Clippy/test lanes and deterministic
         public-path integration campaigns (default).
release  Run full checks, require a clean checkout, verify production-admitted sibling pins, and
         seal exact source/contract/toolchain/host identities. Doodlestein predecessor receipts
         remain the authority for step success.
EOF
}

MODE=full
SIBLING_ROOT=
RECEIPT_OUT=qualification/fdgr-local-qualification.json
while (($#)); do
  case "$1" in
    --mode) MODE="${2:?missing value for --mode}"; shift 2 ;;
    --sibling-root) SIBLING_ROOT="${2:?missing value for --sibling-root}"; shift 2 ;;
    --receipt-out) RECEIPT_OUT="${2:?missing value for --receipt-out}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) printf 'ERROR: unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
  esac
done
case "$MODE" in static|full|release) ;; *) printf 'ERROR: invalid mode: %s\n' "$MODE" >&2; exit 2 ;; esac

if [[ -t 1 ]]; then
  BLUE='\033[1;34m'; GREEN='\033[1;32m'; RESET='\033[0m'
else
  BLUE=''; GREEN=''; RESET=''
fi
step() { printf '%b==> %s%b\n' "$BLUE" "$1" "$RESET"; }
pass() { printf '%bPASS: %s%b\n' "$GREEN" "$1" "$RESET"; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export PYTHONDONTWRITEBYTECODE=1

step 'Checking generated traceability and Beads bootstrap'
python3 scripts/generate_traceability.py --check
python3 scripts/export_beads_bootstrap.py --output .beads/bootstrap.jsonl --check
step 'Validating closed dependency universe'
python3 scripts/check_dependency_policy.py
step 'Exercising registry schema, typed-reference, and authority mutation controls'
python3 scripts/test_registry_contracts.py
step 'Validating repository and agent contracts'
python3 scripts/validate_repo.py
python3 scripts/validate_agent_contracts.py
step 'Compiling Python sources without creating repository artifacts'
python3 - <<'PY'
from pathlib import Path
for path in sorted(Path('scripts').glob('*.py')):
    compile(path.read_text(encoding='utf-8'), str(path), 'exec')
print('PASS: Python sources compile')
PY
step 'Checking shell syntax and diff hygiene'
for script in scripts/*.sh scripts/e2e/*.sh; do bash -n "$script"; done
git diff --check
pass 'Static FDGR contract qualification completed'

if [[ "$MODE" == static ]]; then
  exit 0
fi

CARGO="${CARGO:-cargo}"
RUSTC="${RUSTC:-rustc}"
for tool in "$CARGO" "$RUSTC"; do
  command -v "$tool" >/dev/null 2>&1 || { printf 'ERROR: required tool is unavailable: %s\n' "$tool" >&2; exit 3; }
done
step 'Recording the selected Rust toolchain'
"$RUSTC" --version --verbose
"$CARGO" --version --verbose
step 'Checking Rust formatting'
"$CARGO" fmt --all --check
step 'Checking the complete workspace'
"$CARGO" check --workspace --all-targets --locked
step 'Running Clippy with warnings denied'
"$CARGO" clippy --workspace --all-targets --locked -- -D warnings
step 'Running workspace tests'
"$CARGO" test --workspace --all-targets --locked
step 'Running recorded-media ingest and independent verification E2E'
bash scripts/e2e/recorded_media_ingest_and_verify.sh
step 'Running canonical recorded-media timeline E2E'
bash scripts/e2e/recorded_media_timeline.sh
step 'Running robust clock-fit E2E'
bash scripts/e2e/clock_fit.sh
step 'Running deterministic keyframe-selection E2E'
bash scripts/e2e/keyframe_select.sh
step 'Running descriptor correspondence and track-assembly E2E'
bash scripts/e2e/correspondence_build.sh
step 'Running calibrated epipolar-verification E2E'
bash scripts/e2e/epipolar_verify.sh
step 'Running physical relative-pose verification E2E'
bash scripts/e2e/relative_pose_verify.sh
step 'Running pose-graph topology and rotation-cycle E2E'
bash scripts/e2e/pose_graph_build.sh
step 'Running relative edge-scale consensus and cycle E2E'
bash scripts/e2e/edge_scale_resolve.sh
step 'Running component-relative global-pose initialization E2E'
bash scripts/e2e/global_pose_initialize.sh
pass 'Full native FDGR qualification completed'

if [[ "$MODE" == full ]]; then
  exit 0
fi

[[ -n "$SIBLING_ROOT" ]] || { printf 'ERROR: --sibling-root is required in release mode\n' >&2; exit 2; }
step 'Requiring a clean exact release checkout'
[[ -z "$(git status --porcelain=v1 --untracked-files=all)" ]] || { printf 'ERROR: release mode requires a clean Git tree\n' >&2; exit 4; }
step 'Verifying production-admitted sibling source closure'
python3 scripts/verify_source_closure.py --sibling-root "$SIBLING_ROOT"
step 'Sealing source, contract, closure, toolchain, and host identities'
python3 scripts/emit_local_qualification_receipt.py --out "$RECEIPT_OUT"
pass 'Release-candidate identity sealed; Doodlestein must retain all predecessor receipts'
