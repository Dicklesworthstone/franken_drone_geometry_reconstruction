# Agent Quickstart

This is the shortest safe operating loop for an agent using FDGR. The exact CLI is a target
contract until the corresponding implementation gate is earned.

```bash
fdgr capabilities --json
fdgr doctor --quick --json
fdgr session open --campaign home-2026 --profile briefing --json
fdgr orient --campaign home-2026 --profile briefing --max-tokens 1200 --json
```

Read the returned Agent Turn Packet in this order:

1. `continuity` and `anchor_vector`;
2. `situation.work` for active/indeterminate obligations;
3. `situation.system` for authority and degraded capabilities;
4. `attention` and `recommendations`;
5. `situation.epistemic` for uncertainty, questions, and coverage;
6. `situation.world` for admitted physical claims.

Do not begin by dumping frames or meshes. Select a focus:

```bash
fdgr orient --focus question:<id> --profile tactical --max-tokens 3000 --json
fdgr query --handle question:<id> --projection evidence,counterevidence,coverage --json
```

Propose rather than dispatching implementation commands:

```bash
fdgr propose --objective-file objective.json --alternatives 4 --json
fdgr compare --plans plan:<a>,plan:<b>,plan:<c> --json
fdgr commit --plan-digest <digest> --idempotency-key <key> --json
fdgr watch obligation:<id> --stream --format jsonl
```

After completion:

```bash
fdgr explain obligation:<id> --projection outcome,surprise,cost,objective_progress --json
fdgr handoff create --campaign home-2026 --out handoff.json --json
```

A new agent resumes with:

```bash
fdgr handoff resume handoff.json --profile pulse --json
```

The first safe step after any error is the structured recovery action in the returned packet. Never
blindly retry an indeterminate effect.

## Driver-seat shortcut

For any nontrivial task, read the current `decision_frame` first. Expand only the focal question,
attention item, candidate, obligation, or spatial handle that could change the next decision. Do
not reconstruct the task by joining raw subsystem results. During flight, treat each pilot card as
a proposal: acknowledge or refuse it, then wait for observed evidence gain before advancing.
