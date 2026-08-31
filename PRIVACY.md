
# Privacy Doctrine

A detailed digital twin of a home is intrinsically private. FDGR is local-first and treats privacy
scope as part of every anchor, model request, archive policy, report, and export.

## Default posture

- Raw media, telemetry, packet captures, geometry, textures, and scene graphs stay local.
- Model workers run locally with network disabled unless a separate explicit remote-processing
  capability is granted.
- Cloud replication is opt-in, private, encrypted according to policy, and verified.
- No public sharing link is created implicitly.
- Location and geodetic anchors are separate capabilities from local building coordinates.
- Faces, license plates, neighboring windows/yards, screens, documents, and people are detected as
  privacy-sensitive observations and can trigger redaction or publication refusal.

## Spatial scopes

A session names an authorized capture and processing domain. Geometry outside that domain may be
retained as raw evidence when unavoidable but excluded, blurred, cropped, or generalized in
published twins. Search counts and absence claims also respect scope so derived indexes cannot leak
hidden regions.

## Derived-product classes

```text
private_evidence        originals, telemetry, full geometry, detailed semantics
private_operational     previews, indexes, diagnostics, model proposals
shareable_owner         selected reports and exports for trusted recipients
shareable_redacted      textures/coordinates/assets redacted under a named policy
public                   requires an explicit publication review and receipt
```

Moving an object to a less restrictive class is an effect with a plan and terminal proof. Merely
removing obvious EXIF does not make a 3D model anonymous.

## Human subjects and bystanders

Capture playbooks should minimize people and private activity. Dynamic humans are masked from
static fusion where possible. Retaining a person track for geometry rejection does not grant
authority to identify, classify, or publish the person. Semantic ontology work focuses on property
assets, not personal attributes.

## Model privacy

Prompts and context packs include only the minimum frames, crops, geometry summaries, and ontology
needed for a task. Remote APIs are not part of the default architecture. Local open-weight models
must still be treated as untrusted processes that may emit memorized or irrelevant content.

## Retention and deletion

Authoritative deletion is difficult because manifests, replicas, repair symbols, caches, reports,
indexes, and branches may reference an object. `doctor` enumerates references; `delete plan`
produces a sealed graph cut; apply revalidates the root, removes eligible objects, verifies remote
state, and publishes a tombstone receipt. Deleting a derived preview never implies deletion of the
original, and deleting the original is blocked while a retention obligation requires it.
