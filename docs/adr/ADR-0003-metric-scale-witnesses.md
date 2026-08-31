
# ADR-0003 — Metric scale is witness-gated

**Status:** Accepted for design

Monocular and multi-view models frequently produce geometry whose apparent metric scale depends on
training priors, calibration assumptions, crop, scene content, or internal gauge choices. FDGR
therefore stores scale status separately from geometry and prohibits metric claims until a
registered witness establishes the transform for a named domain.

Accepted witness families may include measured fiducials, measured segments/baselines,
admitted telemetry, cross-sensor alignment, and survey control. Correlated model predictions do
not become independent witnesses. Contradictions can invalidate a previously witnessed transform.
All dimensions carry the witness and uncertainty.
