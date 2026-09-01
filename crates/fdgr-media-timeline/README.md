# `fdgr-media-timeline`

`fdgr-media-timeline` is the deterministic reference boundary between authenticated encoded-media
sample tables and later clock, calibration, keyframe, decode, and geometry work.

It consumes an already validated `fdgr_media::TrackSampleWindow` plus exact recorded-media/source
identities and produces one canonical `fdgr.media_timeline/1` value with:

- exact track/timescale and recorded-media/source roots;
- explicit represented sample range, prefix/suffix omissions, and whole-track coverage;
- DTS, PTS, signed composition offset, duration, byte interval, sync state, and sample-description
  identity for every represented sample;
- explicit decode-time gaps rather than inferred or repaired timestamps;
- presentation-order and source-byte-order reordering flags;
- non-overlap and source-bounds validation for encoded sample intervals;
- a domain-separated deterministic semantic digest and lossless JSON projection.

Request ceilings and index-scan counts remain visible diagnostics but are intentionally excluded
from the semantic digest, so an optimized index can prove equality with the reference timeline
without pretending that identical evidence had identical execution cost.

The crate does **not** decode media, estimate a telemetry clock, interpolate absent samples, absorb
edit-list semantics that were not parsed upstream, or claim that a partial window represents an
entire track. Those remain separate evidence and authority boundaries.

Production timestamp fusion must cite this immutable source timeline (or an explicitly registered
successor format) rather than rebuilding packet/frame timing ad hoc in each downstream subsystem.
