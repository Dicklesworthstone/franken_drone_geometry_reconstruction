#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CARGO="${CARGO:-cargo}"
PYTHON="${PYTHON:-python3}"
command -v "$CARGO" >/dev/null 2>&1 || {
  printf 'ERROR: cargo is unavailable: %s\n' "$CARGO" >&2
  exit 3
}
command -v "$PYTHON" >/dev/null 2>&1 || {
  printf 'ERROR: python is unavailable: %s\n' "$PYTHON" >&2
  exit 3
}

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-clock-fit-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
ANCHORS="$TMP_ROOT/anchors.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
MUTATED_STDOUT="$TMP_ROOT/mutated.stdout"
MUTATED_STDERR="$TMP_ROOT/mutated.stderr"

cat >"$ANCHORS" <<'EOF'
anchor_id	source_tick	reference_tick	uncertainty_ticks	correlation_group
1	0	500000	100	1
2	1000	1000500000	100	2
3	2000	2000500100	100	3
4	3000	8999500000	100	4
5	4000	4000500000	100	5
EOF

BASIS_DIGEST="$($PYTHON - "$ANCHORS" <<'PY'
import hashlib
from pathlib import Path
import sys
print(hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest())
PY
)"

run_fit() {
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    clock-fit "$ANCHORS" \
    --basis-digest "$BASIS_DIGEST" \
    --source-domain media_pts \
    --reference-domain host_monotonic \
    --source-epoch 1 \
    --reference-epoch 1 \
    --generation 1 \
    --source-timescale 1000 \
    --reference-timescale 1000000000 \
    --max-residual-ticks 1000 \
    --max-drift-ppm 1000 \
    --min-independent-groups 3 \
    --format json
}

run_fit >"$FIRST"
run_fit >"$SECOND"
cmp "$FIRST" "$SECOND"

"$PYTHON" - "$FIRST" "$ANCHORS" "$BASIS_DIGEST" <<'PY'
import json
import sys
from pathlib import Path

with open(sys.argv[1], encoding="utf-8") as handle:
    document = json.load(handle)
assert document["schema"] == "fdgr.clock_model/1"
assert document["basis_digest"] == sys.argv[3]
assert document["source_domain"] == "media_pts"
assert document["reference_domain"] == "host_monotonic"
assert document["source_epoch"] == 1
assert document["reference_epoch"] == 1
assert document["model_generation"] == 1
assert document["rate_numerator"] == "1000000"
assert document["rate_denominator"] == "1"
assert document["offset_numerator"] == "500000"
assert document["drift_ppm"] == 0
assert document["outlier_anchor_ids"] == [4]
assert document["outlier_group_ids"] == [4]
assert document["inlier_group_ids"] == [1, 2, 3, 5]
assert document["source_support_start_ticks"] == "0"
assert document["source_support_end_ticks"] == "4000"
assert sys.argv[2] not in json.dumps(document, sort_keys=True)
PY

printf '6\t5000\t5000500000\t100\t6\n' >>"$ANCHORS"
set +e
run_fit >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated anchor evidence was accepted under the old basis identity\n' >&2
  exit 1
fi
if ! grep -q 'clock anchor basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: basis mismatch refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" \
  "$MUTATED_STATUS" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as handle:
    model = json.load(handle)
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "clock_fit_public_path",
    "source_commit": sys.argv[2],
    "cargo_version": sys.argv[3],
    "rustc_version": sys.argv[4],
    "model_digest": model["model_digest"],
    "basis_digest": model["basis_digest"],
    "inlier_groups": model["inlier_group_ids"],
    "outlier_groups": model["outlier_group_ids"],
    "mutated_basis_exit_code": int(sys.argv[5]),
    "mutated_basis_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
