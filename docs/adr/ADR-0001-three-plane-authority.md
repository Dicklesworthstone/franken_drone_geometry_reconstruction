
# ADR-0001 — Three-plane authority separation

**Status:** Accepted for design

FDGR separates authoritative evidence, reconstruction/cognition, and device/effect planes.
Reconstruction and semantic components can propose claims but cannot publish or operate devices.
Device and process adapters can materialize requested outputs but cannot declare them correct.
Only the authoritative plane owns identities, anchors, claims, policy, publication, and completion.

This prevents a model, ffmpeg invocation, cloud response, or adapter acknowledgement from silently
becoming truth. The cost is explicit tickets, manifests, validation, and publication coordination.
That cost is accepted because every major FDGR failure mode crosses one of these seams.
