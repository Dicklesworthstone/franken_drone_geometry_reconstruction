# `fdgr-relative-pose`

`fdgr-relative-pose` is the deterministic fixed-point reference boundary between descriptor-backed
correspondence hypotheses and later pose-graph state.

It provides:

- exact correspondence, calibration, bearing-table, candidate-table, policy, frame-pair, and generation identities;
- calibrated unit-bearing evidence and explicit per-match uncertainty;
- validated left-to-right rotation and translation-direction candidates;
- normalized epipolar residuals with epipole-degeneracy refusal;
- parallax and least-squares two-ray cheirality evidence;
- aggregate inlier, positive-depth, residual, and parallax gates;
- explicit no-candidate, ambiguous, and uniquely geometrically-verified outcomes;
- complete per-candidate/per-match rejection evidence;
- deterministic canonical identity, JSON, replay, and input-order invariance;
- operation ceilings that remain cost evidence rather than contaminating successful semantic identity.

The crate does **not** generate five-point/eight-point candidates, estimate metric translation,
publish a camera pose, or mutate a pose graph. A uniquely verified candidate is still a two-view
relative-motion evidence product bound to one exact basis. Pose-graph admission, loop consistency,
bundle refinement, scale, and held-out validation remain later gates.
