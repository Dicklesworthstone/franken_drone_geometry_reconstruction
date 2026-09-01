#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CARGO="${CARGO:-cargo}"
RUSTC="${RUSTC:-rustc}"
PYTHON="${PYTHON:-python3}"
for tool in "$CARGO" "$RUSTC" "$PYTHON"; do
  command -v "$tool" >/dev/null 2>&1 || {
    printf 'ERROR: required tool is unavailable: %s\n' "$tool" >&2
    exit 3
  }
done

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-recorded-media-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
SOURCE="$TMP_ROOT/flight.mp4"
STORE="$TMP_ROOT/store"
INGEST_JSON="$TMP_ROOT/ingest.json"
VERIFY_JSON="$TMP_ROOT/verify.json"
PLAN_JSON="$TMP_ROOT/decode-plan.json"
PLAN_REPEAT_JSON="$TMP_ROOT/decode-plan-repeat.json"
RANGE_STDOUT="$TMP_ROOT/range.stdout"
RANGE_STDERR="$TMP_ROOT/range.stderr"
CORRUPT_STDOUT="$TMP_ROOT/corrupt.stdout"
CORRUPT_STDERR="$TMP_ROOT/corrupt.stderr"
WORKER_EXECUTABLE_DIGEST="$(printf '1%.0s' {1..64})"
WORKER_VERSION_DIGEST="$(printf '2%.0s' {1..64})"
PROFILE_DIGEST="$(printf '3%.0s' {1..64})"

"$PYTHON" - "$SOURCE" <<'PY'
from pathlib import Path
import struct
import sys


def be32(value: int) -> bytes:
    return struct.pack(">I", value)


def atom(kind: bytes, payload: bytes) -> bytes:
    return be32(8 + len(payload)) + kind + payload


def container(kind: bytes, children: list[bytes]) -> bytes:
    return atom(kind, b"".join(children))


def put32(buffer: bytearray, offset: int, value: int) -> None:
    buffer[offset : offset + 4] = be32(value)


ftyp = atom(b"ftyp", b"isom" + be32(0) + b"isom")
mdat = atom(b"mdat", bytes(18))

mvhd_payload = bytearray(20)
put32(mvhd_payload, 12, 1_000)
put32(mvhd_payload, 16, 4_000)
mvhd = atom(b"mvhd", bytes(mvhd_payload))

tkhd_payload = bytearray(84)
put32(tkhd_payload, 12, 1)
put32(tkhd_payload, 76, 1_920 << 16)
put32(tkhd_payload, 80, 1_080 << 16)
tkhd = atom(b"tkhd", bytes(tkhd_payload))

mdhd_payload = bytearray(20)
put32(mdhd_payload, 12, 1_000)
put32(mdhd_payload, 16, 4_000)
mdhd = atom(b"mdhd", bytes(mdhd_payload))

hdlr_payload = bytearray(12)
hdlr_payload[8:12] = b"vide"
hdlr = atom(b"hdlr", bytes(hdlr_payload))

stsd_payload = bytearray(8)
put32(stsd_payload, 4, 1)
stsd_payload.extend(be32(8) + b"avc1")
stsd = atom(b"stsd", bytes(stsd_payload))

stts_payload = bytearray(16)
put32(stts_payload, 4, 1)
put32(stts_payload, 8, 4)
put32(stts_payload, 12, 1_000)
stts = atom(b"stts", bytes(stts_payload))

stsz_payload = bytearray(28)
put32(stsz_payload, 8, 4)
for index, size in enumerate((3, 4, 5, 6)):
    put32(stsz_payload, 12 + index * 4, size)
stsz = atom(b"stsz", bytes(stsz_payload))

stco_payload = bytearray(16)
put32(stco_payload, 4, 2)
put32(stco_payload, 8, 28)
put32(stco_payload, 12, 35)
stco = atom(b"stco", bytes(stco_payload))

stsc_payload = bytearray(20)
put32(stsc_payload, 4, 1)
put32(stsc_payload, 8, 1)
put32(stsc_payload, 12, 2)
put32(stsc_payload, 16, 1)
stsc = atom(b"stsc", bytes(stsc_payload))

stbl = container(b"stbl", [stsd, stts, stsz, stco, stsc])
minf = container(b"minf", [stbl])
mdia = container(b"mdia", [mdhd, hdlr, minf])
trak = container(b"trak", [tkhd, mdia])
moov = container(b"moov", [mvhd, trak])

Path(sys.argv[1]).write_bytes(ftyp + mdat + moov)
PY

"$CARGO" run --quiet --locked -p fdgr-cli -- \
  recorded-media-ingest "$STORE" "$SOURCE" \
  --source-chunk-size 16 \
  --derived-chunk-size 64 \
  --max-boxes 128 \
  --max-tracks 4 \
  --max-table-entries 128 \
  --max-table-bytes 4096 \
  --format json >"$INGEST_JSON"

ROOT_MANIFEST="$($PYTHON - "$INGEST_JSON" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    document = json.load(handle)
assert document["schema"] == "fdgr.recorded_media_ingest/1"
assert document["publication_complete"] is True
assert document["closure_verified"] is True
assert document["media"]["schema"] == "fdgr.media_inspection/1"
assert document["media"]["decode_performed"] is False
assert document["media"]["track_count"] == 1
assert document["media"]["tracks"][0]["track_id"] == 1
assert document["media"]["tracks"][0]["sample_count"] == 4
print(document["root"]["manifest_digest"])
PY
)"

"$CARGO" run --quiet --locked -p fdgr-cli -- \
  recorded-media-verify "$STORE" "$ROOT_MANIFEST" --format json >"$VERIFY_JSON"

"$PYTHON" - "$INGEST_JSON" "$VERIFY_JSON" "$SOURCE" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    ingest = json.load(handle)
with open(sys.argv[2], encoding="utf-8") as handle:
    verified = json.load(handle)
assert verified["schema"] == "fdgr.verified_recorded_media/1"
assert verified["closure_verified"] is True
assert verified["root_manifest_digest"] == ingest["root"]["manifest_digest"]
assert verified["root_object_digest"] == ingest["root"]["object_digest"]
assert verified["source"] == ingest["source"]
assert verified["inspection"] == ingest["inspection"]
assert verified["media"] == ingest["media"]
assert sys.argv[3] not in json.dumps(ingest, sort_keys=True)
assert sys.argv[3] not in json.dumps(verified, sort_keys=True)
PY

run_decode_plan() {
  local output="$1"
  local samples="$2"
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    media-decode-plan "$STORE" "$ROOT_MANIFEST" \
    --track-id 1 \
    --start-sample 0 \
    --max-samples "$samples" \
    --pixel-format rgb24 \
    --width 2 \
    --height 2 \
    --worker-executable-digest "$WORKER_EXECUTABLE_DIGEST" \
    --worker-version-digest "$WORKER_VERSION_DIGEST" \
    --profile-digest "$PROFILE_DIGEST" \
    --worker-threads 1 \
    --format json >"$output"
}

run_decode_plan "$PLAN_JSON" 4
run_decode_plan "$PLAN_REPEAT_JSON" 4
cmp -s "$PLAN_JSON" "$PLAN_REPEAT_JSON" || {
  printf 'ERROR: identical decode planning was not byte-stable\n' >&2
  exit 1
}

"$PYTHON" - "$VERIFY_JSON" "$PLAN_JSON" "$SOURCE" \
  "$WORKER_EXECUTABLE_DIGEST" "$WORKER_VERSION_DIGEST" "$PROFILE_DIGEST" <<'PY'
import json
import re
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    verified = json.load(handle)
with open(sys.argv[2], encoding="utf-8") as handle:
    plan = json.load(handle)
assert plan["schema"] == "fdgr.media_decode_plan/1"
assert re.fullmatch(r"[0-9a-f]{64}", plan["plan_digest"])
assert plan["source_root_manifest_digest"] == verified["root_manifest_digest"]
assert plan["source_manifest_digest"] == verified["source"]["manifest_digest"]
assert plan["source_object_digest"] == verified["source"]["object_digest"]
assert plan["source_object_length"] == verified["source"]["object_length"]
assert plan["track_id"] == 1
assert plan["start_sample"] == 0
assert plan["max_samples"] == 4
assert plan["pixel_format"] == "rgb24"
assert plan["output_width"] == 2
assert plan["output_height"] == 2
assert plan["max_frames"] == 4
assert plan["max_output_bytes"] == 48
assert plan["worker_executable_digest"] == sys.argv[4]
assert plan["worker_version_digest"] == sys.argv[5]
assert plan["profile_digest"] == sys.argv[6]
assert plan["worker_threads"] == 1
assert plan["network_allowed"] is False
assert plan["deterministic"] is True
assert sys.argv[3] not in json.dumps(plan, sort_keys=True)


def keys(node):
    if isinstance(node, dict):
        for key, value in node.items():
            yield key
            yield from keys(value)
    elif isinstance(node, list):
        for value in node:
            yield from keys(value)


assert not any("path" in key or "dispatch" in key for key in keys(plan))
PY

set +e
run_decode_plan "$RANGE_STDOUT" 5 2>"$RANGE_STDERR"
RANGE_STATUS=$?
set -e
if [[ "$RANGE_STATUS" -eq 0 ]]; then
  printf 'ERROR: out-of-range decode plan was accepted\n' >&2
  exit 1
fi
if ! grep -q 'exceeds track 1 sample count 4' "$RANGE_STDERR"; then
  printf 'ERROR: range refusal lacked stable track-domain context\n' >&2
  cat "$RANGE_STDERR" >&2
  exit 1
fi

ROOT_OBJECT="$($PYTHON - "$VERIFY_JSON" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as handle:
    print(json.load(handle)["root_object_digest"])
PY
)"
ROOT_OBJECT_PATH="$STORE/objects/${ROOT_OBJECT:0:2}/${ROOT_OBJECT:2}.fdgr-object"

"$PYTHON" - "$ROOT_OBJECT_PATH" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
assert data
data[0] ^= 0x01
path.write_bytes(data)
PY

set +e
"$CARGO" run --quiet --locked -p fdgr-cli -- \
  recorded-media-verify "$STORE" "$ROOT_MANIFEST" --format json \
  >"$CORRUPT_STDOUT" 2>"$CORRUPT_STDERR"
CORRUPT_STATUS=$?
set -e
if [[ "$CORRUPT_STATUS" -eq 0 ]]; then
  printf 'ERROR: corrupted root object was accepted\n' >&2
  exit 1
fi
if ! grep -q 'recorded-media verification failed' "$CORRUPT_STDERR"; then
  printf 'ERROR: corruption refusal lacked stable public command context\n' >&2
  cat "$CORRUPT_STDERR" >&2
  exit 1
fi

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$($RUSTC --version)"
"$PYTHON" - "$INGEST_JSON" "$VERIFY_JSON" "$PLAN_JSON" "$SOURCE" \
  "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" "$RANGE_STATUS" \
  "$CORRUPT_STATUS" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    ingest = json.load(handle)
with open(sys.argv[2], encoding="utf-8") as handle:
    verified = json.load(handle)
with open(sys.argv[3], encoding="utf-8") as handle:
    plan = json.load(handle)
fixture = Path(sys.argv[4]).read_bytes()
plan_bytes = Path(sys.argv[3]).read_bytes()
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "recorded_media_ingest_verify_and_decode_plan",
    "source_commit": sys.argv[5],
    "cargo_version": sys.argv[6],
    "rustc_version": sys.argv[7],
    "fixture_sha256": hashlib.sha256(fixture).hexdigest(),
    "root_manifest_digest": verified["root_manifest_digest"],
    "root_object_digest": verified["root_object_digest"],
    "source_manifest_digest": verified["source"]["manifest_digest"],
    "inspection_manifest_digest": verified["inspection"]["manifest_digest"],
    "decode_plan_digest": plan["plan_digest"],
    "decode_plan_json_sha256": hashlib.sha256(plan_bytes).hexdigest(),
    "decode_plan_network_allowed": plan["network_allowed"],
    "decode_plan_deterministic": plan["deterministic"],
    "publication_complete": ingest["publication_complete"],
    "closure_verified": verified["closure_verified"],
    "range_exit_code": int(sys.argv[8]),
    "range_refused": True,
    "corruption_exit_code": int(sys.argv[9]),
    "corruption_refused": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
