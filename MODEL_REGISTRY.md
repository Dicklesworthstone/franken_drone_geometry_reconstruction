
# Model Registry and Admission Doctrine

**Research snapshot:** 2026-08-30. The machine summary lives in
`registries/models.toml`. This document is deliberately per-artifact: a repository being Apache
licensed does not automatically authorize every checkpoint, dataset, container, or derivative.

## Roles, not monarchs

FDGR separates model roles:

- geometry prior: poses, intrinsics, depth, point maps, correspondences, confidence;
- streaming prior: bounded-state estimates for long video;
- mask/tracking prior: persistent 2D regions across frames;
- semantic proposer: object, material, relation, and inspection hypotheses;
- reranker: prioritize evidence or capture suggestions;
- validator: challenge another model on held-out views or semantic counterexamples.

No model is the system of record. Model outputs are immutable proposal bundles tied to an anchor,
worker profile, exact artifacts, coordinate policy, and resource receipt.

## Initial candidates

### MapAnything Apache checkpoint — default geometry candidate

The Apache-2.0 checkpoint is the best initial general-purpose candidate because it exposes a
unified feed-forward geometry interface and permissive redistribution posture. It can propose
camera, depth, point, ray, and confidence products from several input modality combinations.

Admission still requires:

- exact weight and code digests;
- offline materialization and no-network inference;
- coordinate and crop reconstruction tests;
- determinism envelope for the selected hardware/profile;
- drone/UAV and home-exterior benchmark results;
- scale honesty, including behavior when priors are absent or contradictory;
- uncertainty calibration rather than raw confidence reuse;
- held-out reprojection and classical refinement compatibility.

### Depth Anything 3 Streaming — long-sequence candidate

Its sliding-window streaming mode is architecturally attractive for provisional geometry under
bounded GPU memory. The exact checkpoint, license, training restrictions, window state, boundary
artifacts, drift behavior, and reproducibility profile must be reviewed before admission. Code
license alone is not a weight receipt.

### CUT3R and other recurrent/stateful models — online candidates

Continuous-state reconstruction may reduce repeated full-window inference and improve live
feedback. It also creates hidden-state identity and replay problems. A stateful worker must publish
state checkpoints, frame high-water marks, reset conditions, numeric policy, and deterministic
replay fixtures. An opaque Python object in GPU memory is not a durable reconstruction state.

### VGGT-Ω — research-only challenger

The released materials use the FAIR Noncommercial Research License. The maintainers also posted an
August 18, 2026 notice that possible benchmark contamination in an ancestor checkpoint may inflate
reported results for the released 1B model. FDGR may use an exact checkpoint in a separate
noncommercial research campaign to challenge other methods, but it cannot be a default commercial
or distributable runtime and published benchmark claims are not accepted as admission evidence.

### Qwen3.8-27B — semantic proposer

The Apache-2.0 27B dense vision-language model is a strong candidate for interpreting selected
frames, video segments, geometry renders, and structured evidence packs. It may propose concepts,
attributes, relations, inspection questions, and counterevidence. It may not:

- define camera pose, depth, metric scale, or geometry;
- resolve a critical asset from a single caption;
- infer hidden utilities as observed fact;
- certify absence;
- grant capability or approve publication;
- consume unredacted private footage outside the admitted local worker policy.

### SAM 3.1 — mask/tracking candidate

Video object multiplexing and tracking are useful for dynamic masks and persistent semantic
regions. The exact checkpoint, access terms, license, artifact digest, and execution profile remain
unresolved. Its masks are observations with failure modes around fine structures, reflections,
occlusion, textureless regions, and prompt ambiguity.

## Worker identity

A model execution identity includes:

```text
model registry ID
weight object roots
code and environment roots
runtime and driver versions
hardware profile
precision / quantization / kernel policy
preprocessing and crop policy
input anchor and exact input roots
randomness policy and seed
output schema and coordinate convention
resource budgets
license receipt
```

Changing any load-bearing field creates another profile. “Same model” is insufficient for replay.

## Correlated evidence

An ensemble of models trained on overlapping datasets or derived from a common ancestor is not
independent evidence. The model registry records lineage and correlation classes. Combining five
correlated depth estimates cannot manufacture a five-witness scale proof or an unjustifiably narrow
uncertainty interval.

## Model admission gates

1. **Artifact:** exact bytes, license, source, redistribution, provenance, malware/static scan.
2. **Execution:** offline install, bounded worker, no-network default, descendant cleanup.
3. **Contract:** schema, finite values, coordinates, crops, masks, confidence, state checkpoints.
4. **Reproducibility:** repeated-run envelope on named hardware and deterministic fixtures.
5. **Domain:** UAV/home, motion blur, rolling shutter, foliage, sky, reflective glass, low texture,
   repetitive siding, oblique views, and changing exposure.
6. **Geometry:** held-out reprojection, trajectory, depth, scale, surface, topology, completeness,
   and uncertainty calibration.
7. **Semantics:** presence, localization, tracking, confusion, counterevidence, and absence refusal.
8. **Economics:** latency, memory, energy, storage, operator time, and quality gain versus baseline.
9. **Fallback:** absence or failure leaves authoritative evidence intact and produces a typed
   degraded result.
10. **Release:** exact artifact is included in the compatibility and qualification matrix.

Adaptive routing may select among admitted profiles based on budget and workload. It cannot select
an unadmitted model or lower a claim gate.
