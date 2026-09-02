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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-keyframe-select-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
CANDIDATES="$TMP_ROOT/candidates.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
MUTATED_STDOUT="$TMP_ROOT/mutated.stdout"
MUTATED_STDERR="$TMP_ROOT/mutated.stderr"

cat >"$CANDIDATES" <<'EOF'
sample_index	frame_digest	presentation_tick	sharpness_ppm	texture_ppm	dark_clipped_ppm	bright_clipped_ppm	dynamic_content_ppm	overlap_ppm	view_sector	baseline_bin	coverage_cells
0	0101010101010101010101010101010101010101010101010101010101010101	0	800000	700000	10000	20000	30000	600000	1	1	1,2
1	0202020202020202020202020202020202020202020202020202020202020202	1000	800000	700000	10000	20000	30000	600000	2	2	2,3
2	0303030303030303030303030303030303030303030303030303030303030303	2000	800000	700000	10000	20000	30000	600000	1	1	1,2
3	0404040404040404040404040404040404040404040404040404040404040404	3000	10000	700000	10000	20000	30000	600000	3	3	4
4	0505050505050505050505050505050505050505050505050505050505050505	4000	800000	700000	10000	20000	30000	600000	4	4	5
EOF

CANDIDATE_DIGEST="$($PYTHON - "$CANDIDATES" <<'PY'
import hashlib
from pathlib import Path
import sys
print(hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest())
PY
)"
TIMELINE_DIGEST="1111111111111111111111111111111111111111111111111111111111111111"
DECODED_DIGEST="2222222222222222222222222222222222222222222222222222222222222222"
CALIBRATION_DIGEST="3333333333333333333333333333333333333333333333333333333333333333"
POLICY_DIGEST="4444444444444444444444444444444444444444444444444444444444444444"

run_selection() {
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    keyframe-select "$CANDIDATES" \
    --candidate-basis-digest "$CANDIDATE_DIGEST" \
    --timeline-digest "$TIMELINE_DIGEST" \
    --decoded-frame-generation-digest "$DECODED_DIGEST" \
    --calibration-digest "$CALIBRATION_DIGEST" \
    --policy-digest "$POLICY_DIGEST" \
    --generation 1 \
    --max-selected 2 \
    --min-sharpness-ppm 200000 \
    --min-texture-ppm 100000 \
    --max-dark-clipped-ppm 200000 \
    --max-bright-clipped-ppm 200000 \
    --max-dynamic-content-ppm 300000 \
    --min-overlap-ppm 100000 \
    --format json
}

run_selection >"$FIRST"
run_selection >"$SECOND"
cmp "$FIRST" "$SECOND"

"$PYTHON" - "$FIRST" "$CANDIDATES" "$CANDIDATE_DIGEST" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    document = json.load(handle)
assert document["schema"] == "fdgr.keyframe_selection/1"
assert document["candidate_basis_digest"] == sys.argv[3]
assert document["candidate_count"] == 5
assert document["eligible_candidate_count"] == 4
assert document["selected_count"] == 2
assert document["rejected_count"] == 3
assert document["covered_cells"] == [1, 2, 3]
assert [entry["sample_index"] for entry in document["selected"]] == [0, 1]
reasons = {entry["sample_index"]: entry["reason"] for entry in document["rejected"]}
assert reasons == {
    2: "redundant_evidence",
    3: "sharpness_below_minimum",
    4: "capacity",
}
assert next(entry for entry in document["rejected"] if entry["sample_index"] == 3)["marginal_coverage_cells"] == 1
assert sys.argv[2] not in json.dumps(document, sort_keys=True)
PY

printf '5\t0606060606060606060606060606060606060606060606060606060606060606\t5000\t800000\t700000\t10000\t20000\t30000\t600000\t5\t5\t6\n' >>"$CANDIDATES"
set +e
run_selection >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated keyframe evidence was accepted under the stale basis identity\n' >&2
  exit 1
fi
if ! grep -q 'keyframe candidate basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale keyframe basis refusal lacked stable public context\n' >&2
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
    selection = json.load(handle)
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "keyframe_select_public_path",
    "source_commit": sys.argv[2],
    "cargo_version": sys.argv[3],
    "rustc_version": sys.argv[4],
    "selection_digest": selection["selection_digest"],
    "candidate_basis_digest": selection["candidate_basis_digest"],
    "selected_samples": [entry["sample_index"] for entry in selection["selected"]],
    "rejection_reasons": {
        str(entry["sample_index"]): entry["reason"] for entry in selection["rejected"]
    },
    "mutated_basis_exit_code": int(sys.argv[5]),
    "mutated_basis_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
