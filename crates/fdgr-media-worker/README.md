# `fdgr-media-worker`

This crate defines the safe-Rust semantic protocol for bounded external media decoding. It is an
effect-adapter contract, not a process supervisor and not evidence that FFmpeg execution is
production-admitted.

Implemented reference surfaces:

- path-free decode plans bound to a verified recorded-media root, exact source bytes, track/sample
  range, decoded pixel representation, worker executable/version/profile identities, and hard
  frame/byte/time/memory/thread/network budgets;
- domain-separated canonical plan identities and deterministic JSON projection;
- a bounded parser for FFmpeg `framehash` version 2 with SHA-256 records;
- typed worker termination states that distinguish known failure, pre-dispatch cancellation,
  cancellation after dispatch, timeout, forced termination, and indeterminate outcome;
- receipt validation that rejects identity drift, false success, partial output-root publication,
  noncanonical frame sequences, inconsistent framehash totals, and successful resource overruns.

Still missing:

- Asupersync-owned process spawn, cancellation, descendant drain, operation lookup, sandboxing, and
  resource enforcement;
- exact admitted FFmpeg argument/environment profiles;
- publication of decoded frame objects and their root manifest;
- independent replay against real FFmpeg builds and media corpora.

No caller may treat a `MediaDecodePlan` as dispatch authority or a process exit as semantic decode
completion. Only a receipt validated against the exact plan and complete immutable output root can
satisfy the reference completion predicate.
