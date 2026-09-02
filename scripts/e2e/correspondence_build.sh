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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-correspondence-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
OBSERVATIONS="$TMP_ROOT/observations.tsv"
PAIRS="$TMP_ROOT/pairs.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
MUTATED_STDOUT="$TMP_ROOT/mutated.stdout"
MUTATED_STDERR="$TMP_ROOT/mutated.stderr"

cat >"$OBSERVATIONS" <<'EOF'
observation_id	sample_index	frame_digest	feature_id	x_nano_pixels	y_nano_pixels	response_ppm	uncertainty_nano_pixels	descriptor_hex	dynamic_masked
1	0	0101010101010101010101010101010101010101010101010101010101010101	1	0	0	800000	0	0000000000000000000000000000000000000000000000000000000000000000	false
2	1	0202020202020202020202020202020202020202020202020202020202020202	1	0	0	800000	0	0000000000000000000000000000000000000000000000000000000000000000	false
3	2	0303030303030303030303030303030303030303030303030303030303030303	1	0	0	800000	0	0000000000000000000000000000000000000000000000000000000000000000	false
EOF
cat >"$PAIRS" <<'EOF'
pair_id	left_sample_index	right_sample_index
1	0	1
2	1	2
EOF

FEATURE_DIGEST="$($PYTHON - "$OBSERVATIONS" <<'PY'
import hashlib
from pathlib import Path
import sys
print(hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest())
PY
)"
PAIR_DIGEST="$($PYTHON - "$PAIRS" <<'PY'
import hashlib
from pathlib import Path
import sys
print(hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest())
PY
)"
KEYFRAME_DIGEST="1111111111111111111111111111111111111111111111111111111111111111"
CALIBRATION_DIGEST="2222222222222222222222222222222222222222222222222222222222222222"
POLICY_DIGEST="3333333333333333333333333333333333333333333333333333333333333333"

run_build() {
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    correspondence-build "$OBSERVATIONS" "$PAIRS" \
    --feature-basis-digest "$FEATURE_DIGEST" \
    --pair-basis-digest "$PAIR_DIGEST" \
    --keyframe-selection-digest "$KEYFRAME_DIGEST" \
    --calibration-digest "$CALIBRATION_DIGEST" \
    --policy-digest "$POLICY_DIGEST" \
    --generation 1 \
    --max-hamming-distance 0 \
    --ratio-threshold-ppm 800000 \
    --require-second-best false \
    --require-mutual true \
    --min-response-ppm 100000 \
    --max-uncertainty-nano-pixels 1 \
    --reject-dynamic-masked true \
    --max-distance-evaluations 100 \
    --format json
}

run_build >"$FIRST"
run_build >"$SECOND"
cmp "$FIRST" "$SECOND"

"$PYTHON" - "$FIRST" "$OBSERVATIONS" "$PAIRS" "$FEATURE_DIGEST" "$PAIR_DIGEST" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as handle:
    document = json.load(handle)
assert document["schema"] == "fdgr.correspondence_generation/1"
assert document["feature_basis_digest"] == sys.argv[4]
assert document["pair_basis_digest"] == sys.argv[5]
assert document["observation_count"] == 3
assert document["pair_count"] == 2
assert document["accepted_match_count"] == 2
assert document["rejected_match_count"] == 0
assert document["track_count"] == 1
assert document["distance_evaluations"] == 4
assert document["tracks"] == [{
    "track_id": 1,
    "edge_count": 2,
    "observation_ids": [1, 2, 3],
    "sample_indices": [0, 1, 2],
    "supporting_pair_ids": [1, 2],
}]
rendered = json.dumps(document, sort_keys=True)
assert sys.argv[2] not in rendered
assert sys.argv[3] not in rendered
PY

printf '4\t3\t0404040404040404040404040404040404040404040404040404040404040404\t1\t0\t0\t800000\t0\t0000000000000000000000000000000000000000000000000000000000000000\tfalse\n' >>"$OBSERVATIONS"
set +e
run_build >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated feature evidence was accepted under the stale basis identity\n' >&2
  exit 1
fi
if ! grep -q 'feature observation basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale correspondence basis refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$MUTATED_STATUS" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as handle:
    generation = json.load(handle)
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "correspondence_build_public_path",
    "source_commit": sys.argv[2],
    "cargo_version": sys.argv[3],
    "rustc_version": sys.argv[4],
    "generation_digest": generation["generation_digest"],
    "accepted_match_count": generation["accepted_match_count"],
    "track_count": generation["track_count"],
    "mutated_basis_exit_code": int(sys.argv[5]),
    "mutated_basis_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
