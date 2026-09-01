#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CARGO="${CARGO:-cargo}"
PYTHON="${PYTHON:-python3}"
RUSTC="${RUSTC:-rustc}"
for tool in "$CARGO" "$PYTHON" "$RUSTC"; do
  command -v "$tool" >/dev/null 2>&1 || {
    printf 'ERROR: required tool is unavailable: %s\n' "$tool" >&2
    exit 3
  }
done

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fdgr-media-timeline-e2e.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
SOURCE="$TMP_ROOT/flight-with-ctts.mp4"
STORE="$TMP_ROOT/store"
INGEST_JSON="$TMP_ROOT/ingest.json"
TIMELINE_A="$TMP_ROOT/timeline-a.json"
TIMELINE_B="$TMP_ROOT/timeline-b.json"
PARTIAL_JSON="$TMP_ROOT/timeline-partial.json"

"$PYTHON" - "$SOURCE" <<'PY'
from pathlib import Path
import struct
import sys


def be32(value: int) -> bytes:
    return struct.pack(">I", value)


def i32(value: int) -> bytes:
    return struct.pack(">i", value)


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

# Version-1 composition offsets produce PTS [2000, 1000, 2000, 3000], proving that
# presentation order is distinct from decode order without inventing a timestamp repair.
ctts_payload = bytearray(b"\x01\x00\x00\x00" + be32(2))
ctts_payload.extend(be32(1) + i32(2_000))
ctts_payload.extend(be32(3) + i32(0))
ctts = atom(b"ctts", bytes(ctts_payload))

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

stbl = container(b"stbl", [stsd, stts, ctts, stsz, stco, stsc])
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
assert document["closure_verified"] is True
print(document["root"]["manifest_digest"])
PY
)"

for output in "$TIMELINE_A" "$TIMELINE_B"; do
  "$CARGO" run --quiet --locked -p fdgr-cli -- \
    recorded-media-timeline "$STORE" "$ROOT_MANIFEST" \
    --track-id 1 \
    --start-sample 0 \
    --sample-limit 4 \
    --max-window-records 4 \
    --max-index-entries-scanned 128 \
    --max-boxes 128 \
    --max-tracks 4 \
    --max-table-entries 128 \
    --max-table-bytes 4096 \
    --format json >"$output"
done

"$CARGO" run --quiet --locked -p fdgr-cli -- \
  recorded-media-timeline "$STORE" "$ROOT_MANIFEST" \
  --track-id 1 \
  --start-sample 1 \
  --sample-limit 2 \
  --max-window-records 2 \
  --max-index-entries-scanned 128 \
  --format json >"$PARTIAL_JSON"

SOURCE_COMMIT="$(git rev-parse HEAD)"
CARGO_VERSION="$($CARGO --version)"
RUSTC_VERSION="$($RUSTC --version)"
"$PYTHON" - "$TIMELINE_A" "$TIMELINE_B" "$PARTIAL_JSON" "$SOURCE" \
  "$SOURCE_COMMIT" "$CARGO_VERSION" "$RUSTC_VERSION" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    full = json.load(handle)
with open(sys.argv[2], encoding="utf-8") as handle:
    replay = json.load(handle)
with open(sys.argv[3], encoding="utf-8") as handle:
    partial = json.load(handle)

assert full == replay
assert full["schema"] == "fdgr.media_timeline/1"
assert full["track_id"] == 1
assert full["timescale"] == 1_000
assert full["returned_samples"] == 4
assert full["reaches_track_end"] is True
assert full["covers_entire_track"] is True
assert full["prefix_unrepresented_samples"] == 0
assert full["suffix_unrepresented_samples"] == 0
assert full["presentation_reordered"] is True
assert full["source_byte_order_reordered"] is False
assert full["total_gap_duration"] == 0
assert [item["presentation_time_ticks"] for item in full["samples"]] == [
    "2000",
    "1000",
    "2000",
    "3000",
]
assert full["samples"][0]["composition_offset_ticks"] == "2000"
assert sys.argv[4] not in json.dumps(full, sort_keys=True)

assert partial["start_sample"] == 1
assert partial["end_sample"] == 3
assert partial["returned_samples"] == 2
assert partial["reaches_track_end"] is False
assert partial["covers_entire_track"] is False
assert partial["prefix_unrepresented_samples"] == 1
assert partial["suffix_unrepresented_samples"] == 1

fixture = Path(sys.argv[4]).read_bytes()
receipt = {
    "schema": "fdgr.test_receipt/1",
    "suite": "recorded_media_timeline",
    "source_commit": sys.argv[5],
    "cargo_version": sys.argv[6],
    "rustc_version": sys.argv[7],
    "fixture_sha256": hashlib.sha256(fixture).hexdigest(),
    "recorded_media_root_manifest_digest": full["recorded_media_root_manifest_digest"],
    "timeline_digest": full["timeline_digest"],
    "deterministic_replay": True,
    "presentation_reordering_observed": True,
    "partial_coverage_explicit": True,
    "source_path_absent": True,
    "verdict": "pass",
}
print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
PY
