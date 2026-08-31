
# Beads bootstrap

The normative work graph lives in `registries/work_packages.toml`. The plan intentionally keeps
stable `WP-*` identifiers independent of any one issue tracker database. When this repository is
initialized with Beads, create one epic per workstream and one issue per work package, preserving
those IDs in titles and dependencies. `scripts/export_beads_bootstrap.py` emits deterministic
JSONL suitable for review before import; it does not mutate a Beads database.
