# `fdgr-pose-graph`

`fdgr-pose-graph` composes exact pairwise relative-rotation hypotheses into deterministic,
component-local orientation hypotheses. It uses `fdgr-graph` to select a maximum-support forest,
propagates one arbitrary root orientation per connected component, and scores every non-forest edge
as an explicit rotation-cycle witness.

The crate intentionally does **not** produce camera centers. Pairwise essential geometry supplies a
translation direction but not an independently comparable baseline for every edge. Until scale and
translation synchronization are separately solved, the only truthful translation state is
`direction_only_scale_underdetermined`.

This is a reference oracle, not bundle adjustment. A cycle-consistent orientation generation is a
candidate basis for later robust global optimization, not final trajectory or metric authority.
