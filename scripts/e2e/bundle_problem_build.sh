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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-bundle-problem-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
NODES="$TMP_ROOT/nodes.tsv"
EDGES="$TMP_ROOT/edges.tsv"
WITNESSES="$TMP_ROOT/witnesses.tsv"
CAMERAS="$TMP_ROOT/cameras.tsv"
LANDMARKS="$TMP_ROOT/landmarks.tsv"
OBSERVATIONS="$TMP_ROOT/observations.tsv"
REORDERED_OBSERVATIONS="$TMP_ROOT/reordered-observations.tsv"
MISSING_HELD_OUT="$TMP_ROOT/missing-held-out.tsv"
ROOT_UNOBSERVED="$TMP_ROOT/root-unobserved.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
REORDERED="$TMP_ROOT/reordered.json"
LARGER_BUDGET="$TMP_ROOT/larger-budget.json"
MISSING="$TMP_ROOT/missing.json"
ROOT_BLOCKED="$TMP_ROOT/root-blocked.json"
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

cat >"$CAMERAS" <<'EOF_CAMERAS'
camera_node_id	sample_index	frame_digest	effective_calibration_digest
1	10	7171717171717171717171717171717171717171717171717171717171717171	8181818181818181818181818181818181818181818181818181818181818181
2	20	7272727272727272727272727272727272727272727272727272727272727272	8282828282828282828282828282828282828282828282828282828282828282
3	30	7373737373737373737373737373737373737373737373737373737373737373	8383838383838383838383838383838383838383838383838383838383838383
EOF_CAMERAS

cat >"$LANDMARKS" <<'EOF_LANDMARKS'
landmark_id	source_track_id	component_root_node_id	scale_component_root_edge_id	seed_evidence_digest	seed_x_nano	seed_y_nano	seed_z_nano	seed_uncertainty_nano
100	500	1	10	a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1	0	0	5000000000	100000
200	600	1	10	a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2	1000000000	0	5000000000	100000
300	700	1	10	a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3a3	2000000000	0	5000000000	100000
EOF_LANDMARKS

cat >"$OBSERVATIONS" <<'EOF_OBSERVATIONS'
observation_id	landmark_id	camera_node_id	sample_index	frame_digest	source_feature_observation_id	evidence_digest	x_nano_pixels	y_nano_pixels	localization_uncertainty_nano_pixels	dynamic_masked	role
1	100	1	10	7171717171717171717171717171717171717171717171717171717171717171	1001	9191919191919191919191919191919191919191919191919191919191919191	100000000000	80000000000	100000	false	optimize
2	100	2	20	7272727272727272727272727272727272727272727272727272727272727272	1002	9292929292929292929292929292929292929292929292929292929292929292	101000000000	80000000000	100000	false	optimize
3	200	1	10	7171717171717171717171717171717171717171717171717171717171717171	1003	9393939393939393939393939393939393939393939393939393939393939393	110000000000	80000000000	100000	false	optimize
4	200	2	20	7272727272727272727272727272727272727272727272727272727272727272	1004	9494949494949494949494949494949494949494949494949494949494949494	111000000000	80000000000	100000	false	optimize
5	300	1	10	7171717171717171717171717171717171717171717171717171717171717171	1005	9595959595959595959595959595959595959595959595959595959595959595	120000000000	80000000000	100000	false	optimize
6	300	2	20	7272727272727272727272727272727272727272727272727272727272727272	1006	9696969696969696969696969696969696969696969696969696969696969696	121000000000	80000000000	100000	false	optimize
7	100	3	30	7373737373737373737373737373737373737373737373737373737373737373	1007	9797979797979797979797979797979797979797979797979797979797979797	102000000000	80000000000	100000	false	held_out
8	200	3	30	7373737373737373737373737373737373737373737373737373737373737373	1008	9898989898989898989898989898989898989898989898989898989898989898	112000000000	80000000000	100000	false	held_out
9	300	3	30	7373737373737373737373737373737373737373737373737373737373737373	1009	9999999999999999999999999999999999999999999999999999999999999999	122000000000	80000000000	100000	false	held_out
EOF_OBSERVATIONS

{
  head -n 1 "$OBSERVATIONS"
  tail -n +2 "$OBSERVATIONS" | tac
} >"$REORDERED_OBSERVATIONS"
head -n 7 "$OBSERVATIONS" >"$MISSING_HELD_OUT"
cat >"$ROOT_UNOBSERVED" <<'EOF_ROOT_UNOBSERVED'
observation_id	landmark_id	camera_node_id	sample_index	frame_digest	source_feature_observation_id	evidence_digest	x_nano_pixels	y_nano_pixels	localization_uncertainty_nano_pixels	dynamic_masked	role
11	100	2	20	7272727272727272727272727272727272727272727272727272727272727272	1011	6161616161616161616161616161616161616161616161616161616161616161	101000000000	80000000000	100000	false	optimize
12	100	3	30	7373737373737373737373737373737373737373737373737373737373737373	1012	6262626262626262626262626262626262626262626262626262626262626262	102000000000	80000000000	100000	false	optimize
13	200	2	20	7272727272727272727272727272727272727272727272727272727272727272	1013	6363636363636363636363636363636363636363636363636363636363636363	111000000000	80000000000	100000	false	optimize
14	200	3	30	7373737373737373737373737373737373737373737373737373737373737373	1014	6464646464646464646464646464646464646464646464646464646464646464	112000000000	80000000000	100000	false	optimize
EOF_ROOT_UNOBSERVED

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
CAMERA_DIGEST="$(sha256_file "$CAMERAS")"
LANDMARK_DIGEST="$(sha256_file "$LANDMARKS")"
OBSERVATION_DIGEST="$(sha256_file "$OBSERVATIONS")"
REORDERED_DIGEST="$(sha256_file "$REORDERED_OBSERVATIONS")"
MISSING_DIGEST="$(sha256_file "$MISSING_HELD_OUT")"
ROOT_UNOBSERVED_DIGEST="$(sha256_file "$ROOT_UNOBSERVED")"
GRAPH_POLICY_DIGEST="4141414141414141414141414141414141414141414141414141414141414141"
ROTATION_POLICY_DIGEST="4242424242424242424242424242424242424242424242424242424242424242"
SCALE_POLICY_DIGEST="4343434343434343434343434343434343434343434343434343434343434343"
GLOBAL_POLICY_DIGEST="4444444444444444444444444444444444444444444444444444444444444444"
REFINEMENT_POLICY_DIGEST="4545454545454545454545454545454545454545454545454545454545454545"

run_bundle() {
  local observation_path="$1"
  local observation_digest="$2"
  local min_root_landmarks="$3"
  local min_held_out_observations="$4"
  local graph_budget="$5"
  local operation_budget="$6"
  local camera_digest="${7:-$CAMERA_DIGEST}"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    bundle-problem-build "$NODES" "$EDGES" "$WITNESSES" "$CAMERAS" "$LANDMARKS" "$observation_path" \
    --node-file-digest "$NODE_DIGEST" \
    --pose-edge-file-digest "$EDGE_DIGEST" \
    --scale-witness-file-digest "$WITNESS_DIGEST" \
    --camera-binding-file-digest "$camera_digest" \
    --landmark-seed-file-digest "$LANDMARK_DIGEST" \
    --bundle-observation-file-digest "$observation_digest" \
    --graph-selection-policy-digest "$GRAPH_POLICY_DIGEST" \
    --rotation-policy-digest "$ROTATION_POLICY_DIGEST" \
    --pose-graph-generation 1 \
    --edge-scale-policy-digest "$SCALE_POLICY_DIGEST" \
    --edge-scale-generation 1 \
    --global-pose-policy-digest "$GLOBAL_POLICY_DIGEST" \
    --global-pose-generation 1 \
    --pose-refinement-policy-digest "$REFINEMENT_POLICY_DIGEST" \
    --pose-refinement-generation 1 \
    --bundle-problem-generation 1 \
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
    --max-refinement-operations 100000 \
    --min-optimize-cameras-per-landmark 2 \
    --min-active-landmarks-per-camera 2 \
    --min-root-active-landmarks "$min_root_landmarks" \
    --max-bundle-observation-uncertainty-nano-pixels 1000000 \
    --min-held-out-observations-per-component "$min_held_out_observations" \
    --min-held-out-cameras-per-component 1 \
    --max-bundle-graph-path-expansions "$graph_budget" \
    --max-bundle-operations "$operation_budget" \
    --format json
}

run_bundle "$OBSERVATIONS" "$OBSERVATION_DIGEST" 2 2 1000 100000 >"$FIRST"
run_bundle "$OBSERVATIONS" "$OBSERVATION_DIGEST" 2 2 1000 100000 >"$SECOND"
cmp "$FIRST" "$SECOND"
run_bundle "$REORDERED_OBSERVATIONS" "$REORDERED_DIGEST" 2 2 1000 100000 >"$REORDERED"
run_bundle "$OBSERVATIONS" "$OBSERVATION_DIGEST" 2 2 1000000 1000000 >"$LARGER_BUDGET"
run_bundle "$MISSING_HELD_OUT" "$MISSING_DIGEST" 2 2 1000 100000 >"$MISSING"
run_bundle "$ROOT_UNOBSERVED" "$ROOT_UNOBSERVED_DIGEST" 1 1 1000 100000 >"$ROOT_BLOCKED"

"$PYTHON" - "$FIRST" "$REORDERED" "$LARGER_BUDGET" "$MISSING" "$ROOT_BLOCKED" "$NODES" "$EDGES" "$WITNESSES" "$CAMERAS" "$LANDMARKS" "$OBSERVATIONS" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

value = load(sys.argv[1])
reordered = load(sys.argv[2])
larger = load(sys.argv[3])
missing = load(sys.argv[4])
root_blocked = load(sys.argv[5])
assert value["schema"] == "fdgr.bundle_problem/1"
assert value["authority"] == "admitted_relative_bundle_problem"
assert value["camera_count"] == 3
assert value["landmark_count"] == 3
assert value["observation_count"] == 9
assert value["graph_node_count"] == 5
assert value["graph_edge_count"] == 6
assert value["graph_cycle_rank"] == 2
assert value["component_count"] == 1
component = value["components"][0]
assert component["status"] == "redundant"
assert component["decision"] == "admit"
assert component["recommendation"] == "proceed_to_bounded_bundle_optimization"
assert component["root_connected"] is True
assert component["root_active_landmark_count"] == 3
assert component["cycle_rank"] == 2
assert component["bridge_observation_ids"] == []
assert component["eligible_held_out_observation_ids"] == [7, 8, 9]
assert component["eligible_held_out_camera_node_ids"] == [3]
assert component["rank_authority"] == "planning_count_only"
cameras = {camera["camera_node_id"]: camera for camera in value["cameras"]}
assert cameras[1]["disposition"] == "active_gauge_root"
assert cameras[2]["disposition"] == "active_training"
assert cameras[3]["disposition"] == "pruned_insufficient_landmark_support"
landmarks = {landmark["landmark_id"]: landmark for landmark in value["landmarks"]}
assert all(landmark["disposition"] == "active_training" for landmark in landmarks.values())
assert all(landmark["held_out_camera_node_ids"] == [3] for landmark in landmarks.values())
observations = {observation["observation_id"]: observation for observation in value["observations"]}
assert all(observations[index]["disposition"] == "active_optimize" for index in range(1, 7))
assert all(observations[index]["disposition"] == "eligible_held_out" for index in range(7, 10))
assert value["bundle_problem_digest"] == reordered["bundle_problem_digest"]
assert value["observation_basis_digest"] == reordered["observation_basis_digest"]
assert value["bundle_problem_digest"] == larger["bundle_problem_digest"]
assert value["policy_digest"] == larger["policy_digest"]
assert value["policy"]["max_operations"] != larger["policy"]["max_operations"]
assert value["policy"]["max_graph_path_expansions"] != larger["policy"]["max_graph_path_expansions"]
assert missing["components"][0]["status"] == "missing_held_out_evidence"
assert missing["components"][0]["decision"] == "admit_diagnostic"
assert missing["components"][0]["recommendation"] == "reserve_independent_held_out_views"
assert root_blocked["components"][0]["status"] == "gauge_root_unobserved"
assert root_blocked["components"][0]["decision"] == "block"
assert root_blocked["components"][0]["recommendation"] == "observe_gauge_root_neighborhood"
rendered = json.dumps(value, sort_keys=True)
for path in sys.argv[6:]:
    assert path not in rendered
for forbidden in ("metric_scale", "bundle_adjusted", "optimized_landmark", "sparse_geometry"):
    assert forbidden not in rendered.lower()
PY

set +e
run_bundle "$OBSERVATIONS" "$OBSERVATION_DIGEST" 2 2 1000 1 >"$BUDGET_STDOUT" 2>"$BUDGET_STDERR"
BUDGET_STATUS=$?
set -e
if [[ "$BUDGET_STATUS" -eq 0 ]]; then
  printf 'ERROR: bundle compiler operation ceiling failed to refuse partial output\n' >&2
  exit 1
fi
if ! grep -q 'bundle-problem compilation attempted operation' "$BUDGET_STDERR"; then
  printf 'ERROR: bundle compiler budget refusal lacked stable public context\n' >&2
  cat "$BUDGET_STDERR" >&2
  exit 1
fi

printf '4\t40\t7474747474747474747474747474747474747474747474747474747474747474\t8484848484848484848484848484848484848484848484848484848484848484\n' >>"$CAMERAS"
set +e
run_bundle "$OBSERVATIONS" "$OBSERVATION_DIGEST" 2 2 1000 100000 "$CAMERA_DIGEST" >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated camera bindings were accepted under a stale file identity\n' >&2
  exit 1
fi
if ! grep -q 'bundle camera-binding basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale bundle input refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$MISSING" "$ROOT_BLOCKED" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$MUTATED_STATUS" "$BUDGET_STATUS" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

admitted = load(sys.argv[1])
missing = load(sys.argv[2])
blocked = load(sys.argv[3])
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "bundle_problem_build_public_path",
    "source_commit": sys.argv[4],
    "cargo_version": sys.argv[5],
    "rustc_version": sys.argv[6],
    "bundle_problem_digest": admitted["bundle_problem_digest"],
    "redundant_problem_admitted": admitted["components"][0]["decision"] == "admit",
    "missing_held_out_is_diagnostic": missing["components"][0]["decision"] == "admit_diagnostic",
    "unobserved_root_blocked": blocked["components"][0]["decision"] == "block",
    "canonical_row_order_preserved": True,
    "execution_ceilings_nonsemantic": True,
    "mutated_basis_exit_code": int(sys.argv[7]),
    "mutated_basis_refused": True,
    "budget_exit_code": int(sys.argv[8]),
    "budget_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
