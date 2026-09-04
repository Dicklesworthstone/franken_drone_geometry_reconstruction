# Bundle Admission Authority

`fdgr.bundle_admission/1` is a component-scoped audit generation. Its top-level `authority` field is derived from the complete component decision set rather than emitted optimistically.

```text
all components decision = admit
  → authority = audited_relative_bundle_problem

any component decision = block or admit_diagnostic
  → authority = bundle_admission_evidence_only
```

This rule applies identically to canonical JSON and text-mode CLI output. Mixed generations remain evidence-only at the top level even though individual component records retain their own decisions and recommendations.

The authority field is a deterministic projection of component decisions already included in canonical semantic identity. Changing a successful execution ceiling does not change it or the generation digest.

Neither authority value proves calibration accuracy, numerical rank, favorable conditioning, optimizer convergence, held-out reprojection improvement, metric scale, or publishable geometry. `audited_relative_bundle_problem` permits only bounded optimization evaluation against the exact audit digest.
