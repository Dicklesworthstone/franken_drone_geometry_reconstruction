# `fdgr-keyframe`

`fdgr-keyframe` is FDGR's deterministic reference boundary for turning exact decoded-frame quality
and visibility evidence into a bounded keyframe set.

It provides:

- exact timeline, decoded-generation, calibration, candidate-table, and policy identities;
- fixed-point blur/exposure/texture/dynamic-content and overlap gates;
- explicit visibility-cell, view-sector, and baseline-bin evidence;
- deterministic marginal-coverage and diversity-aware greedy selection;
- stable quality, temporal-spacing, redundancy, and capacity rejection reasons;
- canonical selection identities and lossless JSON;
- replay validation and input-order-invariance tests.

The crate does not decode images, estimate optical flow, invent visibility cells, or claim pose or
coverage authority. Its candidate metrics are evidence inputs. A later optimized or learned path
must reproduce the same admitted semantics or publish a separately versioned policy.
