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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-edge-scale-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
NODES="$TMP_ROOT/nodes.tsv"
EDGES="$TMP_ROOT/edges.tsv"
WITNESSES="$TMP_ROOT/witnesses.tsv"
CONFLICTING_WITNESSES="$TMP_ROOT/conflicting-witnesses.tsv"
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
20	2020202020202020202020202020202020202020202020202020202020202020	1	2	3	20	30	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	29	100
30	3030303030303030303030303030303030303030303030303030303030303030	1	1	3	10	30	1000000000	0	0	0	1000000000	0	0	0	1000000000	1000000000	0	0	10	100
EOF_EDGES

cat >"$WITNESSES" <<'EOF_WITNESSES'
witness_id	evidence_digest	correlation_group_id	lower_edge_id	higher_edge_id	ratio_numerator	ratio_denominator	uncertainty_ppm	support_count	source
1	1111111111111111111111111111111111111111111111111111111111111111	101	10	20	1	2	1000	20	shared_track_geometry
2	1212121212121212121212121212121212121212121212121212121212121212	102	10	20	1	2	1000	18	multi_view_geometry
3	1313131313131313131313131313131313131313131313131313131313131313	103	20	30	2	1	1000	20	shared_track_geometry
4	1414141414141414141414141414141414141414141414141414141414141414	104	20	30	2	1	1000	18	multi_view_geometry
5	1515151515151515151515151515151515151515151515151515151515151515	105	10	30	1	1	2000	5	model_prior
EOF_WITNESSES

cat >"$CONFLICTING_WITNESSES" <<'EOF_CONFLICT'
witness_id	evidence_digest	correlation_group_id	lower_edge_id	higher_edge_id	ratio_numerator	ratio_denominator	uncertainty_ppm	support_count	source
1	1111111111111111111111111111111111111111111111111111111111111111	101	10	20	1	2	1000	20	shared_track_geometry
2	1212121212121212121212121212121212121212121212121212121212121212	102	10	20	1	2	1000	18	multi_view_geometry
3	1313131313131313131313131313131313131313131313131313131313131313	103	20	30	2	1	1000	20	shared_track_geometry
4	1414141414141414141414141414141414141414141414141414141414141414	104	20	30	2	1	1000	18	multi_view_geometry
5	1515151515151515151515151515151515151515151515151515151515151515	105	10	30	2	1	2000	5	model_prior
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
WITNESS_DIGEST="$(sha256_file "$WITNESSES")"
CONFLICTING_WITNESS_DIGEST="$(sha256_file "$CONFLICTING_WITNESSES")"
GRAPH_POLICY_DIGEST="4141414141414141414141414141414141414141414141414141414141414141"
ROTATION_POLICY_DIGEST="4242424242424242424242424242424242424242424242424242424242424242"
SCALE_POLICY_DIGEST="4343434343434343434343434343434343434343434343434343434343434343"

run_scale() {
  local witness_path="$1"
  local witness_digest="$2"
  local pose_budget="$3"
  local scale_budget="$4"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    edge-scale-resolve "$NODES" "$EDGES" "$witness_path" \
    --node-file-digest "$NODE_DIGEST" \
    --pose-edge-file-digest "$EDGE_DIGEST" \
    --scale-witness-file-digest "$witness_digest" \
    --graph-selection-policy-digest "$GRAPH_POLICY_DIGEST" \
    --rotation-policy-digest "$ROTATION_POLICY_DIGEST" \
    --pose-graph-generation 1 \
    --edge-scale-policy-digest "$SCALE_POLICY_DIGEST" \
    --edge-scale-generation 1 \
    --max-rotation-cycle-residual-ppm 5000 \
    --max-orientation-drift-ppm 10000 \
    --max-pose-path-expansions "$pose_budget" \
    --max-within-group-residual-ppm 10000 \
    --max-consensus-residual-ppm 20000 \
    --max-scale-cycle-residual-ppm 50000 \
    --min-cross-validation-groups 2 \
    --max-relative-scale-nano 1000000000000 \
    --max-scale-path-expansions "$scale_budget" \
    --format json
}

run_scale "$WITNESSES" "$WITNESS_DIGEST" 1000 1000 >"$FIRST"
run_scale "$WITNESSES" "$WITNESS_DIGEST" 1000 1000 >"$SECOND"
cmp "$FIRST" "$SECOND"
run_scale "$WITNESSES" "$WITNESS_DIGEST" 100000 100000 >"$LARGER_BUDGET"
run_scale "$CONFLICTING_WITNESSES" "$CONFLICTING_WITNESS_DIGEST" 1000 1000 >"$CONFLICTING"

"$PYTHON" - "$FIRST" "$LARGER_BUDGET" "$CONFLICTING" "$NODES" "$EDGES" "$WITNESSES" "$WITNESS_DIGEST" <<'PY'
import json
import sys

def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)

first = load(sys.argv[1])
larger = load(sys.argv[2])
conflicting = load(sys.argv[3])
assert first["schema"] == "fdgr.edge_scale_generation/1"
assert first["unit"] == "component_edge_scale_unit"
assert first["subject_count"] == 3
assert first["witness_count"] == 5
assert first["relation_count"] == 3
assert first["component_count"] == 1
assert first["components"][0]["status"] == "cycle_consistent"
assert first["components"][0]["cross_validated"] is True
scales = {entry["edge_id"]: entry["relative_scale_nano"] for entry in first["scales"]}
assert scales == {10: 1000000000, 20: 2000000000, 30: 1000000000}
assessments = {entry["relation_id"]: entry for entry in first["relation_assessments"]}
assert sorted(entry["status"] for entry in assessments.values()) == ["consistent", "forest", "forest"]
assert first["generation_digest"] == larger["generation_digest"]
assert first["policy"]["max_path_expansions"] != larger["policy"]["max_path_expansions"]
assert first["witness_basis_digest"] != sys.argv[7]
assert conflicting["components"][0]["status"] == "conflicted"
assert conflicting["components"][0]["cross_validated"] is False
assert any(entry["status"] == "conflicting" for entry in conflicting["relation_assessments"])
rendered = json.dumps(first, sort_keys=True)
for path in sys.argv[4:7]:
    assert path not in rendered
assert "meter" not in rendered.lower()
assert "camera_center" not in rendered
PY

printf '6\t1616161616161616161616161616161616161616161616161616161616161616\t106\t10\t20\t1\t2\t1000\t10\texternal_oracle\n' >>"$WITNESSES"
set +e
run_scale "$WITNESSES" "$WITNESS_DIGEST" 1000 1000 >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated edge-scale witnesses were accepted under the stale basis identity\n' >&2
  exit 1
fi
if ! grep -q 'edge-scale witness basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale edge-scale witness refusal lacked stable public context\n' >&2
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
    "suite": "edge_scale_resolve_public_path",
    "source_commit": sys.argv[3],
    "cargo_version": sys.argv[4],
    "rustc_version": sys.argv[5],
    "consistent_generation_digest": consistent["generation_digest"],
    "conflicting_generation_digest": conflicting["generation_digest"],
    "cross_validated": consistent["components"][0]["cross_validated"],
    "cycle_conflict_preserved": conflicting["components"][0]["status"] == "conflicted",
    "mutated_basis_exit_code": int(sys.argv[6]),
    "mutated_basis_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
