
# ADR-0004 — Exact originals are retained as immutable evidence

**Status:** Accepted for design

FDGR never normalizes in place. The exact acquired bytes, source metadata, and range accounting are
published first. Analysis, preview, archive, and thumbnail renditions are derived siblings.

This permits re-decoding with better codecs, correcting timestamp/calibration bugs, benchmarking
new reconstruction models, proving provenance, and detecting encoder drift. Storage cost is
managed through content addressing, deduplication, tiering, compression siblings, erasure coding,
and explicit retention policy rather than destructive replacement.
