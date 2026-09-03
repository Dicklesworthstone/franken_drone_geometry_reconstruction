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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-global-pose-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
NODES="$TMP_ROOT/nodes.tsv"
EDGES="$TMP_ROOT/edges.tsv"
WITNESSES="$TMP_ROOT/witnesses.tsv"
CONFLICTING_WITNESSES="$TMP_ROOT/conflicting-witnesses.tsv"
DISCONNECTED_NODES="$TMP_ROOT/disconnected-nodes.tsv"
DISCONNECTED_EDGES="$TMP_ROOT/disconnected-edges.tsv"
EMPTY_WITNESSES="$TMP_ROOT/empty-witnesses.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
LARGER_BUDGET="$TMP_ROOT/larger-budget.json"
CONFLICTING="$TMP_ROOT/conflicting.json"
DISCONNECTED="$TMP_ROOT/disconnected.json"
MUTATED_STDOUT="$TMP_ROOT/mutated.stdout"
MUTATED_STDERR="$TMP_ROOT/mutated.stderr"
BUDGET_STDOUT="$TMP_ROOT/budget.stdout"
BUDGET_STDERR="$TMP_ROOT/budget.stderr"

cat >"$NODES" <<'EOF_NODES'
node_id	sample_index	keyframe_digest
1	10	0101010101010101010101010101010101010101010101010101010101010101
2	20	0202020202020202020202020202020202020202020202020202020202020202
3	30	0303030303030303030303030303030303030303030303030303030303030303
EOF_NODES

cat >"$EDGES" <<'EOF_EDGES'
edge_id	verification_digest	admitted_candidate_id	left_node_id	right_node_id	left_sample_index	right_sample_index	r00	r01	r02	r10	r11	r12	r20	r21	r22	tx	ty	tz	supported_match_count	median_residual_nano
10	1010101010101010101010101010101010101010101010101010101010101010	1	1	2	10	20	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	30	100
20	2020202020202020202020202020202020202020202020202020202020202020	1	2	3	20	30	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	29	100
30	3030303030303030303030303030303030303030303030303030303030303030	1	1	3	10	30	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	10	100
EOF_EDGES

cat >"$WITNESSES" <<'EOF_WITNESSES'
witness_id	evidence_digest	correlation_group_id	lower_edge_id	higher_edge_id	ratio_numerator	ratio_denominator	uncertainty_ppm	support_count	source
1	1111111111111111111111111111111111111111111111111111111111111111	101	10	20	1	1	1000	20	shared_track_geometry
2	1212121212121212121212121212121212121212121212121212121212121212	102	10	20	1	1	1000	18	multi_view_geometry
3	1313131313131313131313131313131313131313131313131313131313131313	103	20	30	1	2	1000	20	shared_track_geometry
4	1414141414141414141414141414141414141414141414141414141414141414	104	20	30	1	2	1000	18	multi_view_geometry
5	1515151515151515151515151515151515151515151515151515151515151515	105	10	30	1	2	2000	5	model_prior
EOF_WITNESSES

cat >"$CONFLICTING_WITNESSES" <<'EOF_CONFLICT'
witness_id	evidence_digest	correlation_group_id	lower_edge_id	higher_edge_id	ratio_numerator	ratio_denominator	uncertainty_ppm	support_count	source
1	1111111111111111111111111111111111111111111111111111111111111111	101	10	20	1	1	1000	20	shared_track_geometry
2	1212121212121212121212121212121212121212121212121212121212121212	102	10	20	1	1	1000	18	multi_view_geometry
3	1313131313131313131313131313131313131313131313131313131313131313	103	20	30	1	1	1000	20	shared_track_geometry
4	1414141414141414141414141414141414141414141414141414141414141414	104	20	30	1	1	1000	18	multi_view_geometry
5	1515151515151515151515151515151515151515151515151515151515151515	105	10	30	1	1	2000	5	model_prior
EOF_CONFLICT

cat >"$DISCONNECTED_NODES" <<'EOF_DISCONNECTED_NODES'
node_id	sample_index	keyframe_digest
1	10	0101010101010101010101010101010101010101010101010101010101010101
2	20	0202020202020202020202020202020202020202020202020202020202020202
10	100	0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a
11	110	0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b
EOF_DISCONNECTED_NODES

cat >"$DISCONNECTED_EDGES" <<'EOF_DISCONNECTED_EDGES'
edge_id	verification_digest	admitted_candidate_id	left_node_id	right_node_id	left_sample_index	right_sample_index	r00	r01	r02	r10	r11	r12	r20	r21	r22	tx	ty	tz	supported_match_count	median_residual_nano
10	1010101010101010101010101010101010101010101010101010101010101010	1	1	2	10	20	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	30	100
20	2020202020202020202020202020202020202020202020202020202020202020	1	10	11	100	110	1000000000	0	0	0	1000000000	0	0	0	1000000000	0	1000000000	0	29	100
EOF_DISCONNECTED_EDGES

cat >"$EMPTY_WITNESSES" <<'EOF_EMPTY_WITNESSES'
witness_id	evidence_digest	correlation_group_id	lower_edge_id	higher_edge_id	ratio_numerator	ratio_denominator	uncertainty_ppm	support_count	source
EOF_EMPTY_WITNESSES

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
WITNESS_DIGEST="$(sha256_file "$WITNESSES")"
CONFLICTING_WITNESS_DIGEST="$(sha256_file "$CONFLICTING_WITNESSES")"
DISCONNECTED_NODE_DIGEST="$(sha256_file "$DISCONNECTED_NODES")"
DISCONNECTED_EDGE_DIGEST="$(sha256_file "$DISCONNECTED_EDGES")"
EMPTY_WITNESS_DIGEST="$(sha256_file "$EMPTY_WITNESSES")"
GRAPH_POLICY_DIGEST="4141414141414141414141414141414141414141414141414141414141414141"
ROTATION_POLICY_DIGEST="4242424242424242424242424242424242424242424242424242424242424242"
SCALE_POLICY_DIGEST="4343434343434343434343434343434343434343434343434343434343434343"
GLOBAL_POLICY_DIGEST="4444444444444444444444444444444444444444444444444444444444444444"

run_global() {
  local nodes="$1"
  local edges="$2"
  local witnesses="$3"
  local node_digest="$4"
  local edge_digest="$5"
  local witness_digest="$6"
  local pose_budget="$7"
  local scale_budget="$8"
  local global_budget="$9"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    global-pose-initialize "$nodes" "$edges" "$witnesses" \
    --node-file-digest "$node_digest" \
    --pose-edge-file-digest "$edge_digest" \
    --scale-witness-file-digest "$witness_digest" \
    --graph-selection-policy-digest "$GRAPH_POLICY_DIGEST" \
    --rotation-policy-digest "$ROTATION_POLICY_DIGEST" \
    --pose-graph-generation 1 \
    --edge-scale-policy-digest "$SCALE_POLICY_DIGEST" \
    --edge-scale-generation 1 \
    --global-pose-policy-digest "$GLOBAL_POLICY_DIGEST" \
    --global-pose-generation 1 \
    --max-rotation-cycle-residual-ppm 5000 \
    --max-orientation-drift-ppm 10000 \
    --max-pose-path-expansions "$pose_budget" \
    --max-within-group-residual-ppm 10000 \
    --max-consensus-residual-ppm 20000 \
    --max-scale-cycle-residual-ppm 50000 \
    --min-cross-validation-groups 2 \
    --max-relative-scale-nano 1000000000000 \
    --max-scale-path-expansions "$scale_budget" \
    --max-translation-cycle-residual-ppm 50000 \
    --max-camera-center-abs-nano 1000000000000000 \
    --max-global-pose-operations "$global_budget" \
    --format json
}

run_global "$NODES" "$EDGES" "$WITNESSES" "$NODE_DIGEST" "$EDGE_DIGEST" "$WITNESS_DIGEST" 1000 1000 1000 >"$FIRST"
run_global "$NODES" "$EDGES" "$WITNESSES" "$NODE_DIGEST" "$EDGE_DIGEST" "$WITNESS_DIGEST" 1000 1000 1000 >"$SECOND"
cmp "$FIRST" "$SECOND"
run_global "$NODES" "$EDGES" "$WITNESSES" "$NODE_DIGEST" "$EDGE_DIGEST" "$WITNESS_DIGEST" 100000 100000 1000000 >"$LARGER_BUDGET"
run_global "$NODES" "$EDGES" "$CONFLICTING_WITNESSES" "$NODE_DIGEST" "$EDGE_DIGEST" "$CONFLICTING_WITNESS_DIGEST" 1000 1000 1000 >"$CONFLICTING"
run_global "$DISCONNECTED_NODES" "$DISCONNECTED_EDGES" "$EMPTY_WITNESSES" "$DISCONNECTED_NODE_DIGEST" "$DISCONNECTED_EDGE_DIGEST" "$EMPTY_WITNESS_DIGEST" 1000 1000 1000 >"$DISCONNECTED"

"$PYTHON" - "$FIRST" "$LARGER_BUDGET" "$CONFLICTING" "$DISCONNECTED" "$NODES" "$EDGES" "$WITNESSES" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

first = load(sys.argv[1])
larger = load(sys.argv[2])
conflicting = load(sys.argv[3])
disconnected = load(sys.argv[4])
assert first["schema"] == "fdgr.global_pose_initialization/1"
assert first["authority"] == "relative_component_gauge"
assert first["unit"] == "component_edge_scale_unit_nano"
assert first["pose_count"] == 3
assert first["translation_cycle_count"] == 1
assert first["component_count"] == 1
assert first["components"][0]["status"] == "cycle_consistent"
assert first["components"][0]["scale_cross_validated"] is True
poses = {entry["node_id"]: entry for entry in first["poses"]}
assert poses[1]["camera_center_from_root_nano"] == [0, 0, 0]
assert poses[2]["camera_center_from_root_nano"] == [-1000000000, 0, 0]
assert poses[3]["camera_center_from_root_nano"] == [-2000000000, 0, 0]
cycle = first["translation_cycles"][0]
assert cycle["status"] == "consistent"
assert cycle["residual_ppm"] == 0
assert cycle["implied_displacement_nano"] == [-2000000000, 0, 0]
assert cycle["measured_displacement_nano"] == [-2000000000, 0, 0]
assert first["initialization_digest"] == larger["initialization_digest"]
assert first["pose_graph_generation_digest"] == larger["pose_graph_generation_digest"]
assert first["edge_scale_generation_digest"] == larger["edge_scale_generation_digest"]
assert first["policy"]["max_operations"] != larger["policy"]["max_operations"]
assert conflicting["components"][0]["status"] == "translation_conflicted"
assert conflicting["components"][0]["scale_cross_validated"] is True
assert conflicting["translation_cycles"][0]["status"] == "conflicting"
assert conflicting["translation_cycles"][0]["residual_ppm"] > 50000
assert disconnected["component_count"] == 2
assert disconnected["translation_cycle_count"] == 0
assert {entry["component_root_node_id"] for entry in disconnected["components"]} == {1, 10}
assert {entry["status"] for entry in disconnected["components"]} == {"tree_initialized"}
assert {entry["scale_component_root_edge_id"] for entry in disconnected["components"]} == {10, 20}
disconnected_poses = {entry["node_id"]: entry for entry in disconnected["poses"]}
assert disconnected_poses[1]["camera_center_from_root_nano"] == [0, 0, 0]
assert disconnected_poses[10]["camera_center_from_root_nano"] == [0, 0, 0]
assert disconnected_poses[2]["camera_center_from_root_nano"] == [-1000000000, 0, 0]
assert disconnected_poses[11]["camera_center_from_root_nano"] == [0, -1000000000, 0]
assert disconnected_poses[1]["scale_component_root_edge_id"] == 10
assert disconnected_poses[10]["scale_component_root_edge_id"] == 20
rendered = json.dumps(first, sort_keys=True)
for path in sys.argv[5:8]:
    assert path not in rendered
for forbidden in ("meter", "bundle_adjust", "trajectory_publication"):
    assert forbidden not in rendered.lower()
PY

printf '6\t1616161616161616161616161616161616161616161616161616161616161616\t106\t10\t20\t1\t1\t1000\t10\texternal_oracle\n' >>"$WITNESSES"
set +e
run_global "$NODES" "$EDGES" "$WITNESSES" "$NODE_DIGEST" "$EDGE_DIGEST" "$WITNESS_DIGEST" 1000 1000 1000 >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated global-pose witnesses were accepted under the stale basis identity\n' >&2
  exit 1
fi
if ! grep -q 'edge-scale witness basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale global-pose witness refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

set +e
run_global "$NODES" "$EDGES" "$CONFLICTING_WITNESSES" "$NODE_DIGEST" "$EDGE_DIGEST" "$CONFLICTING_WITNESS_DIGEST" 1000 1000 1 >"$BUDGET_STDOUT" 2>"$BUDGET_STDERR"
BUDGET_STATUS=$?
set -e
if [[ "$BUDGET_STATUS" -eq 0 ]]; then
  printf 'ERROR: global-pose operation ceiling failed to refuse partial initialization\n' >&2
  exit 1
fi
if ! grep -q 'global-pose initialization attempted operation' "$BUDGET_STDERR"; then
  printf 'ERROR: global-pose budget refusal lacked stable public context\n' >&2
  cat "$BUDGET_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$CONFLICTING" "$DISCONNECTED" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$MUTATED_STATUS" "$BUDGET_STATUS" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

consistent = load(sys.argv[1])
conflicting = load(sys.argv[2])
disconnected = load(sys.argv[3])
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "global_pose_initialize_public_path",
    "source_commit": sys.argv[4],
    "cargo_version": sys.argv[5],
    "rustc_version": sys.argv[6],
    "consistent_initialization_digest": consistent["initialization_digest"],
    "conflicting_initialization_digest": conflicting["initialization_digest"],
    "disconnected_initialization_digest": disconnected["initialization_digest"],
    "relative_authority_preserved": consistent["authority"] == "relative_component_gauge",
    "translation_conflict_preserved": conflicting["components"][0]["status"] == "translation_conflicted",
    "independent_component_gauges_preserved": disconnected["component_count"] == 2,
    "mutated_basis_exit_code": int(sys.argv[7]),
    "mutated_basis_refused": True,
    "budget_exit_code": int(sys.argv[8]),
    "budget_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
