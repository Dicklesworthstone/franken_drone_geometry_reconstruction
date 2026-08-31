# Deep Dive 05 — `franken_markdown`

**Import decision:** import the bounded-parser, exact-span, deterministic-publication, and small-audit-surface doctrines
**FDGR authority:** protocol grammars, knowledge provenance, diagnostic/report compilation, and canonical textual artifacts

## Why a Markdown renderer matters to drone reconstruction

The obvious connection is reports. The deeper connection is that FDGR must parse and publish many
untrusted, version-sensitive languages while retaining exact provenance:

- DJI protocol captures and inferred grammar annotations;
- H.264/H.265 NAL and MP4/MOV container structures;
- telemetry schemas;
- calibration manifests;
- model cards and licenses;
- flight procedures and property notes;
- semantic ontologies;
- evidence and qualification reports;
- agent-facing explanations;
- deterministic HTML/PDF/Markdown exports.

`franken_markdown` demonstrates how to own a complete parsing/rendering path, keep the core small,
preserve spans and deterministic bytes, treat WASM/host effects as explicit boundaries, and
publish sibling outputs transactionally. Those are directly transferable design techniques.

## Mechanism 1 — exact bytes and source spans

Every parsed textual or protocol artifact retains:

- source object identity;
- exact byte range;
- parser/grammar policy identity;
- decoded token/node identity;
- normalization steps;
- diagnostics and recovery decisions;
- parent/child and transformation lineage.

For DJI reverse engineering, a field hypothesis such as “bytes 18–21 are little-endian device
clock ticks” must cite exact packets, offsets, experiments, and grammar version. For model cards or
manuals, a semantic claim cites exact source spans. Normalized text never replaces original bytes.

A citation survives incremental updates by naming content identity and span in that generation,
not a mutable line number alone.

## Mechanism 2 — arena-based, bounded, nonrecursive parsing

FDGR treats all external structure as adversarial or accidental-malformation capable. Parsers for
MCP JSON, model manifests, archive manifests, container boxes, packet TLVs, telemetry, and
knowledge documents use:

- typed arenas and compact handles;
- explicit work stacks rather than unbounded recursion;
- byte, nesting, node, member, string, array, diagnostic, and output limits;
- checked arithmetic and offset ranges;
- total-state transitions with typed errors;
- deterministic recovery only where a hardened mode explicitly permits it;
- no hidden filesystem, network, clock, or process effects in the parse core.

Strict mode is authoritative. Recovery mode is diagnostic and emits a decision record; recovered
content cannot silently become control authority or canonical calibration.

## Mechanism 3 — clean-room grammar ownership

The core does not permanently delegate semantic parsing to a giant dependency stack. FDGR owns
small purpose-built grammars for:

- evidence manifests and durable encodings;
- DJI profile state machines and packet layouts;
- MP4/MOV boxes and elementary-stream boundaries;
- camera/telemetry metadata;
- internal query and expression languages;
- scene ontology declarations;
- qualification and release receipts.

Generated parser tables, if used, are checked in and produced by an owned deterministic generator.
Build scripts perform no network access. Durable formats are not defined by `serde` enum layout or
compiler implementation details; canonical encoders/decoders are explicit and versioned.

External parsers may serve as differential oracles in qualification processes. They do not define
production truth.

## Mechanism 4 — restartable incremental parsing

Large capture manifests, logs, protocol corpora, and project journals change incrementally. FDGR
uses restart points and bounded invalidation:

1. preserve token/node boundaries and lexical state;
2. locate the earliest affected restart point;
3. reparse forward until state and output converge with the prior generation;
4. reuse unaffected arena ranges by immutable reference;
5. prove incremental/full equivalence over a corpus;
6. publish the successor root atomically.

This pattern also informs native bitstream indexing: appending a media segment should not require
rescanning terabytes when a verified parser state can resume at a segment boundary.

## Mechanism 5 — diagnostics are structured evidence

A parser diagnostic has stable identity and fields:

- source digest and span;
- grammar and rule;
- severity and recoverability;
- expected/observed class;
- context bounded by policy;
- decision/recovery path;
- related spans;
- remediation hint;
- output eligibility impact.

Human caret displays are projections of that object. Agent JSON, event streams, HTML, and PDF all
refer to the same diagnosis. Unbounded raw packet or home-description data is never dumped into an
error message.

## Mechanism 6 — deterministic single semantic model, multiple outputs

FDGR reports and exports parse one typed evidence/report AST and render:

- Markdown;
- self-contained HTML;
- compact tagged PDF;
- JSON/JSONL machine records;
- optional browser/WASM views.

The outputs share sections, identifiers, citations, tables, diagrams, and numeric formatting.
There is no separate “pretty report” pipeline that can drift from machine facts. Given fixed
source roots, policy, fonts/assets, and `SOURCE_DATE_EPOCH`, outputs are byte-stable where the
format contract promises stability.

Floor plans, coverage maps, uncertainty legends, graph diagrams, and reconstruction summaries are
generated from named scene artifacts. A report image cannot be mistaken for authoritative
geometry because it cites the underlying generation and rendering recipe.

## Mechanism 7 — all-or-nothing sibling publication

A reconstruction report release may contain:

```text
report.md
report.html
report.pdf
report.json
scene.glb
floorplan.svg
coverage.svg
citations.json
checksums.txt
qualification-receipt.json
```

These outputs are preflighted, staged, rendered, validated, and published as one sibling set under
a manifest root. If a late PDF or scene validation fails, prior public outputs remain active and
the staged set is quarantined or removed. The system never leaves a new report pointing at an old
scene or a new scene accompanied by stale caveats.

## Mechanism 8 — taint and authority separation

Text from:

- in-scene signs or screens;
- owner notes;
- DJI/model documentation;
- packet strings;
- model-generated captions;
- imported web pages;
- agent memory;

is untrusted content. It may become evidence with provenance. It cannot grant a capability,
select a device effect, weaken a validation gate, or execute code. Prompt-like text remains tainted
through chunking, retrieval, summarization, and report rendering.

A model card claiming metric accuracy is not a scale witness. A manual saying a command succeeded
is not an operation receipt. A generated caption is not a resolved semantic asset.

## Mechanism 9 — compact, auditable render and codec primitives

The project demonstrates a strategy FDGR adopts for many “surely use a library” problems:

- implement only the required language/format surface;
- keep hot loops allocation-conscious and cache-friendly;
- preserve a scalar/simple reference path;
- expand coverage from fixtures and real demands;
- reject unsupported constructs honestly;
- measure before adding generality;
- keep external assets host-supplied and identity-checked.

This informs native metadata parsers, canonical JSON-like encoders, SVG floorplan output, compact
PDF report generation, and eventually media/container components. FDGR is not building a browser
or universal codec suite; it owns the exact surfaces its product contract requires.

## Mechanism 10 — browser/WASM parity without ambient host effects

A pure core accepts bytes and options and returns bytes, diagnostics, and receipts. Native hosts
supply files, network fetches, fonts, images, clocks, and parallel orchestration. This makes the
same evidence viewer/report renderer usable in:

- the CLI;
- a local server;
- browser/WASM review;
- deterministic qualification;
- offline support bundles.

Native and WASM paths share canonical semantics. Host convenience cannot create a second parser or
alter digest identity.

## Mechanism 11 — protocol-documentation co-generation

FDGR's protocol/profile laboratory generates from one grammar model:

- bounded parser/encoder tables;
- packet-field reference documentation;
- Wireshark-like human decode views (without requiring Wireshark at runtime);
- fuzz dictionaries and structured generators;
- golden vectors;
- compatibility-profile schemas;
- redaction rules;
- change reports between firmware/profile epochs.

The grammar source itself carries exact citations to capture experiments. Generated artifacts have
source hashes and drift checks. Manual edits to generated surfaces fail qualification.

## Mechanism 12 — quantitative claims linked to checked artifacts

Report prose does not hand-maintain fast-changing counts or benchmark numbers. Quantitative claims
are generated or checked against machine manifests. A badge or README sentence claiming a number
of qualified device profiles, geometry benchmarks, test families, or supported export features
must trace to the exact registry and evidence bundle.

Current status and target-state prose remain separate. The prospective README may describe the
finished system only if prominently labeled as target architecture and mechanically trued up as
gates land.

## Performance implications

- typed arenas avoid pointer-heavy ASTs and repeated allocation;
- interned identifiers and compact spans reduce report/index memory;
- one parse feeds all outputs;
- incremental reparsing limits work on large evolving corpora;
- deterministic asset subsetting avoids embedding unused resources;
- shared syntax/diagram/layout models reduce duplicated caches;
- exact canonical writers avoid post-processing toolchains.

Performance optimizations are validated against golden byte, structure, citation, and visual
fixtures. Faster wrong spans are disqualifying.

## Superficial imports rejected

- copying report aesthetics while keeping two drifting data models;
- parsing untrusted nested formats recursively without bounds;
- throwing away original bytes after normalization;
- citing a mutable URL or line number without content identity;
- allowing recovery-mode parse output to authorize an effect;
- using `serde` layout as a durable wire/storage specification;
- invoking a headless browser or LaTeX as an undeclared permanent core dependency;
- writing report siblings one by one into the public destination;
- allowing model-generated text to become canonical semantics;
- network access from `build.rs`.

## FDGR admission gate

1. Parsers have explicit limits and no unbounded recursion.
2. Exact bytes and spans round-trip over canonical corpora.
3. Incremental and full parses are equivalent.
4. Malformed/deep/huge inputs fail with bounded diagnostics and resources.
5. Taint survives retrieval and rendering and cannot grant authority.
6. Durable formats have explicit canonical encoders independent of derive layout.
7. Multi-output publication is all-or-nothing under kill/failure injection.
8. HTML/PDF/Markdown/JSON cite the same evidence objects and scene roots.
9. Generated grammar/docs/tests pass drift checks.
10. External render/parser tools remain qualification oracles behind process boundaries.

---

## Agent-native synthesis

### Self-description and proof-carrying reports

Agent docs, help, schemas, context packs, handoffs, decision cards, and evidence reports share stable source spans and all-or-nothing publication. Human prose never becomes the only protocol contract; generated documentation is parity-tested against machine registries.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
