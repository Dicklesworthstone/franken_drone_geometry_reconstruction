# Local Qualification and Release Authority

**Status:** normative
**Release orchestrator:** [`doodlestein_self_releaser`](https://github.com/Dicklesworthstone/doodlestein_self_releaser)
**Portable workflow description:** `.github/workflows/ci.yml`
**Machine job graph:** `release/doodlestein_job_graph.json`
**Canonical local entrypoint:** `scripts/qualify.sh`

A GitHub status badge is not evidence that FDGR is releasable. GitHub-hosted runner availability,
cache state, action revisions, queue policy, and opaque host images are outside the trust model.
Workflow YAML is a portable job description for Doodlestein or another controlled local executor.
Only retained local receipts over exact identities have promotion authority.

## Release identity equation

```text
clean FDGR commit and tree
+ exact clean production-admitted sibling commits and trees
+ exact dated nightly and target
+ locked offline dependency closure
+ native host and qualification-lane identities
+ successful predecessor receipts
+ artifact hashes and semantic smoke receipts
= one candidate release identity
```

Changing any term creates a different candidate.

## Canonical qualifier

### Static contract qualification

```bash
./scripts/qualify.sh --mode static
```

This checks generated traceability and Beads data, the closed dependency universe, all repository
and agent contracts, JSON/TOML and Markdown integrity, Python source compilation, shell syntax, and
diff hygiene. Static success is useful design evidence but is not native or release evidence.

### Full native qualification

```bash
./scripts/qualify.sh --mode full
```

This adds exact `rustc`/`cargo` identity output, formatting, complete workspace checks, Clippy with
warnings denied, and all workspace tests under `rust-toolchain.toml`. It qualifies only the
executed host, target, features, source, and input identity.

### Release-candidate identity sealing

```bash
./scripts/qualify.sh --mode release \
  --sibling-root /path/to/exact/sibling/checkouts \
  --receipt-out qualification/fdgr-local-qualification.json
```

Release mode requires a clean FDGR checkout, runs the full native lane, verifies every sibling
currently marked `production_admitted = true` in `release/source_closure.lock.json`, and invokes
`scripts/emit_local_qualification_receipt.py`. That final receipt binds source, contract, closure,
toolchain, and host identities. It does **not** independently assert that preceding jobs passed;
Doodlestein's retained predecessor receipts provide that authority.

To audit all research snapshots, rather than only production-admitted siblings:

```bash
python3 scripts/verify_source_closure.py \
  --sibling-root /path/to/checkouts \
  --all-planned \
  --out qualification/source-closure.json
```

Source acquisition and clean-snapshot materialization belong to Doodlestein. FDGR qualification
never downloads or silently updates sibling source.

## Job DAG and resumption

`release/doodlestein_job_graph.json` defines the local DAG:

```text
repo-policy
  ├── dependency-policy
  └── generated-contracts
          ↓
       rust-core
          ↓
    agent-contract
          ↓
    agent-scenarios
          ↓
       promotion identity seal
```

Doodlestein may retain a successful node from a failed run only when its resumption key still
matches the FDGR tree, source-closure root, toolchain, contract root, host profile, and input roots.
A partial graph is a cache of evidence, never a release.

## Qualification lanes

`architecture/qualification_lanes.toml` keeps claims separate. Repository policy, dependency
closure, runtime semantics, evidence publication, MVCC, graph algorithms, media codecs, synthetic
and measured geometry, semantic coverage, archive restore, DJI profiles, MCP, packaging,
performance, agent contracts, agent ergonomics, and agent accretion earn distinct receipts. A
release manifest may claim only the dimensions whose required lanes ran and passed.

## Workflow YAML policy

The checked-in workflow:

- requests only `[self-hosted, fdgr-local]`;
- uses no third-party GitHub Action;
- assumes the exact checkout has already been positioned by the local orchestrator;
- invokes only `./scripts/qualify.sh`;
- carries no independent promotion semantics.

If workflow YAML and `scripts/qualify.sh` differ, the script plus normative registries win and the
workflow is repaired.

## Secrets, private property data, and publication

DJI credentials, object-store credentials, signing material, home imagery, private coordinates,
and device identifiers enter through narrow local capabilities. They are excluded or redacted
from public logs, crashpacks, context packs, benchmark corpora, and artifacts. Hardware receipts
retain reproducibility through private manifests and pseudonymous public projections.

Published artifacts require separate build, installer-smoke, checksum/signature, readback, and
redaction receipts. The identity sealer does not create a release manifest and cannot bless an
artifact by itself.
