# Exact Repository Forensics Snapshot

**Captured:** 2026-08-31
**Authority:** design-research evidence only

The second and third FDGR architecture passes were based on exact public repository heads rather
than project names or thematic summaries. A source row establishes what was inspected; it does not
admit that repository into the FDGR production dependency graph, prove its current branch clean on
a builder, or promote every advertised feature as implemented.

| Repository | Commit | Tree | FDGR role under investigation | Analysis |
|---|---|---|---|---|
| `asupersync` | `7b6ff6e95685f2d614eb418e8f026033dee4317a` | `e028dff7ae88f49215566b8cf422ec2398deb450` | exclusive runtime, capability context, deterministic laboratory, ATP and RaptorQ | [deep dive](../deep-dives/01_ASUPERSYNC.md) |
| `frankensqlite` | `2d8a68b9ad82d685f8bacd9d5fe3c8fe5304a0e4` | `ca7a3fb70a9f590bc8bd65ff5069b915afe0201f` | MVCC ledger, witnesses, deterministic commit, recovery | [deep dive](../deep-dives/02_FRANKENSQLITE.md) |
| `frankenfs` | `151ea2dabb37c26d4f21e1369e409b1a348ca00b` | `476cdc0083f122bf2d7a8f6394d9ff4e4b51d01d` | custody, staged-visible-durable publication, repair and crash evidence | [deep dive](../deep-dives/03_FRANKENFS.md) |
| `frankensearch` | `de1dbc4b97ab55481e3605a2302e8a560f3e8248` | `4c2d6d9b7ef55bacc50f7948a0d06bcc938552f3` | progressive retrieval, explanations, immutable search generations | [deep dive](../deep-dives/04_FRANKENSEARCH.md) |
| `franken_markdown` | `d5029f558d4193a2b6c7607d1d7aef3ff44975b1` | `04f2ab20e9e7b7f84c28811e3597d370ff7dd38e` | deterministic exact reports, schemas and agent-readable documents | [deep dive](../deep-dives/05_FRANKEN_MARKDOWN.md) |
| `frankengraphdb` | `a280bda6b904498ec60f9e0a4e9bcbbd667691e1` | `dc8214d26a24e725f00115a0beb37af9096469fd` | one version universe, branches, graph storage, incremental projections | [deep dive](../deep-dives/06_FRANKENGRAPHDB.md) |
| `franken_networkx` | `f3b2a3872dcebcc29155c483543aa6e4ef6b6663` | `ee6c1262cf7eb5d13567a6221f19afcf777b736e` | deterministic graph algorithms, tie-break and complexity witnesses | [deep dive](../deep-dives/07_FRANKEN_NETWORKX.md) |
| `dwarf_fortress_mcp` | `30db7c5aa9f058818a6814abb1a51ca63a09e75a` | `7a699fe88c55a540b67776af42f2dfcd2be94ce0` | agent operating loop, witnessed effects, semantic narrow waist and handoff | [deep dive](../deep-dives/08_DWARF_FORTRESS_MCP.md) |
| `fastmcp_rust` | `db1c17667faa27baeb5fa6208978f1a3e86e224d` | `d2ac16ad956c170008242c0513d6b0947fe2ccd5` | replaceable MCP transport and lifecycle presentation | [deep dive](../deep-dives/09_FASTMCP_RUST.md) |
| `eidetic_engine_cli` | `386157ecd3b14f92338d7b39d2a8d956ff0db563` | `5e086352a42fc9099c11a90fc0630f7e02b90873` | external advisory memory, context packs, outcome feedback and ergonomics | [deep dive](../deep-dives/10_EIDETIC_ENGINE.md) |
| `doodlestein_self_releaser` | `871758b7cf640ffe25f44675acb58d2c9f69e41f` | `f293d5f51eee72a5dc7ed8fe85b7755fcc6e6db0` | local release qualification and exact-source artifact authority | [deep dive](../deep-dives/11_DOODLESTEIN_SELF_RELEASER.md) |

## Method

For each repository the investigation identified concrete files and mechanisms, the invariant each
mechanism establishes, the FDGR semantic owner, failure boundary, reference model, superficial
imitation to reject, and admission evidence. The locus map is
[`SYMBOL_LOCUS_INDEX.md`](SYMBOL_LOCUS_INDEX.md); machine identities are in
[`source_manifest.json`](source_manifest.json).

## Non-claim

These identities are a reproducible research basis. A real FDGR release must additionally capture
an exact clean local source closure, lockfile, features, toolchain, host profile, qualification
receipts, licenses, and artifact readback through Doodlestein.
