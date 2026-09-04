# `fdgr-reprojection`

`fdgr-reprojection` is the deterministic scalar trust boundary between an audited structural bundle problem and any future pose-landmark optimizer.

It provides:

- exact bundle-admission, calibration-table, algorithm, policy, and generation identities;
- exact materialization and digest verification of every effective camera calibration;
- fixed-point root-frame to camera-frame transformation;
- deterministic pinhole and Brown-Conrady projection;
- cheirality, depth, normalized-coordinate, image-domain, and residual gates;
- uncertainty-normalized residuals without floating-point ambiguity;
- explicit optimize versus held-out evidence summaries;
- hard refusal to treat static-pose projection as rolling-shutter authority when row-time motion is unavailable;
- component decisions, evidence-only versus positive authority projection, canonical bytes, JSON, and replay;
- operation ceilings that bound execution without contaminating successful semantic identity.

The oracle evaluates the camera and landmark state already retained by `fdgr.bundle_admission/1`. It does not optimize poses, landmarks, intrinsics, distortion, or rolling-shutter motion. It does not compute covariance or numerical rank, admit metric scale, or publish sparse geometry.
