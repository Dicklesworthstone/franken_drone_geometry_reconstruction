# Deep Dive 09 — `fastmcp_rust`

**Import decision:** admit as the thin, pinned, replaceable MCP presentation plane after profile qualification
**FDGR authority:** none below transport/session presentation

## The correct boundary

MCP framing, JSON-RPC identity, stdio/HTTP session lifecycle, cancellation routing, pagination, and
task projection are necessary but not the FDGR thesis. The thesis is evidence custody,
reconstruction semantics, witnessed plans, device obligations, and proof-backed completion.

`fastmcp_rust` is useful precisely because it shares the asupersync cancellation and capability
model while remaining separable from the semantic core. FDGR borrows transport plumbing and owns
every consequential meaning.

## Admitted profile

The exact profile is machine-registered and pinned to a clean source revision. The intended rules
are:

- `fastmcp-rust` facade only through `fdgr-mcp`;
- exact git/source-closure revision, no floating branch;
- no second async runtime;
- modern protocol profile selected explicitly;
- only required features enabled;
- remote/auth/proxy/legacy/experimental features forbidden unless separately admitted;
- no `fastmcp` type crosses into evidence, geometry, device, storage, or query crates;
- transport can be replaced without changing any stable FDGR semantic schema.

Protocol conformance is a gate on the pinned profile, not inferred from version constants, source
presence, or package publication.

## Handler context mapping

Each incoming request creates a request-owned asupersync child region. `McpContext` is converted at
the seam into an FDGR request context carrying:

- session and request identity;
- negotiated protocol and FDGR semantic versions;
- property/device/capture scope;
- capabilities and caveats;
- evidence freshness/anchor constraints;
- time, CPU, memory, I/O, output, model, and retry budgets;
- cancellation chain and trace/replay identity.

The transport cannot add capability merely because a connection is authenticated. Application
admission and per-operation authority are owned below the seam.

## Cancellation and tasks

Wire cancellation requests FDGR cancellation; it does not drop semantic work. The response can
project:

- cancellation accepted;
- drain/reconciliation progress;
- unresolved external effects;
- terminal `Cancelled`, `Failed`, or `Indeterminate` outcome;
- evidence continuation.

Long-running capture, reconstruction, archive, export, and plan operations map to application-owned
MCP Tasks only after the exact Tasks lifecycle is qualified. The task store is a projection over
FDGR obligations, not an independent source of truth.

Transport shutdown must continue receiving/routing cancellation where the admitted platform path
supports it. Platform-specific limitations are recorded rather than papered over by aggregate
claims.

## Stable narrow tool mapping

Logical names retain semantic identity. Wire-safe names may replace dots with underscores, but
schemas, registries, evidence, and documentation use logical names. A generated mapping prevents
drift.

Tools return stable versioned envelopes:

```text
schema
requestId
sessionId
anchor
status / outcome
result or typed error
completeness
degraded[]
evidence[]
continuation?
nextCommands[]
```

Human prose is content inside the envelope, not the protocol contract.

## Large artifacts and streaming

Point clouds, meshes, packet traces, evidence bundles, and model artifacts are not embedded in huge
MCP JSON responses. The MCP layer returns sealed resource identities, metadata, bounded previews,
and authorized resource URIs/streams. Transfer itself uses FDGR custody/ATP or a bounded local
resource path.

Progress notifications are monotonic events derived from obligation state. Backpressure and
output budgets apply. A disconnected MCP client does not orphan the underlying operation; session
policy decides whether to continue, drain, or checkpoint.

## Error and outcome mapping

FDGR preserves distinctions among:

- invalid request/schema;
- capability denied;
- stale/conflicting anchor;
- budget exhausted with partial result;
- compatibility unavailable/degraded;
- expected domain error;
- cancellation requested/in progress/terminal;
- panic/internal invariant violation;
- indeterminate external effect.

The transport maps these deterministically to protocol errors/results without flattening their
retry semantics. Sensitive context is redacted at the seam.

## Cache and session isolation

Transport-level response caching is admitted only for explicitly pure, anchor-pinned,
capability-projected reads. Keys include committed auth/capability facts, opaque session scope,
request parameters, evidence generation, schema, policy, and output profile. Mutation, local-only
state, uncommitted auth, allocation failure, or changing state fails cache admission closed.

No response cache can make a stale result look current or share a private home projection across
sessions.

## Replaceability test

The semantic test suite runs through:

1. a direct Rust service facade;
2. an in-memory reference transport;
3. the pinned `fastmcp_rust` adapter.

All three produce the same canonical semantic envelopes after removing transport-specific
framing. This proves the MCP crate is a presentation waist rather than an architectural root.

## Security posture

Initial profiles are local stdio and localhost-first. Remote transport is not promoted until an
explicit admission design covers:

- transport-boundary identity;
- capability issuance/revocation;
- CSRF/origin/session fixation where applicable;
- TLS/key custody through an admitted safe boundary;
- rate/admission control;
- privacy and artifact authorization;
- complete cancellation and bidirectional lifecycle behavior.

No auth feature from the MCP framework is assumed safe merely because it exists.

## Qualification and dogfooding

FDGR records upstream gaps discovered during integration as reproducible tests and, when useful,
contributes fixes to `fastmcp_rust`. The pinned revision advances only after:

- local protocol conformance;
- stdio/selected transport lifecycle tests;
- concurrent request and cancellation campaigns;
- large-resource/output bounds;
- session/capability isolation;
- process shutdown and child cleanup;
- semantic replaceability differential;
- exact source-closure qualification through DSR.

## Superficial imports rejected

- allowing transport types into core crates;
- assuming protocol-version constants prove conformance;
- enabling a broad default feature set;
- relying on a second runtime hidden in an HTTP/auth feature;
- using framework task state as canonical operation state;
- huge mesh/trace blobs in tool responses;
- treating connection authentication as device authority;
- serializing all requests because it is easier while claiming concurrency;
- closing the transport and abandoning request-owned work;
- caching without full capability/generation partitioning.

## FDGR admission gate

1. Only `fdgr-mcp` depends on the framework.
2. Exact pinned profile passes local conformance.
3. Direct/reference/MCP semantic outputs are differential-equivalent.
4. Wire cancellation reaches request-owned regions and projects drain state honestly.
5. Task projection is backed by application obligations.
6. Large artifacts use bounded resources, not unbounded JSON.
7. Session, capability, cache, and privacy isolation pass adversarial tests.
8. Transport shutdown leaves no orphan work or unreported external effects.
9. Feature/dependency closure contains no second runtime or forbidden framework.
10. Replacement with the reference transport requires no core changes.

---

## Agent-native synthesis

### Presentation-only agent waist

FastMCP carries framing, schema discovery, cancellation routing, tasks, and progress. FDGR owns all semantics. The eleven logical operations and Agent Turn Packet remain identical over CLI, MCP, NDJSON, or tests, and no FastMCP type crosses into evidence, planning, geometry, or obligation crates.

**Admission consequence:** the integration is incomplete until this behavior is visible through the same Agent Turn Packet, exact anchor vector, four ledgers, typed references, recovery classes, and local agent acceptance scenarios as every other subsystem.
