# `fdgr-clock`

`fdgr-clock` is FDGR's deterministic reference boundary for relating named sensor clocks without
silently treating timestamp fields as interchangeable.

It provides:

- canonical source/reference clock domains, timescales, epochs, and model generations;
- content-addressed synchronization anchors with one robust vote per correlation group;
- an integer-only Theil-Sen-style affine fit with deterministic median offset;
- bounded drift, residual, outlier, and uncertainty accounting;
- support-bounded mapping that refuses extrapolation;
- an ordered model ledger that makes same-epoch gaps and source/reference resets explicit;
- domain-separated model and ledger identities plus lossless JSON.

A valid model is not by itself `CLAIM-TIME-001`. Timeline continuity still requires complete
packet/decode accounting, explicit missing/duplicate/reordered evidence, named epochs, and bounded
residual over the exact claimed domain. This crate does not read device clocks, interpolate
telemetry, infer a relationship from wall time, or supervise adapters.
