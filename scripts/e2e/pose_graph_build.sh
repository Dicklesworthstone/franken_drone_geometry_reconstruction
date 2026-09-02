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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-pose-graph-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
NODES="$TMP_ROOT/nodes.tsv"
EDGES="$TMP_ROOT/edges.tsv"
CONFLICTING_EDGES="$TMP_ROOT/conflicting-edges.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
LARGER_BUDGET="$TMP_ROOT/larger-budget.json"
CONFLICTING="$TMP_ROOT/conflicting.json"
MUTATED_STDOUT="$TMP_ROOT/mutated.stdout"
MUTATED_STDERR="$TMP_ROOT/mutated.stderr"

cat >"$NODES" <<'EOF_NODES'
node_id	sample_index	keyframe_digest
1	10	0101010101010101010101010101010101010101010101010101010101010101
2	20	0202020202020202020202020202020202020202020202020202020202020202
3	30	0303030303030303030303030303030303030303030303030303030303030303
EOF_NODES

cat >"$EDGES" <<'EOF_EDGES'
edge_id	verification_digest	admitted_candidate_id	left_node_id	right_node_id	left_sample_index	right_sample_index	r00	r01	r02	r10	r11	r12	r20	r21	r22	tx	ty	tz	supported_match_count	median_residual_nano
10	1010101010101010101010101010101010101010101010101010101010101010	1	1	2	10	20	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	30	100
20	20202020202020202020202020202020202020202020202020202020202020	1	2	3	20	30	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	29	100
30	3030303030303030303030303030303030303030303030303030303030303030	1	1	3	10	30	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	10	100
EOF_EDGES

cat >"$CONFLICTING_EDGES" <<'EOF_CONFLICT'
edge_id	verification_digest	admitted_candidate_id	left_node_id	right_node_id	left_sample_index	right_sample_index	r00	r01	r02	r10	r11	r12	r20	r21	r22	tx	ty	tz	supported_match_count	median_residual_nano
10	1010101010101010101010101010101010101010101010101010101010101010	1	1	2	10	20	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	30	100
20	20202020202020202020202020202020202020202020202020202020202020	1	2	3	20	30	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	29	100
30	3030303030303030303030303030303030303030303030303030303030303030	1	1	3	10	30	-1000000000	0	0	0	-1000000000	0	0	1000000000	1000000000	0	0	10	100
EOF_CONFLICT

sha256_file() {
  "$PYTHON" - "$1" <<'PY'
import hashlib
from pathlib import Path
import sys
print(hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest())
PY
}

NODE_DIGEST="$(sha256_file "$NODES")"
EDGE_DIGEST="$(sha256_file "$EDGES")"
CONFLICTING_EDGE_DIGEST="$(sha256_file "$CONFLICTING_EDGES")"
GRAPH_POLICY_DIGEST="4141414141414141414141414141414141414141414141414141414141414141"
ROTATION_POLICY_DIGEST="4242424242424242424242424242424242424242424242424242424242424242"

run_graph() {
  local edge_path="$1"
  local edge_digest="$2"
  local path_budget="$3"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    pose-graph-build "$NODES" "$edge_path" \
    --node-file-digest "$NODE_DIGEST" \
    --edge-file-digest "$edge_digest" \
    --graph-selection-policy-digest "$GRAPH_POLICY_DIGEST" \
    --rotation-policy-digest "$ROTATION_POLICY_DIGEST" \
    --generation 1 \
    --max-rotation-cycle-residual-ppm 5000 \
    --max-orientation-drift-ppm 10000 \
    --max-path-expansions "$path_budget" \
    --format json
}

run_graph "$EDGES" "$EDGE_DIGEST" 1000 >"$FIRST"
run_graph "$EDGES" "$EDGE_DIGEST" 1000 >"$SECOND"
cmp "$FIRST" "$SECOND"
run_graph "$EDGES" "$EDGE_DIGEST" 100000 >"$LARGER_BUDGET"
run_graph "$CONFLICTING_EDGES" "$CONFLICTING_EDGE_DIGEST" 1000 >"$CONFLICTING"

"$PYTHON" - "$FIRST" "$LARGER_BUDGET" "$CONFLICTING" "$NODES" "$EDGES" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

first = load(sys.argv[1])
larger = load(sys.argv[2])
conflicting = load(sys.argv[3])
assert first["schema"] == "fdgr.pose_graph_generation/1"
assert first["node_count"] == 3
assert first["edge_count"] == 3
assert first["component_count"] == 1
assert first["cycle_count"] == 1
assert first["forest_edge_ids"] == [10, 20]
assert first["bridge_edge_ids"] == []
assert first["translation_status"] == "direction_only_scale_underdetermined"
assert first["components"][0]["orientation_status"] == "cycle_consistent"
assert first["cycle_assessments"][0]["closing_edge_id"] == 30
assert first["cycle_assessments"][0]["status"] == "consistent"
assert first["generation_digest"] == larger["generation_digest"]
assert first["policy"]["max_path_expansions"] != larger["policy"]["max_path_expansions"]
assert conflicting["components"][0]["orientation_status"] == "conflicted"
assert conflicting["cycle_assessments"][0]["status"] == "conflicting"
rendered = json.dumps(first, sort_keys=True)
assert sys.argv[4] not in rendered
assert sys.argv[5] not in rendered
assert "camera_center" not in rendered
assert "position" not in rendered
PY

printf '4\t40\t0404040404040404040404040404040404040404040404040404040404040404\n' >>"$NODES"
set +e
run_graph "$EDGES" "$EDGE_DIGEST" 1000 >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated pose-graph nodes were accepted under the stale basis identity\n' >&2
  exit 1
fi
if ! grep -q 'pose-graph node basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale pose-graph basis refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$CONFLICTING" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$MUTATED_STATUS" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

consistent = load(sys.argv[1])
conflicting = load(sys.argv[2])
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "pose_graph_build_public_path",
    "source_commit": sys.argv[3],
    "cargo_version": sys.argv[4],
    "rustc_version": sys.argv[5],
    "consistent_generation_digest": consistent["generation_digest"],
    "conflicting_generation_digest": conflicting["generation_digest"],
    "forest_edge_ids": consistent["forest_edge_ids"],
    "cycle_conflict_preserved": conflicting["cycle_assessments"][0]["status"] == "conflicting",
    "mutated_basis_exit_code": int(sys.argv[6]),
    "mutated_basis_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
