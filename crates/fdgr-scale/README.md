# `fdgr-scale`

`fdgr-scale` is FDGR's deterministic reference boundary for resolving metric scale without letting
one model prior, duplicated metadata, or a convenient average become measurement authority.

It provides:

- exact geometry, calibration, scope, and witness-basis identities;
- typed model-prior, telemetry, measured-baseline, fiducial, and survey-control witnesses;
- one robust vote per correlation group;
- deterministic median-ratio fitting and one-pass robust reclassification;
- retained inlier, outlier, conflict, residual, and uncertainty evidence;
- separate `relative_only`, `estimated`, `witnessed`, and `surveyed` authority;
- a metric mapping API that refuses estimated, relative-only, or conflicted scale;
- domain-separated canonical identities and lossless JSON.

A resolved candidate can remain `estimated`: that is useful for rendering and hypothesis work but
cannot authorize a metric claim. `witnessed` and `surveyed` require independent evidence classes and
remain scoped to the exact geometry/calibration/domain basis. This crate does not infer a baseline
from motion, trust vendor telemetry by default, or satisfy `CLAIM-SCALE-001` without the registered
witness and residual evidence.
