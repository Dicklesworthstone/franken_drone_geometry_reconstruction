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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-bundle-admission-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
NODES="$TMP_ROOT/nodes.tsv"
EDGES="$TMP_ROOT/edges.tsv"
WITNESSES="$TMP_ROOT/witnesses.tsv"
CAMERAS="$TMP_ROOT/cameras.tsv"
LANDMARKS="$TMP_ROOT/landmarks.tsv"
OBSERVATIONS="$TMP_ROOT/observations.tsv"
INACTIVE_OBSERVATIONS="$TMP_ROOT/inactive-observations.tsv"
DOMAINS="$TMP_ROOT/domains.tsv"
SMALL_DOMAIN="$TMP_ROOT/small-domain.tsv"
REORDERED_DOMAINS="$TMP_ROOT/reordered-domains.tsv"
PROVENANCE="$TMP_ROOT/provenance.tsv"
INACTIVE_PROVENANCE="$TMP_ROOT/inactive-provenance.tsv"
REORDERED_PROVENANCE="$TMP_ROOT/reordered-provenance.tsv"
LEAKED_PROVENANCE="$TMP_ROOT/leaked-provenance.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
REORDERED="$TMP_ROOT/reordered.json"
LARGER_BUDGET="$TMP_ROOT/larger-budget.json"
INACTIVE="$TMP_ROOT/inactive.json"
OUT_OF_DOMAIN="$TMP_ROOT/out-of-domain.json"
LEAK_STDOUT="$TMP_ROOT/leak.stdout"
LEAK_STDERR="$TMP_ROOT/leak.stderr"
BUDGET_STDOUT="$TMP_ROOT/budget.stdout"
BUDGET_STDERR="$TMP_ROOT/budget.stderr"
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
3	100	3	30	7373737373737373737373737373737373737373737373737373737373737373	1003	9393939393939393939393939393939393939393939393939393939393939393	102000000000	80000000000	100000	false	optimize
4	200	1	10	7171717171717171717171717171717171717171717171717171717171717171	1004	9494949494949494949494949494949494949494949494949494949494949494	110000000000	80000000000	100000	false	optimize
5	200	2	20	7272727272727272727272727272727272727272727272727272727272727272	1005	9595959595959595959595959595959595959595959595959595959595959595	111000000000	80000000000	100000	false	optimize
6	200	3	30	7373737373737373737373737373737373737373737373737373737373737373	1006	9696969696969696969696969696969696969696969696969696969696969696	112000000000	80000000000	100000	false	optimize
7	300	1	10	7171717171717171717171717171717171717171717171717171717171717171	1007	9797979797979797979797979797979797979797979797979797979797979797	120000000000	80000000000	100000	false	optimize
8	300	2	20	7272727272727272727272727272727272727272727272727272727272727272	1008	9898989898989898989898989898989898989898989898989898989898989898	121000000000	80000000000	100000	false	optimize
9	300	3	30	7373737373737373737373737373737373737373737373737373737373737373	1009	9999999999999999999999999999999999999999999999999999999999999999	122000000000	80000000000	100000	false	held_out
EOF_OBSERVATIONS

cat >"$INACTIVE_OBSERVATIONS" <<'EOF_INACTIVE'
observation_id	landmark_id	camera_node_id	sample_index	frame_digest	source_feature_observation_id	evidence_digest	x_nano_pixels	y_nano_pixels	localization_uncertainty_nano_pixels	dynamic_masked	role
1	100	1	10	7171717171717171717171717171717171717171717171717171717171717171	1001	9191919191919191919191919191919191919191919191919191919191919191	100000000000	80000000000	100000	false	optimize
2	100	2	20	7272727272727272727272727272727272727272727272727272727272727272	1002	9292929292929292929292929292929292929292929292929292929292929292	101000000000	80000000000	100000	false	optimize
3	200	1	10	7171717171717171717171717171717171717171717171717171717171717171	1003	9393939393939393939393939393939393939393939393939393939393939393	110000000000	80000000000	100000	false	optimize
4	200	2	20	7272727272727272727272727272727272727272727272727272727272727272	1004	9494949494949494949494949494949494949494949494949494949494949494	111000000000	80000000000	100000	false	optimize
5	300	1	10	7171717171717171717171717171717171717171717171717171717171717171	1005	9595959595959595959595959595959595959595959595959595959595959595	120000000000	80000000000	100000	false	optimize
6	300	2	20	7272727272727272727272727272727272727272727272727272727272727272	1006	9696969696969696969696969696969696969696969696969696969696969696	121000000000	80000000000	100000	false	optimize
7	100	3	30	7373737373737373737373737373737373737373737373737373737373737373	1007	9797979797979797979797979797979797979797979797979797979797979797	102000000000	80000000000	100000	false	held_out
8	200	3	30	7373737373737373737373737373737373737373737373737373737373737373	1008	9898989898989898989898989898989898989898989898989898989898989898	112000000000	80000000000	100000	false	held_out
EOF_INACTIVE

cat >"$DOMAINS" <<'EOF_DOMAINS'
camera_node_id	frame_digest	effective_calibration_digest	image_width	image_height
1	7171717171717171717171717171717171717171717171717171717171717171	8181818181818181818181818181818181818181818181818181818181818181	640	480
2	7272727272727272727272727272727272727272727272727272727272727272	8282828282828282828282828282828282828282828282828282828282828282	640	480
3	7373737373737373737373737373737373737373737373737373737373737373	8383838383838383838383838383838383838383838383838383838383838383	640	480
EOF_DOMAINS

cat >"$SMALL_DOMAIN" <<'EOF_SMALL_DOMAIN'
camera_node_id	frame_digest	effective_calibration_digest	image_width	image_height
1	7171717171717171717171717171717171717171717171717171717171717171	8181818181818181818181818181818181818181818181818181818181818181	640	480
2	7272727272727272727272727272727272727272727272727272727272727272	8282828282828282828282828282828282828282828282828282828282828282	50	480
3	7373737373737373737373737373737373737373737373737373737373737373	8383838383838383838383838383838383838383838383838383838383838383	640	480
EOF_SMALL_DOMAIN

"$PYTHON" - "$DOMAINS" "$REORDERED_DOMAINS" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
Path(sys.argv[2]).write_text(
    "\n".join([source[0], *reversed(source[1:])]) + "\n",
    encoding="utf-8",
)
PY

cat >"$PROVENANCE" <<'EOF_PROVENANCE'
landmark_id	support_observation_ids
100	1,2
200	4,5
300	7,8
EOF_PROVENANCE

cat >"$INACTIVE_PROVENANCE" <<'EOF_INACTIVE_PROVENANCE'
landmark_id	support_observation_ids
100	1,2
200	3,4
300	5,6
EOF_INACTIVE_PROVENANCE

cat >"$REORDERED_PROVENANCE" <<'EOF_REORDERED_PROVENANCE'
landmark_id	support_observation_ids
300	8,7
200	5,4
100	2,1
EOF_REORDERED_PROVENANCE

cat >"$LEAKED_PROVENANCE" <<'EOF_LEAKED_PROVENANCE'
landmark_id	support_observation_ids
100	1,2
200	4,5
300	7,9
EOF_LEAKED_PROVENANCE

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
INACTIVE_OBSERVATION_DIGEST="$(sha256_file "$INACTIVE_OBSERVATIONS")"
DOMAIN_DIGEST="$(sha256_file "$DOMAINS")"
SMALL_DOMAIN_DIGEST="$(sha256_file "$SMALL_DOMAIN")"
REORDERED_DOMAIN_DIGEST="$(sha256_file "$REORDERED_DOMAINS")"
PROVENANCE_DIGEST="$(sha256_file "$PROVENANCE")"
INACTIVE_PROVENANCE_DIGEST="$(sha256_file "$INACTIVE_PROVENANCE")"
REORDERED_PROVENANCE_DIGEST="$(sha256_file "$REORDERED_PROVENANCE")"
LEAKED_PROVENANCE_DIGEST="$(sha256_file "$LEAKED_PROVENANCE")"
GRAPH_POLICY_DIGEST="4141414141414141414141414141414141414141414141414141414141414141"
ROTATION_POLICY_DIGEST="4242424242424242424242424242424242424242424242424242424242424242"
SCALE_POLICY_DIGEST="4343434343434343434343434343434343434343434343434343434343434343"
GLOBAL_POLICY_DIGEST="4444444444444444444444444444444444444444444444444444444444444444"
REFINEMENT_POLICY_DIGEST="4545454545454545454545454545454545454545454545454545454545454545"

run_audit() {
  local observation_path="$1"
  local observation_digest="$2"
  local domain_path="$3"
  local domain_digest="$4"
  local provenance_path="$5"
  local provenance_digest="$6"
  local min_held_out="$7"
  local audit_operations="$8"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    bundle-admission-audit "$NODES" "$EDGES" "$WITNESSES" "$CAMERAS" "$LANDMARKS" "$observation_path" "$domain_path" "$provenance_path" \
    --node-file-digest "$NODE_DIGEST" \
    --pose-edge-file-digest "$EDGE_DIGEST" \
    --scale-witness-file-digest "$WITNESS_DIGEST" \
    --camera-binding-file-digest "$CAMERA_DIGEST" \
    --landmark-seed-file-digest "$LANDMARK_DIGEST" \
    --bundle-observation-file-digest "$observation_digest" \
    --camera-domain-file-digest "$domain_digest" \
    --seed-provenance-file-digest "$provenance_digest" \
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
    --bundle-admission-generation 1 \
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
    --min-root-active-landmarks 2 \
    --max-bundle-observation-uncertainty-nano-pixels 1000000 \
    --min-held-out-observations-per-component "$min_held_out" \
    --min-held-out-cameras-per-component 1 \
    --max-bundle-graph-path-expansions 1000 \
    --max-bundle-operations 100000 \
    --max-seed-uncertainty-nano 1000000 \
    --min-seed-support-observations 2 \
    --min-seed-support-cameras 2 \
    --require-active-held-out-camera true \
    --max-bundle-admission-operations "$audit_operations" \
    --format json
}

run_audit "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$PROVENANCE" "$PROVENANCE_DIGEST" 1 100000 >"$FIRST"
run_audit "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$PROVENANCE" "$PROVENANCE_DIGEST" 1 100000 >"$SECOND"
cmp "$FIRST" "$SECOND"
run_audit "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$REORDERED_DOMAINS" "$REORDERED_DOMAIN_DIGEST" "$REORDERED_PROVENANCE" "$REORDERED_PROVENANCE_DIGEST" 1 100000 >"$REORDERED"
run_audit "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$PROVENANCE" "$PROVENANCE_DIGEST" 1 10000000 >"$LARGER_BUDGET"
run_audit "$INACTIVE_OBSERVATIONS" "$INACTIVE_OBSERVATION_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$INACTIVE_PROVENANCE" "$INACTIVE_PROVENANCE_DIGEST" 2 100000 >"$INACTIVE"
run_audit "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$SMALL_DOMAIN" "$SMALL_DOMAIN_DIGEST" "$PROVENANCE" "$PROVENANCE_DIGEST" 1 100000 >"$OUT_OF_DOMAIN"

"$PYTHON" - "$FIRST" "$REORDERED" "$LARGER_BUDGET" "$INACTIVE" "$OUT_OF_DOMAIN" "$NODES" "$EDGES" "$WITNESSES" "$CAMERAS" "$LANDMARKS" "$OBSERVATIONS" "$DOMAINS" "$PROVENANCE" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

value = load(sys.argv[1])
reordered = load(sys.argv[2])
larger = load(sys.argv[3])
inactive = load(sys.argv[4])
out_of_domain = load(sys.argv[5])
assert value["schema"] == "fdgr.bundle_admission/1"
assert value["authority"] == "audited_relative_bundle_problem"
assert value["camera_domain_count"] == 3
assert value["seed_provenance_count"] == 3
assert value["observation_audit_count"] == 9
assert value["landmark_audit_count"] == 3
assert value["component_count"] == 1
component = value["components"][0]
assert component["status"] == "admitted"
assert component["decision"] == "admit"
assert component["recommendation"] == "proceed_to_bounded_bundle_optimization"
assert component["invalid_image_observation_ids"] == []
assert component["unproven_seed_landmark_ids"] == []
assert component["inactive_held_out_observation_ids"] == []
assert component["independent_held_out_observation_ids"] == [9]
assert component["independent_held_out_camera_node_ids"] == [3]
assert all(landmark["provenance_proven"] for landmark in value["landmarks"])
assert value["bundle_admission_digest"] == reordered["bundle_admission_digest"]
assert value["camera_domain_basis_digest"] == reordered["camera_domain_basis_digest"]
assert value["seed_provenance_basis_digest"] == reordered["seed_provenance_basis_digest"]
assert value["bundle_admission_digest"] == larger["bundle_admission_digest"]
assert value["policy_digest"] == larger["policy_digest"]
assert value["policy"]["max_operations"] != larger["policy"]["max_operations"]
inactive_component = inactive["components"][0]
assert inactive["authority"] == "bundle_admission_evidence_only"
assert inactive_component["status"] == "insufficient_independent_held_out"
assert inactive_component["decision"] == "admit_diagnostic"
assert inactive_component["inactive_held_out_observation_ids"] == [7, 8]
assert inactive_component["independent_held_out_observation_ids"] == []
out_component = out_of_domain["components"][0]
assert out_of_domain["authority"] == "bundle_admission_evidence_only"
assert out_component["status"] == "invalid_image_domain"
assert out_component["decision"] == "block"
assert 2 in out_component["invalid_image_observation_ids"]
rendered = json.dumps(value, sort_keys=True)
for path in sys.argv[6:]:
    assert path not in rendered
for forbidden in ("metric_scale", "bundle_adjusted", "optimized_landmark", "sparse_geometry"):
    assert forbidden not in rendered.lower()
PY

set +e
run_audit "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$LEAKED_PROVENANCE" "$LEAKED_PROVENANCE_DIGEST" 1 100000 >"$LEAK_STDOUT" 2>"$LEAK_STDERR"
LEAK_STATUS=$?
set -e
if [[ "$LEAK_STATUS" -eq 0 ]]; then
  printf 'ERROR: held-out observation was accepted as seed provenance\n' >&2
  exit 1
fi
if ! grep -q 'uses held-out observation' "$LEAK_STDERR"; then
  printf 'ERROR: held-out seed-provenance refusal lacked stable public context\n' >&2
  cat "$LEAK_STDERR" >&2
  exit 1
fi

set +e
run_audit "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$PROVENANCE" "$PROVENANCE_DIGEST" 1 1 >"$BUDGET_STDOUT" 2>"$BUDGET_STDERR"
BUDGET_STATUS=$?
set -e
if [[ "$BUDGET_STATUS" -eq 0 ]]; then
  printf 'ERROR: bundle-admission operation ceiling failed to refuse partial output\n' >&2
  exit 1
fi
if ! grep -q 'bundle-admission audit attempted operation' "$BUDGET_STDERR"; then
  printf 'ERROR: bundle-admission budget refusal lacked stable public context\n' >&2
  cat "$BUDGET_STDERR" >&2
  exit 1
fi

printf '4\t7474747474747474747474747474747474747474747474747474747474747474\t8484848484848484848484848484848484848484848484848484848484848484\t640\t480\n' >>"$DOMAINS"
set +e
run_audit "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$PROVENANCE" "$PROVENANCE_DIGEST" 1 100000 >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated camera domains were accepted under a stale file identity\n' >&2
  exit 1
fi
if ! grep -q 'bundle camera-domain basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale camera-domain refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$INACTIVE" "$OUT_OF_DOMAIN" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$LEAK_STATUS" "$BUDGET_STATUS" "$MUTATED_STATUS" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

admitted = load(sys.argv[1])
inactive = load(sys.argv[2])
out_of_domain = load(sys.argv[3])
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "bundle_admission_audit_public_path",
    "source_commit": sys.argv[4],
    "cargo_version": sys.argv[5],
    "rustc_version": sys.argv[6],
    "bundle_admission_digest": admitted["bundle_admission_digest"],
    "complete_problem_admitted": admitted["components"][0]["decision"] == "admit",
    "positive_authority_only_when_admitted": admitted["authority"] == "audited_relative_bundle_problem" and inactive["authority"] == "bundle_admission_evidence_only" and out_of_domain["authority"] == "bundle_admission_evidence_only",
    "inactive_held_out_demoted": inactive["components"][0]["status"] == "insufficient_independent_held_out",
    "out_of_domain_blocked": out_of_domain["components"][0]["decision"] == "block",
    "held_out_seed_leak_exit_code": int(sys.argv[7]),
    "held_out_seed_leak_refused": True,
    "budget_exit_code": int(sys.argv[8]),
    "budget_refused": True,
    "mutated_basis_exit_code": int(sys.argv[9]),
    "mutated_basis_refused": True,
    "canonical_row_order_preserved": True,
    "execution_ceiling_nonsemantic": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
