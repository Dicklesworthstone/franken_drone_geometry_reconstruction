
# Benchmark and Qualification Matrix

## Geometry metrics

- trajectory absolute and relative error where reference poses exist;
- camera rotation/translation error and loop consistency;
- reprojection residual distributions, including held-out frames;
- depth absolute-relative, RMSE, threshold accuracy, and edge fidelity;
- point/surface Chamfer distance, precision/recall/F-score at several thresholds;
- surface completeness, accuracy, normal consistency, and topology defects;
- metric scale error and uncertainty coverage;
- dimension error for registered segments, openings, and assets;
- free-space/occupancy contradiction rate;
- uncertainty calibration and selective-risk curves.

## Semantic metrics

- 2D and 3D presence precision/recall by ontology concept;
- mask IoU and track identity consistency;
- 3D localization and extent error;
- relation accuracy (`attached_to`, `serves`, `adjacent_to`, `accessed_via`);
- critical-asset false-resolution rate;
- counterevidence sensitivity;
- absence refusal and certified-absence accuracy under known coverage;
- human-review burden per resolved asset.

## System metrics

- ingest bytes/s and packet/frame accounting;
- preview latency and online geometry staleness;
- end-to-end time to first useful draft and converged generation;
- CPU/GPU memory, energy, and cost;
- local and cloud storage amplification by class;
- multipart resume and restore performance;
- cancellation drain time and unresolved obligations;
- deterministic replay and output digest stability;
- operator interventions and additional-capture minutes.

## Corpora

1. Synthetic procedural homes with exact cameras, depth, meshes, semantics, controlled weather,
   motion, rolling shutter, noise, and codec faults.
2. Indoor/outdoor public geometry datasets where licensing permits.
3. UAVFF3D and other UAV-domain benchmarks for camera-prior and domain-shift analysis.
4. FDGR private home corpus with survey/LiDAR/reference measurements and privacy controls.
5. Device compatibility corpus across recorded/live acquisition profiles.
6. Adversarial corpus: low texture, repetitive siding, glass, metal, water, foliage, shadows,
   moving people/cars, sky dominance, exposure changes, and weak loop closure.

## Same-binary experiment doctrine

Alternative algorithms run in one binary from identical immutable inputs. Timing begins after
semantic setup. Each receipt records source, toolchain, target, profile, exact models, hardware,
input roots, arm, policy, warmup, samples, output digests, correctness comparison, and statistics.
An arm whose semantics differ is not a performance comparison until the difference is intended and
separately evaluated.
