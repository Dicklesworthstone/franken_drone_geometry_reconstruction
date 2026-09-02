# `fdgr-epipolar`

`fdgr-epipolar` is the deterministic geometric-admission boundary between descriptor correspondence
hypotheses and later relative-pose estimation.

It provides:

- exact correspondence, calibration, observation-table, candidate-table, policy, frame-pair, and
  generation identities;
- canonical homogeneous essential-matrix candidates with scale/sign normalization and duplicate
  refusal;
- bounded symmetric L1 epipolar residuals over calibrated normalized image coordinates;
- per-match uncertainty gates and explicit undefined-line/residual outcomes;
- rank-two determinant diagnostics, minimum inlier ratio/count, and independent spatial-support
  gates;
- deterministic candidate ranking, runner-up evidence, and conservative ambiguity refusal;
- canonical identities, lossless JSON, replay validation, and input-order-invariance tests;
- explicit separation of semantic output from operation budgets and observed evaluation cost.

An admitted result means only **epipolar-supported hypothesis**. It does not establish a rotation,
translation, cheirality, parallax, metric baseline, camera pose, loop closure, or geometry. A later
relative-pose layer must retain this exact verification basis and earn those stronger claims.
