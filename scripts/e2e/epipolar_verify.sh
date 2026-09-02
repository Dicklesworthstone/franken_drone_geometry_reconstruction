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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-epipolar-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
OBSERVATIONS="$TMP_ROOT/observations.tsv"
CANDIDATES="$TMP_ROOT/candidates.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
AMBIGUOUS_OBSERVATIONS="$TMP_ROOT/ambiguous-observations.tsv"
AMBIGUOUS="$TMP_ROOT/ambiguous.json"
MUTATED_STDOUT="$TMP_ROOT/mutated.stdout"
MUTATED_STDERR="$TMP_ROOT/mutated.stderr"

"$PYTHON" - "$OBSERVATIONS" "$AMBIGUOUS_OBSERVATIONS" <<'PY'
from pathlib import Path
import sys
header = "match_id\tpair_id\tleft_observation_id\tright_observation_id\tleft_x_ppm\tleft_y_ppm\tright_x_ppm\tright_y_ppm\tuncertainty_ppm\tleft_spatial_bin\tright_spatial_bin\n"
def rows(parallax):
    output = [header]
    for index in range(12):
        x = index * 50000 - 250000
        y = (index % 4) * 80000 - 120000
        spatial_bin = index % 4 + 1
        output.append(
            f"{index + 1}\t7\t{index + 101}\t{index + 201}\t{x}\t{y}\t{x + parallax}\t{y}\t100\t{spatial_bin}\t{spatial_bin}\n"
        )
    return "".join(output)
Path(sys.argv[1]).write_text(rows(100000), encoding="utf-8")
Path(sys.argv[2]).write_text(rows(0), encoding="utf-8")
PY

cat >"$CANDIDATES" <<'EOF'
candidate_id	source	m00	m01	m02	m10	m11	m12	m20	m21	m22
1	native_minimal_solver	0	0	0	0	0	-5	0	5	0
2	telemetry_prior	0	0	1	0	0	0	-1	0	0
EOF

sha256_file() {
  "$PYTHON" - "$1" <<'PY'
import hashlib
from pathlib import Path
import sys
print(hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest())
PY
}

OBSERVATION_DIGEST="$(sha256_file "$OBSERVATIONS")"
AMBIGUOUS_OBSERVATION_DIGEST="$(sha256_file "$AMBIGUOUS_OBSERVATIONS")"
CANDIDATE_DIGEST="$(sha256_file "$CANDIDATES")"
CORRESPONDENCE_DIGEST="1111111111111111111111111111111111111111111111111111111111111111"
CALIBRATION_DIGEST="2222222222222222222222222222222222222222222222222222222222222222"
POLICY_DIGEST="3333333333333333333333333333333333333333333333333333333333333333"

run_verification() {
  local observations="$1"
  local observation_digest="$2"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    epipolar-verify "$observations" "$CANDIDATES" \
    --observation-basis-digest "$observation_digest" \
    --candidate-basis-digest "$CANDIDATE_DIGEST" \
    --correspondence-generation-digest "$CORRESPONDENCE_DIGEST" \
    --calibration-digest "$CALIBRATION_DIGEST" \
    --policy-digest "$POLICY_DIGEST" \
    --pair-id 7 \
    --left-sample-index 10 \
    --right-sample-index 20 \
    --generation 1 \
    --max-residual-ppm 1000 \
    --min-inliers 8 \
    --min-inlier-ratio-ppm 700000 \
    --min-spatial-bins-per-image 4 \
    --max-determinant-residual-ppm 1000 \
    --min-inlier-margin 2 \
    --max-evaluations 1000 \
    --format json
}

run_verification "$OBSERVATIONS" "$OBSERVATION_DIGEST" >"$FIRST"
run_verification "$OBSERVATIONS" "$OBSERVATION_DIGEST" >"$SECOND"
cmp "$FIRST" "$SECOND"
run_verification "$AMBIGUOUS_OBSERVATIONS" "$AMBIGUOUS_OBSERVATION_DIGEST" >"$AMBIGUOUS"

"$PYTHON" - "$FIRST" "$AMBIGUOUS" "$OBSERVATIONS" "$CANDIDATES" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as handle:
    document = json.load(handle)
with open(sys.argv[2], encoding="utf-8") as handle:
    ambiguous = json.load(handle)
assert document["schema"] == "fdgr.epipolar_verification/1"
assert document["decision"] == "epipolar_supported_hypothesis"
assert document["best_candidate_id"] == 1
assert document["admitted_candidate_id"] == 1
assert document["runner_up_candidate_id"] is None
assert document["observation_count"] == 12
assert document["candidate_count"] == 2
assert document["evaluation_count"] == 24
assert document["candidates"][0]["matrix"] == [0, 0, 0, 0, 0, 1, 0, -1, 0]
evaluations = {entry["candidate_id"]: entry for entry in document["evaluations"]}
assert evaluations[1]["passes_admission_gates"] is True
assert evaluations[1]["inlier_count"] == 12
assert evaluations[1]["inlier_ratio_ppm"] == 1000000
assert evaluations[2]["passes_admission_gates"] is False
assert ambiguous["decision"] == "ambiguous"
assert ambiguous["best_candidate_id"] == 1
assert ambiguous["runner_up_candidate_id"] == 2
assert ambiguous["admitted_candidate_id"] is None
rendered = json.dumps(document, sort_keys=True)
assert sys.argv[3] not in rendered
assert sys.argv[4] not in rendered
assert "pose_authority" not in rendered
PY

printf '13\t7\t113\t213\t350000\t-120000\t450000\t-120000\t100\t1\t1\n' >>"$OBSERVATIONS"
set +e
run_verification "$OBSERVATIONS" "$OBSERVATION_DIGEST" >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated epipolar observations were accepted under the stale basis identity\n' >&2
  exit 1
fi
if ! grep -q 'epipolar observation basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale epipolar basis refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$AMBIGUOUS" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$MUTATED_STATUS" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as handle:
    admitted = json.load(handle)
with open(sys.argv[2], encoding="utf-8") as handle:
    ambiguous = json.load(handle)
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "epipolar_verify_public_path",
    "source_commit": sys.argv[3],
    "cargo_version": sys.argv[4],
    "rustc_version": sys.argv[5],
    "admitted_verification_digest": admitted["verification_digest"],
    "admitted_candidate_id": admitted["admitted_candidate_id"],
    "ambiguous_verification_digest": ambiguous["verification_digest"],
    "ambiguous_admitted_candidate_id": ambiguous["admitted_candidate_id"],
    "mutated_basis_exit_code": int(sys.argv[6]),
    "mutated_basis_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
