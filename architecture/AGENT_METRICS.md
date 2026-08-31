# Agent-Native Metrics

FDGR qualifies agent ergonomics as observable behavior, not aesthetic opinion. Metrics are always
reported with workload, model, device, policy, host, source, and anchor identities.

## Core metrics

| Dimension | Metric |
|---|---|
| Orientation | cold-arrival task success from one briefing; time/tokens to correct anchor understanding |
| Continuity | gaps/resets/indeterminate states detected without false continuity |
| Context | decision success per transferred token; Pack-DNA omission regret; redundancy rate |
| Questions | decision-weighted question-closure rate; unnecessary evidence acquired; consciously accepted debt |
| Planning | Pareto-frontier stability; candidate regret; stale-plan and blind-retry prevention |
| Progress | semantic milestone accuracy; polling calls avoided; terminal-proof latency |
| Flight | useful evidence gain per pilot minute/battery; cards skipped; overload/abort rate; reflights avoided |
| Spatial language | handle/alias resolution success; coordinate-frame errors; historical-handle resolution |
| Handoff | safe next-step completion by a fresh agent without transcript replay |
| Accretion | control-cost reduction on repeated workloads with proof/calibration unchanged or improved |
| Errors | first-try repair success; exact state preserved; unsafe retry suggestions equal zero |
| Multi-agent | duplicate work prevented; conflicts detected; branch/lease handoff success |

## Denominators and negative controls

A metric without a fixed benchmark intent set and failure denominator is not a readiness claim.
Agent tests include deliberately stale anchors, contradictory models, empty-but-incomplete searches,
indeterminate uploads/effects, misleading memory, ambiguous landmarks, equivalent candidates,
unchanged heartbeats, and operator refusal.

## North-star quantity

```text
verified objective progress + decision-relevant uncertainty reduction
---------------------------------------------------------------------
 tokens + compute + bytes + pilot/battery + risk + recovery burden
```

Hard safety, privacy, authority, scale, custody, and evidence gates remain constraints rather than
terms to optimize away.
