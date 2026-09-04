# `fdgr-projection`

`fdgr-projection` is the deterministic fixed-point scalar projection boundary for exact derived camera domains.

It provides:

- global-shutter pinhole projection;
- deterministic staged Brown-Conrady radial and tangential distortion;
- exact undistorted normalized camera coordinates;
- positive-depth and half-open image-domain evidence;
- explicit normalized and projected coordinate bounds;
- a compatibility rectified-only API that continues to reject distorted input;
- typed refusal of rolling-shutter projection until an exact row-time motion model exists.

Perspective division cancels the arbitrary component scale. The crate therefore grants no metric, pose, landmark, optimizer, calibration-accuracy, held-out-validation, or publication authority. The caller must prove that the supplied point is expressed in the exact camera frame corresponding to the derived calibration.
