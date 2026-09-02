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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-relative-pose-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
BEARINGS="$TMP_ROOT/bearings.tsv"
CANDIDATES="$TMP_ROOT/candidates.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
MUTATED_STDOUT="$TMP_ROOT/mutated.stdout"
MUTATED_STDERR="$TMP_ROOT/mutated.stderr"

cat >"$BEARINGS" <<'EOF'
match_id	left_observation_id	right_observation_id	left_bx_nano	left_by_nano	left_bz_nano	right_bx_nano	right_by_nano	right_bz_nano	uncertainty_nano
1	1	101	0	0	1000000000	-600000000	0	800000000	0
2	2	102	0	0	1000000000	-600000000	0	800000000	0
3	3	103	0	0	1000000000	-600000000	0	800000000	0
4	4	104	0	0	1000000000	-600000000	0	800000000	0
EOF
cat >"$CANDIDATES" <<'EOF'
candidate_id	evidence_digest	source	r00	r01	r02	r10	r11	r12	r20	r21	r22	tx	ty	tz
1	0101010101010101010101010101010101010101010101010101010101010101	diagnostic_hypothesis	1000000000	0	0	0	1000000000	0	0	0	1000000000	-1000000000	0	0
2	0202020202020202020202020202020202020202020202020202020202020202	diagnostic_hypothesis	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0
3	0303030303030303030303030303030303030303030303030303030303030303	diagnostic_hypothesis	1000000000	0	0	0	1000000000	0	0	0	1000000000	0	-1000000000	0
EOF

BEARING_DIGEST="$($PYTHON - "$BEARINGS" <<'PY'
import hashlib
from pathlib import Path
import sys
print(hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest())
PY
)"
CANDIDATE_DIGEST="$($PYTHON - "$CANDIDATES" <<'PY'
import hashlib
from pathlib import Path
import sys
print(hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest())
PY
)"
CORRESPONDENCE_DIGEST="1111111111111111111111111111111111111111111111111111111111111111"
CALIBRATION_DIGEST="2222222222222222222222222222222222222222222222222222222222222222"
POLICY_DIGEST="3333333333333333333333333333333333333333333333333333333333333333"

run_verify() {
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    relative-pose-verify "$BEARINGS" "$CANDIDATES" \
    --bearing-basis-digest "$BEARING_DIGEST" \
    --candidate-basis-digest "$CANDIDATE_DIGEST" \
    --correspondence-generation-digest "$CORRESPONDENCE_DIGEST" \
    --calibration-digest "$CALIBRATION_DIGEST" \
    --policy-digest "$POLICY_DIGEST" \
    --left-sample-index 10 \
    --right-sample-index 20 \
    --generation 1 \
    --max-epipolar-residual-nano 1000000 \
    --min-epipolar-normal-nano 10000000 \
    --min-parallax-nano 1000000 \
    --min-inlier-matches 3 \
    --min-inlier-ratio-ppm 800000 \
    --min-positive-depth-ratio-ppm 800000 \
    --max-median-residual-nano 1000000 \
    --require-cheirality true \
    --max-evaluations 100 \
    --format json
}

run_verify >"$FIRST"
run_verify >"$SECOND"
cmp "$FIRST" "$SECOND"

"$PYTHON" - "$FIRST" "$BEARINGS" "$CANDIDATES" "$BEARING_DIGEST" "$CANDIDATE_DIGEST" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as handle:
    document = json.load(handle)
assert document["schema"] == "fdgr.relative_pose_verification/1"
assert document["bearing_basis_digest"] == sys.argv[4]
assert document["candidate_basis_digest"] == sys.argv[5]
assert document["status"] == "geometrically_verified"
assert document["selected_candidate_id"] == 1
assert document["ambiguous_candidate_ids"] == []
assert document["match_count"] == 4
assert document["candidate_count"] == 3
assert document["evaluation_count"] == 12
by_id = {entry["candidate_id"]: entry for entry in document["evaluations"]}
assert by_id[1]["accepted"] is True
assert by_id[1]["inlier_count"] == 4
assert {entry["rejection_reason"] for entry in by_id[2]["assessments"]} == {"cheirality_failed"}
assert {entry["rejection_reason"] for entry in by_id[3]["assessments"]} == {"epipolar_residual_exceeded"}
rendered = json.dumps(document, sort_keys=True)
assert sys.argv[2] not in rendered
assert sys.argv[3] not in rendered
PY

printf '4\t0404040404040404040404040404040404040404040404040404040404040404\tdiagnostic_hypothesis\t1000000000\t0\t0\t0\t1000000000\t0\t0\t0\t1000000000\t-1000000000\t0\t0\n' >>"$CANDIDATES"
set +e
run_verify >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated pose candidates were accepted under the stale basis identity\n' >&2
  exit 1
fi
if ! grep -q 'pose candidate basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale pose-candidate refusal lacked stable public context\n' >&2
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
    verification = json.load(handle)
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "relative_pose_verify_public_path",
    "source_commit": sys.argv[2],
    "cargo_version": sys.argv[3],
    "rustc_version": sys.argv[4],
    "verification_digest": verification["verification_digest"],
    "status": verification["status"],
    "selected_candidate_id": verification["selected_candidate_id"],
    "mutated_basis_exit_code": int(sys.argv[5]),
    "mutated_basis_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
