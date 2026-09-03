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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-global-pose-singleton.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
NODES="$TMP_ROOT/nodes.tsv"
EDGES="$TMP_ROOT/edges.tsv"
WITNESSES="$TMP_ROOT/witnesses.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"

cat >"$NODES" <<'EOF_NODES'
node_id	sample_index	keyframe_digest
7	70	0707070707070707070707070707070707070707070707070707070707070707
EOF_NODES

cat >"$EDGES" <<'EOF_EDGES'
edge_id	verification_digest	admitted_candidate_id	left_node_id	right_node_id	left_sample_index	right_sample_index	r00	r01	r02	r10	r11	r12	r20	r21	r22	tx	ty	tz	supported_match_count	median_residual_nano
EOF_EDGES

cat >"$WITNESSES" <<'EOF_WITNESSES'
witness_id	evidence_digest	correlation_group_id	lower_edge_id	higher_edge_id	ratio_numerator	ratio_denominator	uncertainty_ppm	support_count	source
EOF_WITNESSES

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
GRAPH_POLICY_DIGEST="5151515151515151515151515151515151515151515151515151515151515151"
ROTATION_POLICY_DIGEST="5252525252525252525252525252525252525252525252525252525252525252"
SCALE_POLICY_DIGEST="5353535353535353535353535353535353535353535353535353535353535353"
GLOBAL_POLICY_DIGEST="5454545454545454545454545454545454545454545454545454545454545454"

run_singleton() {
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    global-pose-initialize "$NODES" "$EDGES" "$WITNESSES" \
    --node-file-digest "$NODE_DIGEST" \
    --pose-edge-file-digest "$EDGE_DIGEST" \
    --scale-witness-file-digest "$WITNESS_DIGEST" \
    --graph-selection-policy-digest "$GRAPH_POLICY_DIGEST" \
    --rotation-policy-digest "$ROTATION_POLICY_DIGEST" \
    --pose-graph-generation 1 \
    --edge-scale-policy-digest "$SCALE_POLICY_DIGEST" \
    --edge-scale-generation 1 \
    --global-pose-policy-digest "$GLOBAL_POLICY_DIGEST" \
    --global-pose-generation 1 \
    --max-pose-path-expansions 100 \
    --max-scale-path-expansions 100 \
    --max-global-pose-operations 100 \
    --format json
}

run_singleton >"$FIRST"
run_singleton >"$SECOND"
cmp "$FIRST" "$SECOND"

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$NODES" "$EDGES" "$WITNESSES" <<'PY'
import json
import sys
from pathlib import Path

result = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
assert result["schema"] == "fdgr.global_pose_initialization/1"
assert result["authority"] == "relative_component_gauge"
assert result["unit"] == "component_edge_scale_unit_nano"
assert result["pose_count"] == 1
assert result["translation_cycle_count"] == 0
assert result["component_count"] == 1
assert len(result["poses"]) == 1
assert result["poses"][0]["node_id"] == 7
assert result["poses"][0]["component_root_node_id"] == 7
assert result["poses"][0]["scale_component_root_edge_id"] is None
assert result["poses"][0]["camera_center_from_root_nano"] == [0, 0, 0]
assert result["poses"][0]["parent_node_id"] is None
assert result["poses"][0]["parent_edge_id"] is None
assert len(result["components"]) == 1
assert result["components"][0]["status"] == "singleton"
assert result["components"][0]["scale_component_root_edge_id"] is None
assert result["components"][0]["node_ids"] == [7]
assert result["components"][0]["forest_edge_ids"] == []
assert result["components"][0]["non_forest_edge_ids"] == []
rendered = json.dumps(result, sort_keys=True)
for path in sys.argv[5:8]:
    assert path not in rendered
for forbidden in ("meter", "bundle_adjust", "trajectory_publication"):
    assert forbidden not in rendered.lower()
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "global_pose_singleton_public_path",
    "source_commit": sys.argv[2],
    "cargo_version": sys.argv[3],
    "rustc_version": sys.argv[4],
    "initialization_digest": result["initialization_digest"],
    "pose_graph_generation_digest": result["pose_graph_generation_digest"],
    "edge_scale_generation_digest": result["edge_scale_generation_digest"],
    "singleton_component_preserved": True,
    "empty_edge_scale_universe_preserved": True,
    "relative_authority_preserved": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
