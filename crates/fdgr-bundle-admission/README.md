# `fdgr-bundle-admission`

Deterministic audit between an exact `fdgr.bundle_problem/1` structural generation and any future landmark-bearing optimizer.

The crate:

- authenticates one exact image width, height, frame identity, and effective-calibration identity per camera;
- verifies every observation against its half-open top-left image domain;
- authenticates optimize-only landmark-seed provenance;
- proves that active seeds retain enough surviving support observations and cameras;
- removes held-out observations from independent evidence when their camera is absent from the final optimize core;
- recomputes component decisions instead of inheriting upstream `admit` blindly;
- preserves typed observation, landmark, and component audit evidence;
- separates semantic policy from successful execution ceilings.

Its only positive authority is `audited_relative_bundle_problem`. It does not estimate calibration, triangulate or refine landmarks, minimize reprojection error, prove numerical rank, admit metric scale, or publish sparse geometry.

See [`../../architecture/BUNDLE_ADMISSION_REFERENCE.md`](../../architecture/BUNDLE_ADMISSION_REFERENCE.md).
