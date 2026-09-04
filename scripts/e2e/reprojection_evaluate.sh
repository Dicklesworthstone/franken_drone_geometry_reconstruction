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

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-reprojection-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
NODES="$TMP_ROOT/nodes.tsv"
EDGES="$TMP_ROOT/edges.tsv"
WITNESSES="$TMP_ROOT/witnesses.tsv"
LANDMARKS="$TMP_ROOT/landmarks.tsv"
OBSERVATIONS="$TMP_ROOT/observations.tsv"
CONFLICT_OBSERVATIONS="$TMP_ROOT/conflict-observations.tsv"
PROVENANCE="$TMP_ROOT/provenance.tsv"
CAMERAS="$TMP_ROOT/cameras.tsv"
DOMAINS="$TMP_ROOT/domains.tsv"
CALIBRATIONS="$TMP_ROOT/calibrations.tsv"
REORDERED_CALIBRATIONS="$TMP_ROOT/reordered-calibrations.tsv"
SUBSTITUTED_CALIBRATIONS="$TMP_ROOT/substituted-calibrations.tsv"
ROLLING_CAMERAS="$TMP_ROOT/rolling-cameras.tsv"
ROLLING_DOMAINS="$TMP_ROOT/rolling-domains.tsv"
ROLLING_CALIBRATIONS="$TMP_ROOT/rolling-calibrations.tsv"
FIRST="$TMP_ROOT/first.json"
SECOND="$TMP_ROOT/second.json"
REORDERED="$TMP_ROOT/reordered.json"
LARGER_BUDGET="$TMP_ROOT/larger-budget.json"
CONFLICT="$TMP_ROOT/conflict.json"
ROLLING="$TMP_ROOT/rolling.json"
SUBSTITUTION_STDOUT="$TMP_ROOT/substitution.stdout"
SUBSTITUTION_STDERR="$TMP_ROOT/substitution.stderr"
MUTATED_STDOUT="$TMP_ROOT/mutated.stdout"
MUTATED_STDERR="$TMP_ROOT/mutated.stderr"
BUDGET_STDOUT="$TMP_ROOT/budget.stdout"
BUDGET_STDERR="$TMP_ROOT/budget.stderr"

"$PYTHON" - "$NODES" "$EDGES" "$WITNESSES" "$LANDMARKS" "$OBSERVATIONS" "$CONFLICT_OBSERVATIONS" "$PROVENANCE" "$CAMERAS" "$DOMAINS" "$CALIBRATIONS" "$REORDERED_CALIBRATIONS" "$SUBSTITUTED_CALIBRATIONS" "$ROLLING_CAMERAS" "$ROLLING_DOMAINS" "$ROLLING_CALIBRATIONS" <<'PY'
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

(
    nodes_path,
    edges_path,
    witnesses_path,
    landmarks_path,
    observations_path,
    conflict_observations_path,
    provenance_path,
    cameras_path,
    domains_path,
    calibrations_path,
    reordered_calibrations_path,
    substituted_calibrations_path,
    rolling_cameras_path,
    rolling_domains_path,
    rolling_calibrations_path,
) = map(Path, sys.argv[1:])

NANO = 1_000_000_000
DERIVED_SCHEMA = "fdgr.derived_calibration/1"
CALIBRATION_HEADER = [
    "camera_node_id",
    "frame_digest",
    "effective_calibration_digest",
    "source_calibration_digest",
    "source_width",
    "source_height",
    "crop_x",
    "crop_y",
    "crop_width",
    "crop_height",
    "output_width",
    "output_height",
    "fx_nano_pixels",
    "fy_nano_pixels",
    "cx_nano_pixels",
    "cy_nano_pixels",
    "distortion_model",
    "k1_nano",
    "k2_nano",
    "p1_nano",
    "p2_nano",
    "k3_nano",
    "rolling",
    "readout_direction",
    "first_observed_line_offset_ns",
    "observed_readout_time_ns",
    "reference_phase_nano",
    "r00",
    "r01",
    "r02",
    "r10",
    "r11",
    "r12",
    "r20",
    "r21",
    "r22",
    "tx_micrometers",
    "ty_micrometers",
    "tz_micrometers",
    "declared_uncertainty_nano_pixels",
]


def u8(value: int) -> bytes:
    return struct.pack(">B", value)


def u32(value: int) -> bytes:
    return struct.pack(">I", value)


def u64(value: int) -> bytes:
    return struct.pack(">Q", value)


def i64(value: int) -> bytes:
    return struct.pack(">q", value)


def put_str(value: str) -> bytes:
    encoded = value.encode()
    return u64(len(encoded)) + encoded


def domain_hash(domain: str, payload: bytes) -> str:
    encoded = domain.encode()
    framed = b"FDGR\0" + u64(len(encoded)) + encoded + u64(len(payload)) + payload
    return hashlib.sha256(framed).hexdigest()


def repeated(byte: int) -> str:
    return bytes([byte]).hex() * 32


def derived_payload(source_digest: str, rolling: bool) -> bytes:
    values = bytearray()
    values.extend(put_str(DERIVED_SCHEMA))
    values.extend(bytes.fromhex(source_digest))
    for value in (640, 480, 0, 0, 640, 480, 640, 480):
        values.extend(u32(value))
    for value in (100 * NANO, 100 * NANO, 320 * NANO, 240 * NANO):
        values.extend(i64(value))
    values.extend(u8(1))
    values.extend(u8(0))
    for _ in range(5):
        values.extend(i64(0))
    values.extend(u8(1 if rolling else 0))
    values.extend(u8(1 if rolling else 0))
    values.extend(u64(0))
    values.extend(u64(10_000_000 if rolling else 0))
    values.extend(u32(500_000_000 if rolling else 0))
    for value in (NANO, 0, 0, 0, NANO, 0, 0, 0, NANO):
        values.extend(i64(value))
    for _ in range(3):
        values.extend(i64(0))
    values.extend(u64(100_000_000))
    return bytes(values)


def calibration_row(camera: int, rolling: bool, retain_digest: str | None = None) -> tuple[list[str], str, str]:
    frame_digest = repeated(0x70 + camera)
    source_digest = repeated((0x50 if retain_digest is None else 0x60) + camera)
    effective_digest = retain_digest or domain_hash(
        DERIVED_SCHEMA, derived_payload(source_digest, rolling)
    )
    row = [
        str(camera),
        frame_digest,
        effective_digest,
        source_digest,
        "640",
        "480",
        "0",
        "0",
        "640",
        "480",
        "640",
        "480",
        str(100 * NANO),
        str(100 * NANO),
        str(320 * NANO),
        str(240 * NANO),
        "none",
        "0",
        "0",
        "0",
        "0",
        "0",
        "true" if rolling else "false",
        "top_to_bottom" if rolling else "none",
        "0",
        "10000000" if rolling else "0",
        "500000000" if rolling else "0",
        str(NANO),
        "0",
        "0",
        "0",
        str(NANO),
        "0",
        "0",
        "0",
        str(NANO),
        "0",
        "0",
        "0",
        "100000000",
    ]
    return row, frame_digest, effective_digest


def write_rows(path: Path, header: list[str], rows: list[list[str]]) -> None:
    path.write_text(
        "\t".join(header) + "\n" + "".join("\t".join(row) + "\n" for row in rows),
        encoding="utf-8",
    )


nodes_path.write_text(
    "node_id\tsample_index\tkeyframe_digest\n"
    + "1\t10\t" + repeated(1) + "\n"
    + "2\t20\t" + repeated(2) + "\n"
    + "3\t30\t" + repeated(3) + "\n",
    encoding="utf-8",
)
edges_path.write_text(
    "edge_id\tverification_digest\tadmitted_candidate_id\tleft_node_id\tright_node_id\tleft_sample_index\tright_sample_index\tr00\tr01\tr02\tr10\tr11\tr12\tr20\tr21\tr22\ttx\tty\ttz\tsupported_match_count\tmedian_residual_nano\n"
    + f"10\t{repeated(0x10)}\t1\t1\t2\t10\t20\t{NANO}\t0\t0\t0\t{NANO}\t0\t0\t0\t{NANO}\t{NANO}\t0\t0\t30\t100\n"
    + f"20\t{repeated(0x20)}\t1\t2\t3\t20\t30\t{NANO}\t0\t0\t0\t{NANO}\t0\t0\t0\t{NANO}\t{NANO}\t0\t0\t29\t100\n"
    + f"30\t{repeated(0x30)}\t1\t1\t3\t10\t30\t{NANO}\t0\t0\t0\t{NANO}\t0\t0\t0\t{NANO}\t{NANO}\t0\t0\t10\t100\n",
    encoding="utf-8",
)
witnesses_path.write_text(
    "witness_id\tevidence_digest\tcorrelation_group_id\tlower_edge_id\thigher_edge_id\tratio_numerator\tratio_denominator\tuncertainty_ppm\tsupport_count\tsource\n"
    + f"1\t{repeated(0x11)}\t101\t10\t20\t1\t1\t1000\t20\tshared_track_geometry\n"
    + f"2\t{repeated(0x12)}\t102\t10\t20\t1\t1\t1000\t18\tmulti_view_geometry\n"
    + f"3\t{repeated(0x13)}\t103\t20\t30\t1\t2\t1000\t20\tshared_track_geometry\n"
    + f"4\t{repeated(0x14)}\t104\t20\t30\t1\t2\t1000\t18\tmulti_view_geometry\n"
    + f"5\t{repeated(0x15)}\t105\t10\t30\t1\t2\t2000\t5\tmodel_prior\n",
    encoding="utf-8",
)
landmark_rows = [
    ["100", "500", "1", "10", repeated(0xA1), "0", "0", str(5 * NANO), "100000000"],
    ["200", "600", "1", "10", repeated(0xA2), str(NANO), "0", str(5 * NANO), "100000000"],
    ["300", "700", "1", "10", repeated(0xA3), str(2 * NANO), "0", str(5 * NANO), "100000000"],
]
write_rows(
    landmarks_path,
    [
        "landmark_id",
        "source_track_id",
        "component_root_node_id",
        "scale_component_root_edge_id",
        "seed_evidence_digest",
        "seed_x_nano",
        "seed_y_nano",
        "seed_z_nano",
        "seed_uncertainty_nano",
    ],
    landmark_rows,
)

base_calibrations = [calibration_row(camera, False) for camera in (1, 2, 3)]
rolling_calibrations = [calibration_row(camera, camera == 3) for camera in (1, 2, 3)]
write_rows(calibrations_path, CALIBRATION_HEADER, [item[0] for item in base_calibrations])
write_rows(
    reordered_calibrations_path,
    CALIBRATION_HEADER,
    [item[0] for item in reversed(base_calibrations)],
)
substituted_rows = [item[0].copy() for item in base_calibrations]
substituted = calibration_row(1, False, retain_digest=base_calibrations[0][2])[0]
substituted_rows[0] = substituted
write_rows(substituted_calibrations_path, CALIBRATION_HEADER, substituted_rows)
write_rows(
    rolling_calibrations_path,
    CALIBRATION_HEADER,
    [item[0] for item in rolling_calibrations],
)

def write_camera_and_domain(
    camera_path: Path,
    domain_path: Path,
    calibrations: list[tuple[list[str], str, str]],
) -> None:
    write_rows(
        camera_path,
        ["camera_node_id", "sample_index", "frame_digest", "effective_calibration_digest"],
        [
            [str(camera), str(camera * 10), frame, effective]
            for camera, (_, frame, effective) in zip((1, 2, 3), calibrations, strict=True)
        ],
    )
    write_rows(
        domain_path,
        ["camera_node_id", "frame_digest", "effective_calibration_digest", "image_width", "image_height"],
        [
            [str(camera), frame, effective, "640", "480"]
            for camera, (_, frame, effective) in zip((1, 2, 3), calibrations, strict=True)
        ],
    )


write_camera_and_domain(cameras_path, domains_path, base_calibrations)
write_camera_and_domain(
    rolling_cameras_path, rolling_domains_path, rolling_calibrations
)

points_x = {100: 0, 200: NANO, 300: 2 * NANO}
centers_x = {1: 0, 2: -NANO, 3: -2 * NANO}
specifications = [
    (1, 100, 1, "optimize"),
    (2, 100, 2, "optimize"),
    (3, 100, 3, "optimize"),
    (4, 200, 1, "optimize"),
    (5, 200, 2, "optimize"),
    (6, 200, 3, "optimize"),
    (7, 300, 1, "optimize"),
    (8, 300, 2, "optimize"),
    (9, 300, 3, "held_out"),
]
observation_rows: list[list[str]] = []
for observation_id, landmark_id, camera, role in specifications:
    camera_x = points_x[landmark_id] - centers_x[camera]
    x = 320 * NANO + (100 * NANO * camera_x) // (5 * NANO)
    observation_rows.append(
        [
            str(observation_id),
            str(landmark_id),
            str(camera),
            str(camera * 10),
            base_calibrations[camera - 1][1],
            str(1000 + observation_id),
            repeated(0x20 + observation_id),
            str(x),
            str(240 * NANO),
            "100000000",
            "false",
            role,
        ]
    )
observation_header = [
    "observation_id",
    "landmark_id",
    "camera_node_id",
    "sample_index",
    "frame_digest",
    "source_feature_observation_id",
    "evidence_digest",
    "x_nano_pixels",
    "y_nano_pixels",
    "localization_uncertainty_nano_pixels",
    "dynamic_masked",
    "role",
]
write_rows(observations_path, observation_header, observation_rows)
conflict_rows = [row.copy() for row in observation_rows]
conflict_rows[0][7] = str(int(conflict_rows[0][7]) + 3 * NANO)
write_rows(conflict_observations_path, observation_header, conflict_rows)
write_rows(
    provenance_path,
    ["landmark_id", "support_observation_ids"],
    [["100", "1,2"], ["200", "4,5"], ["300", "7,8"]],
)
PY

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
LANDMARK_DIGEST="$(sha256_file "$LANDMARKS")"
OBSERVATION_DIGEST="$(sha256_file "$OBSERVATIONS")"
CONFLICT_OBSERVATION_DIGEST="$(sha256_file "$CONFLICT_OBSERVATIONS")"
PROVENANCE_DIGEST="$(sha256_file "$PROVENANCE")"
CAMERA_DIGEST="$(sha256_file "$CAMERAS")"
DOMAIN_DIGEST="$(sha256_file "$DOMAINS")"
CALIBRATION_DIGEST="$(sha256_file "$CALIBRATIONS")"
REORDERED_CALIBRATION_DIGEST="$(sha256_file "$REORDERED_CALIBRATIONS")"
SUBSTITUTED_CALIBRATION_DIGEST="$(sha256_file "$SUBSTITUTED_CALIBRATIONS")"
ROLLING_CAMERA_DIGEST="$(sha256_file "$ROLLING_CAMERAS")"
ROLLING_DOMAIN_DIGEST="$(sha256_file "$ROLLING_DOMAINS")"
ROLLING_CALIBRATION_DIGEST="$(sha256_file "$ROLLING_CALIBRATIONS")"
GRAPH_POLICY_DIGEST="4141414141414141414141414141414141414141414141414141414141414141"
ROTATION_POLICY_DIGEST="4242424242424242424242424242424242424242424242424242424242424242"
SCALE_POLICY_DIGEST="4343434343434343434343434343434343434343434343434343434343434343"
GLOBAL_POLICY_DIGEST="4444444444444444444444444444444444444444444444444444444444444444"
REFINEMENT_POLICY_DIGEST="4545454545454545454545454545454545454545454545454545454545454545"

run_evaluation() {
  local observation_path="$1"
  local observation_digest="$2"
  local camera_path="$3"
  local camera_digest="$4"
  local domain_path="$5"
  local domain_digest="$6"
  local calibration_path="$7"
  local calibration_digest="$8"
  local reprojection_operations="$9"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    reprojection-evaluate "$NODES" "$EDGES" "$WITNESSES" "$camera_path" "$LANDMARKS" "$observation_path" "$domain_path" "$PROVENANCE" "$calibration_path" \
    --node-file-digest "$NODE_DIGEST" \
    --pose-edge-file-digest "$EDGE_DIGEST" \
    --scale-witness-file-digest "$WITNESS_DIGEST" \
    --camera-binding-file-digest "$camera_digest" \
    --landmark-seed-file-digest "$LANDMARK_DIGEST" \
    --bundle-observation-file-digest "$observation_digest" \
    --camera-domain-file-digest "$domain_digest" \
    --seed-provenance-file-digest "$PROVENANCE_DIGEST" \
    --reprojection-calibration-file-digest "$calibration_digest" \
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
    --reprojection-generation 1 \
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
    --max-bundle-observation-uncertainty-nano-pixels 200000000 \
    --min-held-out-observations-per-component 1 \
    --min-held-out-cameras-per-component 1 \
    --max-bundle-graph-path-expansions 1000 \
    --max-bundle-operations 100000 \
    --max-seed-uncertainty-nano 200000000 \
    --min-seed-support-observations 2 \
    --min-seed-support-cameras 2 \
    --require-active-held-out-camera true \
    --max-bundle-admission-operations 100000 \
    --minimum-positive-depth-nano 1 \
    --max-normalized-coordinate-abs-nano 10000000000 \
    --max-projected-coordinate-abs-nano-pixels 1000000000000000 \
    --max-reprojection-residual-nano-pixels 2000000000 \
    --max-normalized-residual-ppm 4000000 \
    --min-reprojection-optimize-observations 8 \
    --min-reprojection-held-out-observations 1 \
    --max-reprojection-operations "$reprojection_operations" \
    --format json
}

run_evaluation "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$CAMERAS" "$CAMERA_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$CALIBRATIONS" "$CALIBRATION_DIGEST" 100000 >"$FIRST"
run_evaluation "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$CAMERAS" "$CAMERA_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$CALIBRATIONS" "$CALIBRATION_DIGEST" 100000 >"$SECOND"
cmp "$FIRST" "$SECOND"
run_evaluation "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$CAMERAS" "$CAMERA_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$REORDERED_CALIBRATIONS" "$REORDERED_CALIBRATION_DIGEST" 100000 >"$REORDERED"
run_evaluation "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$CAMERAS" "$CAMERA_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$CALIBRATIONS" "$CALIBRATION_DIGEST" 10000000 >"$LARGER_BUDGET"
run_evaluation "$CONFLICT_OBSERVATIONS" "$CONFLICT_OBSERVATION_DIGEST" "$CAMERAS" "$CAMERA_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$CALIBRATIONS" "$CALIBRATION_DIGEST" 100000 >"$CONFLICT"
run_evaluation "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$ROLLING_CAMERAS" "$ROLLING_CAMERA_DIGEST" "$ROLLING_DOMAINS" "$ROLLING_DOMAIN_DIGEST" "$ROLLING_CALIBRATIONS" "$ROLLING_CALIBRATION_DIGEST" 100000 >"$ROLLING"

"$PYTHON" - "$FIRST" "$REORDERED" "$LARGER_BUDGET" "$CONFLICT" "$ROLLING" "$NODES" "$EDGES" "$WITNESSES" "$CAMERAS" "$LANDMARKS" "$OBSERVATIONS" "$DOMAINS" "$PROVENANCE" "$CALIBRATIONS" <<'PY'
import json
import sys


def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


value = load(sys.argv[1])
reordered = load(sys.argv[2])
larger = load(sys.argv[3])
conflict = load(sys.argv[4])
rolling = load(sys.argv[5])
assert value["schema"] == "fdgr.reprojection_evaluation/1"
assert value["authority"] == "audited_relative_reprojection_baseline"
assert value["calibration_count"] == 3
assert value["observation_count"] == 9
assert value["component_count"] == 1
component = value["components"][0]
assert component["status"] == "consistent"
assert component["decision"] == "admit"
assert component["recommendation"] == "proceed_to_bounded_optimization"
assert component["optimize"] == {
    "eligible_observation_count": 8,
    "projected_observation_count": 8,
    "inlier_observation_count": 8,
    "rms_residual_nano_pixels": 0,
    "median_residual_nano_pixels": 0,
    "maximum_residual_nano_pixels": 0,
}
assert component["held_out"] == {
    "eligible_observation_count": 1,
    "projected_observation_count": 1,
    "inlier_observation_count": 1,
    "rms_residual_nano_pixels": 0,
    "median_residual_nano_pixels": 0,
    "maximum_residual_nano_pixels": 0,
}
assert all(item["disposition"] == "projected_inlier" for item in value["observations"])
assert value["reprojection_digest"] == reordered["reprojection_digest"]
assert value["calibration_table_digest"] == reordered["calibration_table_digest"]
assert value["reprojection_digest"] == larger["reprojection_digest"]
assert value["policy_digest"] == larger["policy_digest"]
assert value["policy"]["max_operations"] != larger["policy"]["max_operations"]
conflict_component = conflict["components"][0]
assert conflict["authority"] == "reprojection_evidence_only"
assert conflict_component["status"] == "residual_conflict"
assert conflict_component["decision"] == "admit_diagnostic"
assert conflict_component["conflicting_observation_ids"] == [1]
assert next(item for item in conflict["observations"] if item["observation_id"] == 1)["disposition"] == "residual_exceeded"
rolling_component = rolling["components"][0]
assert rolling["authority"] == "reprojection_evidence_only"
assert rolling_component["status"] == "projection_conflict"
assert rolling_component["decision"] == "block"
assert rolling_component["unprojectable_observation_ids"] == [3, 6, 9]
assert [item["observation_id"] for item in rolling["observations"] if item["disposition"] == "rolling_shutter_unsupported"] == [3, 6, 9]
rendered = json.dumps(value, sort_keys=True)
for path in sys.argv[6:]:
    assert path not in rendered
for forbidden in ("metric_scale", "bundle_adjusted", "optimized_landmark", "sparse_geometry", "covariance"):
    assert forbidden not in rendered.lower()
PY

set +e
run_evaluation "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$CAMERAS" "$CAMERA_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$SUBSTITUTED_CALIBRATIONS" "$SUBSTITUTED_CALIBRATION_DIGEST" 100000 >"$SUBSTITUTION_STDOUT" 2>"$SUBSTITUTION_STDERR"
SUBSTITUTION_STATUS=$?
set -e
if [[ "$SUBSTITUTION_STATUS" -eq 0 ]]; then
  printf 'ERROR: substituted calibration materialization was accepted\n' >&2
  exit 1
fi
if ! grep -q 'wrong effective calibration identity' "$SUBSTITUTION_STDERR"; then
  printf 'ERROR: calibration substitution refusal lacked stable public context\n' >&2
  cat "$SUBSTITUTION_STDERR" >&2
  exit 1
fi

printf '\n' >>"$CALIBRATIONS"
set +e
run_evaluation "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$CAMERAS" "$CAMERA_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$CALIBRATIONS" "$CALIBRATION_DIGEST" 100000 >"$MUTATED_STDOUT" 2>"$MUTATED_STDERR"
MUTATED_STATUS=$?
set -e
if [[ "$MUTATED_STATUS" -eq 0 ]]; then
  printf 'ERROR: mutated calibration bytes were accepted under a stale file identity\n' >&2
  exit 1
fi
if ! grep -q 'reprojection calibration basis digest mismatch' "$MUTATED_STDERR"; then
  printf 'ERROR: stale calibration refusal lacked stable public context\n' >&2
  cat "$MUTATED_STDERR" >&2
  exit 1
fi

set +e
run_evaluation "$OBSERVATIONS" "$OBSERVATION_DIGEST" "$CAMERAS" "$CAMERA_DIGEST" "$DOMAINS" "$DOMAIN_DIGEST" "$REORDERED_CALIBRATIONS" "$REORDERED_CALIBRATION_DIGEST" 1 >"$BUDGET_STDOUT" 2>"$BUDGET_STDERR"
BUDGET_STATUS=$?
set -e
if [[ "$BUDGET_STATUS" -eq 0 ]]; then
  printf 'ERROR: reprojection operation ceiling failed to refuse partial output\n' >&2
  exit 1
fi
if ! grep -q 'reprojection evaluation attempted operation' "$BUDGET_STDERR"; then
  printf 'ERROR: reprojection budget refusal lacked stable public context\n' >&2
  cat "$BUDGET_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$(rustc --version)"
"$PYTHON" - "$FIRST" "$CONFLICT" "$ROLLING" "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$SUBSTITUTION_STATUS" "$MUTATED_STATUS" "$BUDGET_STATUS" <<'PY'
import json
import sys


def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


admitted = load(sys.argv[1])
conflict = load(sys.argv[2])
rolling = load(sys.argv[3])
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "reprojection_evaluate_public_path",
    "source_commit": sys.argv[4],
    "cargo_version": sys.argv[5],
    "rustc_version": sys.argv[6],
    "reprojection_digest": admitted["reprojection_digest"],
    "exact_baseline_admitted": admitted["authority"] == "audited_relative_reprojection_baseline",
    "residual_conflict_preserved": conflict["components"][0]["status"] == "residual_conflict",
    "rolling_shutter_refused_without_motion_model": rolling["components"][0]["status"] == "projection_conflict",
    "calibration_substitution_exit_code": int(sys.argv[7]),
    "calibration_substitution_refused": True,
    "mutated_basis_exit_code": int(sys.argv[8]),
    "mutated_basis_refused": True,
    "budget_exit_code": int(sys.argv[9]),
    "budget_refused": True,
    "canonical_row_order_preserved": True,
    "execution_ceiling_nonsemantic": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
