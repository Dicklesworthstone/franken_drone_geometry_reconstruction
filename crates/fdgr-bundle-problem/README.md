# fdgr-bundle-problem

Deterministic structural admission between an exact component-relative pose refinement and any future landmark-bearing bundle optimizer.

The crate:

- authenticates every camera against one exact sample, frame object, and effective calibration;
- authenticates component-relative landmark seeds and camera observations;
- keeps optimize and held-out evidence disjoint;
- computes a deterministic fixed-point camera/landmark support core;
- derives a bipartite topology certificate with cycles and observation-level bridges;
- emits one typed status, decision, and minimum-cost next action per pose component.

Its only positive authority is `admitted_relative_bundle_problem`. It does not triangulate or refine landmarks, minimize reprojection error, estimate covariance or numerical rank, admit metric scale, or publish sparse geometry.

See [`../../architecture/BUNDLE_PROBLEM_REFERENCE.md`](../../architecture/BUNDLE_PROBLEM_REFERENCE.md) for the normative contract.
