# `fdgr-graph`

`fdgr-graph` is FDGR's deterministic safe-Rust graph-topology oracle. It turns exact node and edge
evidence into a canonical maximum-priority spanning forest, connected components, fundamental-cycle
witnesses, bridge edges, and an explicit operation receipt.

The reference implementation is deliberately simple and dependency-free. It provides the semantic
oracle against which optimized `franken_networkx` and `frankengraphdb` paths must prove equality.

It does not infer geometry, optimize poses, assign physical coordinates, or turn edge priority into
a confidence probability. A graph edge is admitted evidence supplied by a higher-level domain.
