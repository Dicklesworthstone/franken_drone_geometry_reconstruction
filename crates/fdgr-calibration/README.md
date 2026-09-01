# `fdgr-calibration`

`fdgr-calibration` defines FDGR's deterministic reference representation for camera calibration and
image-geometry derivation. It makes the assumptions that are commonly hidden in video-to-3D
pipelines explicit and content-addressed:

- exact camera profile, lens state, evidence basis, calibration epoch, and model generation;
- source image dimensions and top-left, half-pixel-center convention;
- pinhole intrinsics in fixed nano-pixel units;
- explicit none or Brown-Conrady distortion coefficients;
- explicit global or directional rolling-shutter readout;
- body-to-camera rigid extrinsics with deterministic orthonormality and determinant checks;
- residual and uncertainty summaries;
- exact crop/resize derivations with transformed intrinsics and a new immutable identity;
- applicability checks against the exact profile, lens state, dimensions, and temperature scope.

The crate does not estimate calibration from images, infer unstated stabilization, apply a hidden
vendor camera model, or claim that metadata has passed `CLAIM-CAL-001`. A later deterministic
solver and held-out reprojection corpus must publish evidence against these same types.
