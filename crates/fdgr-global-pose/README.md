# `fdgr-global-pose`

`fdgr-global-pose` deterministically initializes component-relative camera centers only after exact pose-graph orientations and relative pose-edge baseline gauges are available.

It provides:

- exact pose-graph, edge-scale, policy, and generation identities;
- one explicit zero-origin gauge per connected camera component;
- the correct left-to-right camera-center displacement convention;
- refusal to combine forest edges from unrelated scale components;
- deterministic tree propagation with exact parent-edge provenance;
- translation-cycle assessments for every non-forest pose edge;
- explicit consistent, conflicting, and incomparable-gauge cycle outcomes;
- component maturity that preserves orientation, scale, and translation contradictions;
- deterministic replay, canonical bytes, JSON, and operation-cost separation.

The output unit is `component_edge_scale_unit_nano`. It is neither meters nor a bundle-adjusted trajectory. This crate performs no nonlinear optimization, reprojection minimization, covariance estimation, or metric-scale admission.
