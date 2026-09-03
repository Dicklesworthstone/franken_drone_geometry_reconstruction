# fdgr-pose-refinement

Deterministic fixed-point reference refinement for component-relative camera centers.

The crate consumes an exact `fdgr.global_pose_initialization/1` generation, keeps its rotations and
relative edge-scale gauges fixed, pins each component root, and performs bounded robust weighted
translation relaxation. It publishes objective change, factor dispositions, component decisions,
and exact operation evidence.

It does not estimate landmarks, rotations, calibration, scale, depth, or metric coordinates. It is
not full bundle adjustment and does not publish a global trajectory.
