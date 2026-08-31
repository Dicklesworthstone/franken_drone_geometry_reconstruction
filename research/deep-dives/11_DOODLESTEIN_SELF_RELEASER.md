# Deep Dive 11 — `doodlestein_self_releaser`

**Import decision:** local qualification and release evidence are constitutional
**FDGR authority:** the only authority for build/test/qualification/promotion claims

## Why release machinery belongs in the architecture

FDGR depends on exact nightly behavior, owned sibling revisions, native filesystem/network/device
semantics, local GPUs or CPU profiles, physical DJI compatibility, large evidence corpora, and
multi-machine workloads. Hosted CI cannot be the trust root for that system, even when a workflow
happens to run successfully.

The Doodlestein import is:

```text
one clean source identity
+ one exact owned dependency closure
+ one qualification contract
+ native local execution per target/profile
+ deterministic evidence receipts
= a releasable artifact set
```

GitHub workflow YAML is retained as a portable executable job graph and documentation surface. It
is run locally through Doodlestein/self-hosted machinery. GitHub-hosted runner availability or
success is never required evidence and never overrides a failing local receipt.

## Mechanism 1 — clean source snapshot, never the mutable checkout

A qualification/release run begins from a clean committed FDGR source identity. The release system
materializes an isolated snapshot containing:

- repository commit and tree digest;
- submodule state if any;
- Cargo lock and toolchain files;
- generated-artifact drift state;
- exact owned sibling source trees at clean revisions;
- dependency allowlist and transitive closure;
- qualification contract and workflow identities;
- source-date and reproducibility inputs.

Untracked files, dirty sibling changes, editor caches, local path substitutions, and later branch
movement cannot leak into the attributed artifact. Development qualification may optionally test a
dirty checkout, but its receipt is visibly non-promotable.

## Mechanism 2 — exact owned-sibling closure

Production dependencies on `asupersync` and admitted Franken projects are not floating Git URLs at
release time. The snapshot records and materializes the exact clean source of each sibling. Cargo
resolution is locked and checked offline against this closure.

The source-closure manifest records:

- repository and revision;
- tree/content digest;
- license and policy identity;
- feature set and caller crates;
- dependency edges;
- generated-code identity;
- local patches (normally forbidden for promotable runs);
- admission/qualification receipt required by FDGR.

This prevents an FDGR binary being attributed to commit X while silently incorporating dirty or
newer sibling code.

## Mechanism 3 — one qualification contract

The repository has one direct local qualifier, `scripts/qualify.sh`, whose stages are data-driven
by the machine registry. Workflow jobs invoke the same stages rather than duplicating commands.
The contract includes at least:

1. repository and generated-artifact validation;
2. dependency/source-closure policy;
3. formatting;
4. build/check for all targets/features in the profile;
5. clippy/lint policy;
6. unit/integration/doc tests;
7. deterministic/replay tests;
8. fault/crash/cancellation lanes;
9. differential and metamorphic corpora;
10. performance gates where the host profile is qualified;
11. packaging and installer smoke tests;
12. artifact/manifest/checksum/provenance verification.

Each stage emits a versioned receipt, not just console text. The overall receipt is a root over
stage receipts and artifacts.

## Mechanism 4 — workflow YAML as portable job graph

`.github/workflows/*.yml` may describe matrices, dependencies, environment, and commands. Its
`runs-on` labels express required local host capabilities. Doodlestein executes it on the user's
machines or translates it into controlled local jobs.

Rules:

- no workflow step assumes GitHub-hosted secrets/services;
- no correctness gate depends on GitHub API availability;
- caches are accelerators and never evidence;
- artifact upload actions are optional mirrors, not the authoritative artifact store;
- release publication consumes locally sealed artifacts/receipts;
- workflows remain usable specifications even when GitHub Actions is disabled.

## Mechanism 5 — host and lane manifests

Every native receipt names:

- host ID/pseudonymous worker identity;
- OS/kernel/filesystem;
- architecture and CPU features;
- memory and storage profile;
- GPU/accelerator and driver where relevant;
- Rust toolchain digest/version;
- environment whitelist;
- target triple and linker/profile;
- connected DJI fixture/device profile for hardware lanes;
- model and corpus roots;
- start/end times from the controlled runner;
- thermal/power constraints for performance lanes.

A test on Linux x86_64 does not qualify macOS arm64. A simulator test does not qualify a physical
DJI profile. A release matrix shows exactly which dimensions earned evidence.

## Mechanism 6 — resumable work without blessing partial release

Long multi-platform runs fail. Doodlestein retains completed target artifacts and receipts keyed by
full source/closure/contract/host identity. A resumed run can reuse an exact matching successful
stage.

Partial artifacts are useful but never promoted as a release set. The authoritative release
manifest is withheld until every required lane is complete. Optional lanes are labeled and cannot
silently become required or vice versa.

A resumed receipt records reused versus newly executed stages and verifies all referenced roots.

## Mechanism 7 — deterministic artifact identity and promotion

Packaging is deterministic where the format/platform permits. The release manifest includes:

- source and sibling closure roots;
- qualification-contract root;
- target/profile/feature set;
- binary/archive/SBOM/checksum identities;
- installer and smoke-test receipts;
- provenance/signature bundles where configured;
- required runtime/model/data compatibility;
- known limitations and unearned lanes.

Promotion is a root transition. Uploading files is not promotion. Public release metadata is
created only from the sealed local manifest, and post-upload readback verifies that published
assets match it.

## Mechanism 8 — local evidence for physical and large-corpus lanes

Some FDGR gates cannot run on ordinary CI:

- physical DJI passive ingest and command reconciliation;
- long live-capture spool tests;
- multi-terabyte archive restore;
- real filesystem kill/power-loss matrices;
- local model inference and GPU/CPU benchmarks;
- measured-room geometry campaigns;
- multi-machine ATP/RaptorQ donor-loss tests;
- hours-long deterministic/fuzz/soak campaigns.

Doodlestein schedules these on labeled local resources, retains evidence bundles, and links them to
source/contract identity. Small PR checks are informative; they cannot substitute for the required
native lane.

## Mechanism 9 — same-binary experiments and benchmark governance

Performance lanes compare runtime-selected arms inside one qualified binary. The runner first
checks semantic output equality, then collects A/A and A/B distributions under a fixed workload
manifest. Receipts include raw samples, warmup, cache state, host load, thermal state, and analysis
policy.

A benchmark regression or win is not copied manually into README prose. Registered claims pull from
accepted local receipts. Hardware changes create new baselines rather than silently shifting one.

## Mechanism 10 — secret and credential boundaries

DJI credentials, object-store keys, signing keys, and private property data are injected by the
local runner as least-authority capabilities. They are absent from source snapshots, logs,
crashpacks, caches, and public artifacts. Redaction scans run before publication.

Hardware qualification receipts use pseudonymous/sanitized device and property identities while
retaining enough profile detail to reproduce privately.

## Mechanism 11 — offline and supply-chain posture

Promotable builds operate with network disabled after the source/model/toolchain closure is
materialized. Build scripts cannot download. Missing dependency/model data is a hard failure.
External model weights are immutable data artifacts with their own license, checksum, producer,
and admission manifests.

Installers verify checksums and, where configured, signatures/provenance before replacement. They
smoke-test exact version and basic semantic behavior. A missing verification tool follows an
explicit profile; it never silently reports stronger authenticity.

## Mechanism 12 — release evidence is append-only and auditable

A release run produces:

```text
RunRoot
├── SourceClosureReceipt
├── ToolchainReceipt
├── DependencyPolicyReceipt
├── QualificationLaneReceipts...
├── ArtifactBuildReceipts...
├── PackagingReceipts...
├── InstallerSmokeReceipts...
├── PerformanceReceipts...
└── PromotionManifest
```

Superseded or failed runs remain available as negative evidence. The system never edits an old
receipt to make a later release look clean.

## FDGR repository changes required by this import

- `LOCAL_QUALIFICATION_AND_RELEASE.md` is normative.
- `scripts/qualify.sh` is the semantic source of truth.
- `architecture/qualification_lanes.toml` declares lane dependencies and authority.
- `architecture/dependency_allowlist.toml` declares the closed universe.
- Doodlestein materializes exact clean source snapshots outside the mutable checkout.
- `scripts/verify_source_closure.py` proves offline resolution.
- `scripts/emit_local_qualification_receipt.py` seals exact source, contract, closure, toolchain, and host identities after predecessor receipts succeed.
- `.github/workflows/ci.yml` invokes the same local qualifier and uses self-hosted labels;
  it is not itself evidence.
- `QUALIFICATION.md` distinguishes executed evidence from intended gates.

## Superficial imports rejected

- hosted Actions badge as release authority;
- building from the developer's mutable checkout;
- floating sibling branches;
- a lockfile without materialized source closure;
- duplicated shell commands in workflows and local scripts;
- retaining a partial matrix and labeling it a release;
- performance numbers without host/workload/raw samples;
- caches treated as proof;
- build-time network access;
- secrets or home imagery in logs/artifacts;
- upload success treated as public-asset verification;
- source presence or compilation treated as hardware compatibility.

## FDGR admission gate

1. Clean snapshot and sibling closure reproduce offline.
2. All required lanes are declared in a DAG with stable identities.
3. Direct qualifier and workflow jobs call the same stage commands.
4. Hosted GitHub status has no authority in promotion logic.
5. Every artifact traces to source, closure, toolchain, host, and lane receipts.
6. Resumption reuses only exact-identity stages and never blesses partial sets.
7. Physical/device/model/performance claims cite native local receipts.
8. Installers and published assets verify against the sealed manifest.
9. Secret/redaction scans pass before publication.
10. A clean-room rebuild of a representative release matches the declared reproducibility contract.

---

## Agent-native synthesis

### Agent ergonomics as a release lane

Doodlestein qualification includes cold arrival, heartbeat economy, gap/reset, budget truncation, unavailable affordances, indeterminate effects, pilot guidance, handoff, multi-agent races, and accretion rollback. Workflow YAML mirrors these local native lanes and has no independent authority.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
