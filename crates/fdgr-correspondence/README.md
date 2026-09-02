# `fdgr-correspondence`

`fdgr-correspondence` is the deterministic reference boundary between admitted keyframe feature
evidence and later relative-pose estimation.

It provides:

- exact keyframe, calibration, feature-table, pair-table, policy, and generation identities;
- canonical fixed-point feature observations with 256-bit descriptors;
- bounded Hamming nearest-neighbor matching with explicit second-best, tie, distance, ratio, and optional mutual checks;
- explicit operation-budget accounting;
- deterministic collision-safe union of pairwise matches into multi-view tracks;
- refusal to place two observations from one frame in the same track;
- retained match rejections, cycle edges, unmatched observations, canonical identities, and JSON;
- deterministic replay and input-order invariance tests.

The crate does not detect features from images, estimate an essential matrix, assert epipolar
correctness, or establish pose authority. Descriptor matches are correspondence hypotheses. Later
geometric verification must either admit or reject them against the exact calibration and pose
basis.
