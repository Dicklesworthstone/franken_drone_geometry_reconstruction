# Changelog

All notable implementation changes are recorded here. Target-state design prose remains in the
comprehensive plan; this file records executable evolution and corrections to previously overstated
or understated implementation status.

## Unreleased

### Added

- A canonical custody-bound media timeline reference crate (`fdgr-media-timeline`).
- Exact timeline binding to recorded-media root, source manifest, source object, track, and
  timescale identities.
- Explicit represented sample range, prefix/suffix omissions, whole-track coverage, DTS, PTS,
  signed composition offsets, decode gaps, sync counts, sample-description identities, source byte
  intervals, source-byte reordering, and presentation reordering.
- Domain-separated `fdgr.media_timeline/1` identities and lossless JSON presentation timestamps.
- Separation of semantic timeline identity from request ceilings and index-scan diagnostics, so
  optimized and reference indexers can produce the same evidence identity with different costs.
- `recorded-media-timeline`, which verifies the recorded-media root before deriving a bounded
  timeline from authenticated source bytes.
- A JSON Schema and registry entry for the timeline output.
- A local public-path E2E lane proving deterministic replay, CTTS presentation reordering, explicit
  partial coverage, source-path omission, and exact root binding.

### Fixed

- The orphaned `fdgr-media-timeline/Cargo.toml` is now a real workspace member with source,
  documentation, lockfile membership, tests, schemas, and qualification ownership.
- Workspace validation now continues to reject every crate directory that is absent from either the
  root member list or `Cargo.lock`.
- Documentation no longer describes the repository as a three-crate scaffold or claims that native
  media parsing and recorded-media integration are wholly unimplemented.
- Partial sample windows can no longer be confused with complete track coverage.
- Presentation-domain `i128` timestamps are emitted as decimal strings rather than lossy JSON
  numbers.
- Encoded sample byte intervals are checked both against authenticated source length and against
  one another, while legitimate non-monotonic source byte order is reported rather than rewritten.

### Qualification

- GitHub-hosted Actions remain non-authoritative. The local qualifier and Doodlestein job graph own
  format, check, Clippy, tests, and public-path E2E receipts.
