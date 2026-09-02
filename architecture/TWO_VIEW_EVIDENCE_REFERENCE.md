# Two-View Evidence Reference

**Status:** executable reference architecture; not global or metric pose authority

This document defines the current boundary between decoded image evidence and future pose-graph admission. It exists to prevent agents and downstream code from collapsing several materially different propositions into the word “match” or “pose.”

## Evidence ladder

```text
authenticated recorded-media root
  → canonical encoded-sample timeline
    → immutable decoded-frame generation (target; not yet implemented)
      → exact calibration or derived-calibration identity
        → keyframe candidate evidence
          → deterministic selected keyframe generation
            → exact feature-observation table
              → descriptor correspondence hypotheses
                → collision-safe multi-view tracks
                  → calibrated bearing correspondences
                    → exact relative-motion candidate set
                      → epipolar/parallax/cheirality adjudication
                        → future view graph and pose graph
```

Each arrow is an explicit evidence transformation. A later stage may reject or retract an earlier proposal; it may not silently strengthen it.

## Authority distinctions

```text
descriptor similarity
≠ correspondence truth
≠ epipolar support
≠ rigid-motion candidate selection
≠ globally consistent camera pose
≠ metric camera pose
≠ geometry publication
```

### Descriptor correspondence

`fdgr-correspondence` consumes exact feature and frame-pair tables. It performs bounded Hamming nearest-neighbor search and records:

- unique nearest versus tied nearest;
- second-best availability;
- distance and ratio gates;
- optional mutual-nearest support;
- response, localization-uncertainty, and dynamic-mask eligibility;
- operation-budget consumption;
- accepted descriptor edges;
- explicit rejection reasons;
- collision-safe track components;
- unmatched and ineligible observations.

An accepted edge means only that the declared descriptor policy admitted a pair. It carries no epipolar, depth, pose, scale, or semantic authority.

### Collision-safe tracks

Pairwise edges are processed in deterministic quality order. Union is refused when it would place two observations from one source frame in the same connected component. Cycle-closing descriptor edges are retained explicitly.

This invariant gives later geometry a valid multi-view observation topology without pretending that the topology is geometrically correct.

### Calibrated bearing evidence

The relative-pose verifier consumes normalized fixed-point bearing vectors whose exact calibration and correspondence generation are named in the basis. The bearing table is independently content-addressed.

A bearing record names:

- match identity;
- left and right observation identities;
- normalized left and right camera rays;
- declared angular/localization uncertainty.

The verifier refuses non-unit vectors, duplicate match or observation-pair identities, all-zero bases, unsafe numeric identities, and excessive collection or operation bounds.

### Relative-motion candidates

A candidate is a proposal, not an estimator result blessed by FDGR. Each candidate contains:

- a stable candidate identity;
- an exact source-evidence digest;
- an origin class such as minimal solver, eight-point solver, telemetry prior, model prior, or manual hypothesis;
- an orthonormal left-to-right rotation;
- a unit translation direction under the explicit convention
  `x_right = R_left_to_right × x_left + t_left_origin_in_right`.

Translation magnitude is intentionally absent. No metric baseline is implied.

## Deterministic adjudication

For every candidate and bearing match, the reference implementation computes fixed-point evidence for:

1. **Epipolar-plane conditioning.** Rays near the translation epipole are rejected as degenerate rather than credited with trivially small residuals.
2. **Normalized epipolar residual.** The residual is scaled by the epipolar-normal magnitude and expanded by declared bearing uncertainty.
3. **Parallax.** Insufficient angular separation is retained as an explicit rejection, because low-residual near-collinear rays do not constrain triangulation.
4. **Cheirality.** A deterministic two-ray least-squares sign test requires positive depth in both views when the policy enables it.
5. **Candidate-level support.** Inlier count, inlier ratio, positive-depth ratio, median residual, maximum residual, and median parallax are retained.

The output state is one of:

```text
no_accepted_candidate
ambiguous
geometrically_verified
```

A `geometrically_verified` result means only that one member of the supplied candidate set satisfied the declared gates and outranked the other admitted members under the registered deterministic policy. It is not proof that the candidate set contained the physical motion, and it is not global pose authority.

An ambiguous result carries no selected transform. A unique result includes the selected source, evidence identity, rotation, and translation direction directly so an agent need not reopen the local candidate table.

## Identity and cost

Semantic result identity includes:

- correspondence generation;
- calibration;
- exact bearing table;
- exact candidate table;
- semantic policy;
- frame pair and generation;
- canonical matches and candidates;
- every candidate evaluation;
- final status and ambiguity set.

Execution ceilings and observed evaluation counts remain operation-cost evidence. A faster implementation may produce the same semantic identity when it evaluates the same exact evidence under the same semantic policy.

## Public diagnostic adapters

```bash
fdgr correspondence-build <features.tsv> <pairs.tsv> [exact bases and policy]
fdgr relative-pose-verify <bearings.tsv> <candidates.tsv> [exact bases and policy]
```

Both commands:

- reject symlinks and non-regular files;
- bound input bytes and records;
- authenticate complete file bytes before parsing;
- require exact versioned TSV headers;
- omit local paths from successful machine output;
- expose deterministic text or JSON;
- fail when evidence is mutated under a stale digest.

These commands are diagnostic/reference surfaces. They do not add top-level operations to the target Agent Narrow Waist. Future agent work should expose the same capabilities as typed affordances through `fdgr.propose`, `fdgr.commit`, and `fdgr.watch`.

## What remains before a pose graph

The current reference does not yet provide:

- native feature detection or descriptor extraction;
- calibrated pixel-to-bearing derivation from a decoded-frame generation;
- five-point, eight-point, homography, or generalized-camera candidate generation;
- robust sampling and degeneracy-model competition;
- multi-view triangulation;
- view-graph construction;
- loop-closure search and verification;
- global rotation/translation averaging;
- bundle adjustment;
- rolling-shutter motion compensation;
- metric baseline admission;
- held-out-view or surveyed accuracy evidence.

The next graph-native boundary should admit only exact `geometrically_verified` two-view receipts, preserve rejected and ambiguous pair evidence, detect bridges/articulation views and weak components, and publish cycle-closure obligations before numerical global optimization.
