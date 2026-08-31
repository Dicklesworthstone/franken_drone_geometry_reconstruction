# FDGR Closed Dependency Universe

**Status:** normative
**Machine contract:** `architecture/dependency_allowlist.toml`
**Enforcement:** `scripts/check_dependency_policy.py`
**Exact owned-source lock:** `release/source_closure.lock.json`

FDGR is not merely a Rust application that happens to have few dependencies. It is a deliberately closed systems stack whose correctness, determinism, cancellation behavior, durable formats, numerical policy, and release identity must remain inspectable end to end.

## Constitutional rule

The production trust domain may contain only:

1. `core`, `alloc`, `std`, and `proc_macro` from the exact pinned nightly;
2. `asupersync` as the exclusive asynchronous runtime, capability, laboratory, transfer, and repair substrate;
3. exact clean revisions of explicitly admitted Franken-suite projects;
4. rare fundamental serialization crates such as `serde`, admitted one at a time with default features disabled and a transitive-closure audit;
5. FDGR-owned crates.

Everything else is either rejected or quarantined behind a supervised process boundary as a non-authoritative oracle.

## What “closed” means

A dependency is not admitted merely because it is popular, written in Rust, or convenient. Admission requires all of the following:

- exact source identity and immutable revision;
- complete transitive source closure;
- semantic owner in the FDGR architecture;
- a statement of which invariant it establishes;
- cancellation, budget, and shutdown behavior;
- durable-format and schema impact;
- determinism and tie-break behavior;
- memory-safety and `unsafe` boundary audit;
- failure and degraded-mode semantics;
- differential or reference model;
- replacement or permanence decision;
- local qualification lane and retained evidence.

No branch, tag, floating semver range, downloaded binary without an authenticated manifest, or host-global library may silently enter a release.

## Exclusive runtime

`asupersync` is the only asynchronous runtime. Tokio, async-std, smol, Rayon, detached threads, and any second cancellation model are forbidden. A foreign library whose transitive closure introduces one of those runtimes is forbidden even if FDGR never calls it directly.

Every blocking or effectful path must be represented through FDGR/asupersync ownership:

```text
region owner
  → explicit Cx/capability
  → multidimensional budget
  → request/drain/reconcile/finalize cancellation
  → terminal receipt
```

## Memory safety

Every FDGR crate uses `unsafe_code = "forbid"`. There is no C/C++ FFI, in-process Python, linked FFmpeg, OpenCV, CUDA runtime binding, C SQLite, or vendor SDK inside the production process.

If an owned sibling eventually requires an audited low-level island, that island remains in the sibling’s ledgered boundary and must expose a safe, deterministic, bit-identical fallback. FDGR itself does not open a new unsafe escape hatch.

## External oracle quarantine

The following may exist during research, differential qualification, or migration:

- FFmpeg/ffprobe as a media conformance oracle;
- COLMAP or another established geometry system as a differential oracle;
- Python/PyTorch workers for current open-weight models;
- NetworkX as a graph differential oracle;
- DJI applications, exports, and documented SDK surfaces;
- platform packet capture and diagnostic tools.

They are **not** production authorities. They may not:

- define FDGR durable identities or formats;
- publish a canonical generation directly;
- grant device capabilities;
- bypass evidence validation;
- run in-process;
- become required for restore or historical replay;
- remain without a named native-Rust retirement or permanence gate.

An oracle emits a bounded candidate object plus exact producer identity. FDGR validates, witnesses, compares, and republishes that result under an FDGR generation root or rejects it.

## Fundamental exceptions

The initial registry exception set is intentionally tiny:

- `serde`
- `serde_json`
- `serde_bytes`

Their presence in the allowlist is permission to propose a dependency, not blanket admission. An actual manifest entry must pin an exact version, disable default features, enable only named features, and appear in the local source-closure receipt.

## Owned-project roles

| Project | FDGR role | Authority boundary |
|---|---|---|
| `asupersync` | runtime, capabilities, structured ownership, ATP, RaptorQ, lab | foundational |
| `frankensqlite` | evidence ledger and MVCC storage | authoritative persistence after admission |
| `frankenfs` | custody, checkpoints, repair, virtual evidence filesystem | authoritative custody after admission |
| `frankensearch` | progressive retrieval and attention | derived cognition only |
| `franken_markdown` | exact report/protocol/document generation | derived publication |
| `frankengraphdb` | temporal scene graph and incremental graph views | derived/authoritative split by registered relation class |
| `franken_networkx` | deterministic graph algorithms and certificates | derived cognition only |
| `fastmcp_rust` | MCP framing and lifecycle | presentation only; `fdgr-mcp` sole caller |
| `eidetic_engine_cli` | agent campaign memory | external advisory memory only |
| `doodlestein_self_releaser` | local qualification and release orchestration | release authority, outside product graph |

## Enforcement

Run:

```bash
python3 scripts/check_dependency_policy.py
```

The checker walks every workspace manifest, target-specific dependency table, lockfile package, and Rust source file. It rejects unapproved registry packages, floating Git dependencies, native links, second runtimes, C ABI declarations, in-process Python, and unsafe items.

The checker is necessary but not sufficient. Release qualification also verifies the exact clean owned-source closure from `release/source_closure.lock.json` and builds offline.
## Explicitly forbidden convenience stacks

The production Rust graph explicitly forbids `tokio`, `async-std`, `smol`, `rayon`, `reqwest`,
`hyper`, `axum`, `tower`, `sqlx`, `diesel`, `sea-orm`, `opencv`, `pyo3`, `rusqlite`, and `rocksdb`.
This list is duplicated in the machine policy so documentation drift fails qualification.
