# Context Packs and Pack DNA

A context pack is FDGR's bounded cognitive read model. It is not a transcript summary and not an unstructured retrieval dump. It is a deterministic, proof-carrying selection of the information with highest expected decision value for a named focus.

## 1. Inputs

A pack is keyed by:

```text
anchor vector
session and grants
mission/objective/question focus
profile
allowed epistemic classes
privacy scope
requested token/byte ceiling
freshness and completeness requirements
policy and model roots
seed when a randomized candidate method is explicitly admitted
```

## 2. Selection objective

Selection maximizes expected decision utility under hard constraints:

```text
maximize  relevance + marginal coverage + contradiction value
        + active-work necessity + safety value + future-control reuse
        - redundancy - token cost - retrieval cost - staleness risk
```

Safety, continuity, active indeterminate effects, and explicit operator confirmations are mandatory classes and cannot be displaced by high-scoring optional context.

The reference selector is deterministic greedy submodular selection with canonical tie breaks. Optimized search, graph expansion, or learned reranking must reproduce the reference eligibility and mandatory-item semantics.

## 3. Pack contents

A pack may contain:

- orientation spine and four-ledger digest;
- objective and question frontier;
- changed facts since the acknowledged basis;
- evidence-backed claims and counterevidence;
- causal, spatial, visibility, and provenance neighborhoods;
- active plans and obligations;
- candidate affordances and recommendations;
- relevant prior episodes and lesson candidates;
- explicit unknowns, omissions, and continuations.

Raw frames, point clouds, traces, and giant manifests remain behind typed references unless the requested profile explicitly requires them.

## 4. Pack DNA

Every pack includes a compact explanation of its composition:

```text
pack_id and content digest
basis anchor and generation roots
selector and policy identity
requested and consumed budgets
mandatory items
selected items with marginal gain
redundancy groups and representatives
omitted high-scoring items and reason
coverage gained and unresolved deficits
continuation frontier
```

Pack DNA lets an agent ask `why was this included?`, `why was that omitted?`, or `what would an extra 500 tokens buy?` without re-running the full query blindly.

## 5. Profiles

- `pulse`: continuity, critical changes, active-work transitions, highest-priority uncertainty, one to three next steps.
- `briefing`: default cold/resume orientation and four-ledger summary.
- `tactical`: one objective/question/region with causal and evidence neighborhood.
- `pilot`: immediate safe manual-flight guidance and the evidence deficit each suggestion addresses.
- `forensic`: bounded evidence-complete audit or reconciliation pack.
- `custom`: explicit registered projections and fixed bounds; unknown names fail closed.

## 6. Delta packs

A continuous session receives semantic deltas from the last acknowledged pack anchor. Stable context is referenced by digest rather than repeated. A delta is accepted only when exact basis continuity is proved. Gap, reset, and staleness are first-class states.

## 7. Cache and invalidation

Pack caches are keyed by all semantic inputs, including grants, privacy, anchor vector, policy roots, selector version, and budget. A cache entry cannot be shared across authority or privacy scopes. Changes to any source high-water mark invalidate only dependent pack components when the dependency map is complete; otherwise the whole pack is conservatively invalidated.

## 8. Completeness and absence

A pack can omit items for budget but must distinguish:

- complete for the authorized domain;
- complete for a named subset;
- top-k only;
- progressive but uncertified;
- truncated with continuation;
- unavailable or stale.

An empty pack result never proves absence unless its coverage witness establishes complete-domain eligibility and detectability.

## 9. Accretive use

The pack records which items the agent used in a plan, which were ignored, and which proved misleading or decisive. This feedback attaches to the episode and may improve future selection only through shadow-tested, rollback-capable policy promotion. The live packet itself does not silently learn during a request.
