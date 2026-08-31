# Immutable Object Graph Format

FDGR owns a canonical durable format independent of filesystems and cloud providers.

## 1. Object identity

```text
ObjectId = domain_separated_digest(canonical_header || payload)
```

The header names schema/version, object kind, codec, uncompressed length, privacy class, and child-reference summary. Identity never depends on provider key, pathname, upload ID, or database row number.

## 2. Object classes

- raw source ranges and acquisition metadata;
- packet/frame/telemetry runs;
- calibration, clock, scale, and transform objects;
- constraint and pose blocks;
- spatial geometry tiles and topology;
- semantic observations, claims, and scene graph runs;
- coverage and visibility data;
- question, plan, obligation, episode, and handoff capsules;
- search/context generations;
- reports and exports;
- transfer, repair, qualification, and restore receipts;
- manifests and roots.

## 3. Graph closure

A manifest lists typed child identities, ordering, sizes, required/optional status, schema roots, and semantic high-water marks. A root is publishable only when every required child is verified and graph traversal is closed under the declared policy.

## 4. Root-last publication

```text
reserve generation identity
→ materialize immutable objects in staging
→ verify bytes, schema, child closure, and semantic predicates
→ durably persist required objects
→ atomically publish the root pointer
```

Receivers use the same protocol. Transfer authority never carries mutation authority.

## 5. Canonical codec

Durable identity uses an FDGR-owned canonical binary codec with explicit field tags/order, integer widths, finite floating-point policy or normalized numeric representation, deterministic maps/sets, unknown-field policy, and golden vectors. JSON is an exchange/debug representation, not the identity codec.

## 6. Partial materialization

Large generations are chunked into independently verifiable immutable blocks. Readers pin a root and may lazily materialize children while preserving exact identity and declared completeness. A missing optional child produces a degraded projection; a missing required child blocks publication or read qualification.

## 7. Repair and transfer

ATP, provider multipart upload, deduplication, compression, encryption envelopes, and RaptorQ repair material wrap or move objects without changing their plaintext semantic identity. Retrieval verifies independent digests before promotion.

## 8. Garbage collection

Retention is reachability over published roots plus legal holds, active obligations, branch roots, and policy pins. Deletion is a separately sealed mark-and-sweep plan with dry run, generation fence, last-original protection, and post-delete audit.
