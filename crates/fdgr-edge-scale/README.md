# `fdgr-edge-scale`

`fdgr-edge-scale` is the deterministic safe-Rust reference boundary that reconciles the otherwise arbitrary baseline magnitudes of admitted pairwise relative-pose edges.

It provides:

- exact pose-graph, subject-table, witness-table, policy, and generation identities;
- one subject per exact admitted relative-pose edge;
- correlation-group collapse before consensus so dependent evidence cannot impersonate independent votes;
- whole-group rejection when a dependence class is internally contradictory;
- deterministic robust ratio consensus with retained supporting and rejected witnesses;
- one arbitrary relative baseline gauge per connected scale component;
- fundamental ratio-cycle evidence for every redundant relation;
- explicit isolated, tree-only, cycle-consistent, and conflicted component states;
- refusal to join evidence across disconnected pose components;
- deterministic replay, canonical bytes, JSON, and operation-cost separation.

The output unit is `component_edge_scale_unit`. It is not meters. Disconnected scale components remain incomparable, and a reconciled edge scale is not an optimized camera position, bundle-adjusted trajectory, or metric reconstruction.
