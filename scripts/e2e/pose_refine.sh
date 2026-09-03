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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-pose-refine-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
NODES="$TMP_ROOT/nodes.tsv"
EDGES="$TMP_ROOT/edges.tsv"
CONFLICTING_WITNESSES="$TMP_ROOT/conflicting-witnesses.tsv"
CONSISTENT_WITNESSES="$TMP_ROOT/consistent-witnesses.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
LARGER_BUDGET="$TMP_ROOT/larger-budget.json"
CONSISTENT="$TMP_ROOT/consistent.json"
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

cat >"$CONFLICTING_WITNESSES" <<'EOF_CONFLICTING'
witness_id	evidence_digest	correlation_group_id	lower_edge_id	higher_edge_id	ratio_numerator	ratio_denominator	uncertainty_ppm	support_count	source
1	1111111111111111111111111111111111111111111111111111111111111111	101	10	20	1	1	1000	20	shared_track_geometry
2	1212121212121212121212121212121212121212121212121212121212121212	102	10	20	1	1	1000	18	multi_view_geometry
3	1313131313131313131313131313131313131313131313131313131313131313	103	20	30	1	1	1000	20	shared_track_geometry
4	1414141414141414141414141414141414141414141414141414141414141414	104	20	30	1	1	1000	18	multi_view_geometry
5	1515151515151515151515151515151515151515151515151515151515151515	105	10	30	1	1	2000	5	model_prior
EOF_CONFLICTING

cat >"$CONSISTENT_WITNESSES" <<'EOF_CONSISTENT'
witness_id	evidence_digest	correlation_group_id	lower_edge_id	higher_edge_id	ratio_numerator	ratio_denominator	uncertainty_ppm	support_count	source
1	1111111111111111111111111111111111111111111111111111111111111111	101	10	20	1	1	1000	20	shared_track_geometry
2	1212121212121212121212121212121212121212121212121212121212121212	102	10	20	1	1	1000	18	multi_view_geometry
3	1313131313131313131313131313131313131313131313131313131313131313	103	20	30	1	2	1000	20	shared_track_geometry
4	1414141414141414141414141414141414141414141414141414141414141414	104	20	30	1	2	1000	18	multi_view_geometry
5	1515151515151515151515151515151515151515151515151515151515151515	105	10	30	1	2	2000	5	model_prior
EOF_CONSISTENT

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
CONFLICTING_WITNESS_DIGEST="$(sha256_file "$CONFLICTING_WITNESSES")"
CONSISTENT_WITNESS_DIGEST="$(sha256_file "$CONSISTENT_WITNESSES")"
GRAPH_POLICY_DIGEST="4141414141414141414141414141414141414141414141414141414141414141"
ROTATION_POLICY_DIGEST="4242424242424242424242424242424242424242424242424242424242424242"
SCALE_POLICY_DIGEST="4343434343434343434343434343434343434343434343434343434343434343"
GLOBAL_POLICY_DIGEST="4444444444444444444444444444444444444444444444444444444444444444"
REFINEMENT_POLICY_DIGEST="4545454545454545454545454545454545454545454545454545454545454545"

run_refine() {
  local witnesses="$1"
  local witness_digest="$2"
  local refinement_budget="$3"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    pose-refine "$NODES" "$EDGES" "$witnesses" \
    --node-file-digest "$NODE_DIGEST" \
    --pose-edge-file-digest "$EDGE_DIGEST" \
    --scale-witness-file-digest "$witness_digest" \
    --graph-selection-policy-digest "$GRAPH_POLICY_DIGEST" \
    --rotation-policy-digest "$ROTATION_POLICY_DIGEST" \
    --pose-graph-generation 1 \
    --edge-scale-policy-digest "$SCALE_POLICY_DIGEST" \
    --edge-scale-generation 1 \
    --global-pose-policy-digest "$GLOBAL_POLICY_DIGEST" \
    --global-pose-generation 1 \
    --pose-refinement-policy-digest "$REFINEMENT_POLICY_DIGEST" \
    --pose-refinement-generation 1 \
    --max-rotation-cycle-residual-ppm 5000 \
    --max-orientation-drift-ppm 10000 \
    --max-pose-path-expansions 1000 \
    --max-within-group-residual-ppm 10000 \
    --max-consensus-residual-ppm 20000 \
    --max-scale-cycle-residual-ppm 50000 \
    --min-cross-validation-groups 2 \
    --max-relative-scale-nano 1000000000000 \
    --max-scale-path-expansions 1000 \
    --max-translation-cycle-residual-ppm 50000 \
    --max-camera-center-abs-nano 1000000000000000 \
    --max-global-pose-operations 1000 \
    --max-refinement-iterations 100 \
    --refinement-convergence-delta-nano 100 \
    --refinement-huber-delta-nano 250000000 \
    --refinement-damping-weight 1 \
    --max-refinement-factor-weight 100 \
    --max-refinement-camera-center-abs-nano 1000000000000000 \
    --max-refinement-operations "$refinement_budget" \
    --format json
}

run_refine "$CONFLICTING_WITNESSES" "$CONFLICTING_WITNESS_DIGEST" 100000 >"$FIRST"
run_refine "$CONFLICTING_WITNESSES" "$CONFLICTING_WITNESS_DIGEST" 100000 >"$SECOND"
cmp "$FIRST" "$SECOND"
run_refine "$CONFLICTING_WITNESSES" "$CONFLICTING_WITNESS_DIGEST" 1000000 >"$LARGER_BUDGET"
run_refine "$CONSISTENT_WITNESSES" "$CONSISTENT_WITNESS_DIGEST" 100000 >"$CONSISTENT"

"$PYTHON" - "$FIRST" "$LARGER_BUDGET" "$CONSISTENT" "$NODES" "$EDGES" "$CONFLICTING_WITNESSES" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

refined = load(sys.argv[1])
larger = load(sys.argv[2])
consistent = load(sys.argv[3])
assert refined["schema"] == "fdgr.pose_refinement/1"
assert refined["authority"] == "relative_component_gauge"
assert refined["unit"] == "component_edge_scale_unit_nano"
assert refined["pose_count"] == 3
assert refined["factor_count"] == 3
assert refined["component_count"] == 1
component = refined["components"][0]
assert component["decision"] == "accept_refined"
assert component["reason"] == "objective_improved"
assert component["do_nothing_dominates"] is False
assert component["initial_rms_residual_nano"] > component["final_rms_residual_nano"]
assert component["rms_improvement_nano"] > 0
assert component["downweighted_edge_ids"] == [30]
poses = {entry["node_id"]: entry for entry in refined["poses"]}
assert poses[1]["root_pinned"] is True
assert poses[1]["refined_camera_center_from_root_nano"] == [0, 0, 0]
assert -2000000000 < poses[3]["refined_camera_center_from_root_nano"][0] < -1000000000
factors = {entry["edge_id"]: entry for entry in refined["factors"]}
assert factors[30]["disposition"] == "active_downweighted"
assert factors[30]["robust_weight"] < factors[30]["base_weight"]
assert factors[30]["final_residual_nano"] < factors[30]["initial_residual_nano"]
assert refined["refinement_digest"] == larger["refinement_digest"]
assert refined["initialization_digest"] == larger["initialization_digest"]
assert refined["policy"]["max_operations"] != larger["policy"]["max_operations"]
consistent_component = consistent["components"][0]
assert consistent_component["status"] == "already_satisfied"
assert consistent_component["decision"] == "retain_initialization"
assert consistent_component["reason"] == "already_satisfied"
assert consistent_component["do_nothing_dominates"] is True
assert consistent_component["rms_improvement_nano"] == 0
assert all(entry["adjustment_nano"] == [0, 0, 0] for entry in consistent["poses"])
rendered = json.dumps(refined, sort_keys=True)
for path in sys.argv[4:7]:
    assert path not in rendered
for forbidden in ("meter", "bundle_adjusted", "landmark_optimized", "global_trajectory"):
    assert forbidden not in rendered.lower()
PY

printf '6\t1616161616161616161616161616161616161616161616161616161616161616\t106\t10\t20\t1\t1\t1000\t10\texternal_oracle\n' >>"$CONFLICTING_WITNESSES"
set +e
run_refine "$CONFLICTING_WITNESSES" "$CONFLICTING_WITNESS_DIGEST" 100000 >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated refinement witnesses were accepted under the stale basis identity\n' >&2
  exit 1
fi
if ! grep -q 'edge-scale witness basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale refinement input refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

set +e
run_refine "$CONSISTENT_WITNESSES" "$CONSISTENT_WITNESS_DIGEST" 1 >"$BUDGET_STDOUT" 2>"$BUDGET_STDERR"
BUDGET_STATUS=$?
set -e
if [[ "$BUDGET_STATUS" -eq 0 ]]; then
  printf 'ERROR: refinement operation ceiling failed to refuse partial output\n' >&2
  exit 1
fi
if ! grep -q 'pose refinement attempted operation' "$BUDGET_STDERR"; then
  printf 'ERROR: refinement budget refusal lacked stable public context\n' >&2
  cat "$BUDGET_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$CONSISTENT" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$MUTATED_STATUS" "$BUDGET_STATUS" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

refined = load(sys.argv[1])
consistent = load(sys.argv[2])
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "pose_refine_public_path",
    "source_commit": sys.argv[3],
    "cargo_version": sys.argv[4],
    "rustc_version": sys.argv[5],
    "refined_digest": refined["refinement_digest"],
    "consistent_digest": consistent["refinement_digest"],
    "strict_objective_improvement": refined["components"][0]["decision"] == "accept_refined",
    "do_nothing_preserved": consistent["components"][0]["decision"] == "retain_initialization",
    "component_relative_authority_preserved": refined["authority"] == "relative_component_gauge",
    "mutated_basis_exit_code": int(sys.argv[6]),
    "mutated_basis_refused": True,
    "budget_exit_code": int(sys.argv[7]),
    "budget_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
