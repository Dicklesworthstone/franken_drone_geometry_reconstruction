# fdgr-projection

Deterministic fixed-point projection kernels for exact rectified image domains.

The current reference kernel projects component-relative camera-frame points through a validated `DerivedCalibration` and returns exact nano-pixel coordinates, positive depth, and an explicit in-domain witness. It accepts only distortion-free, global-shutter derived calibrations. Distorted domains and rolling-shutter domains fail closed until their required distortion and time-varying-pose semantics are implemented.

Perspective division cancels the arbitrary component scale. This crate therefore grants no metric, pose, landmark, optimization, accuracy, or publication authority.
