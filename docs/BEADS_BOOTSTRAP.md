
# Beads Work Graph Bootstrap

The stable work graph is `registries/work_packages.toml`. Generate deterministic reviewable JSONL:

```bash
python3 scripts/export_beads_bootstrap.py --output /tmp/fdgr-work-packages.jsonl
```

The output intentionally does not assume a specific Beads database/import version. Map each row to
one issue while preserving `external_id`, title, dependencies, and acceptance gate. Create epics
for the plan's workstreams and attach work packages without changing dependency order.

Before marking a bead done, attach or reference the gate receipt. Source presence or a merged pull
request is not sufficient when the terminal predicate includes crash, cancellation, compatibility,
benchmark, restore, or positive-evidence requirements.
