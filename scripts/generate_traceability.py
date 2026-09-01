#!/usr/bin/env python3
from __future__ import annotations

import argparse
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md"
SUPPLEMENT = ROOT / "architecture" / "REGISTRY_TRACEABILITY_SUPPLEMENT.md"
BEGIN = "<!-- BEGIN GENERATED REGISTRY TRACEABILITY SUPPLEMENT -->"
END = "<!-- END GENERATED REGISTRY TRACEABILITY SUPPLEMENT -->"


def registry_rows() -> list[tuple[str, str, str]]:
    rows: list[tuple[str, str, str]] = []
    for path in sorted((ROOT / "registries").glob("*.toml")):
        with path.open("rb") as handle:
            document = tomllib.load(handle)
        for table_name, value in document.items():
            if not isinstance(value, list):
                continue
            for entry in value:
                if not isinstance(entry, dict) or not isinstance(entry.get("id"), str):
                    continue
                label = next(
                    (
                        entry[key]
                        for key in (
                            "name",
                            "family",
                            "message",
                            "summary",
                            "statement",
                            "authority",
                            "required_evidence",
                            "path",
                        )
                        if isinstance(entry.get(key), str)
                    ),
                    table_name,
                )
                rows.append((entry["id"], f"registries/{path.name}", label.replace("|", "\\|")))
    return sorted(rows)


def supplemental_rows() -> list[tuple[str, str, str]]:
    plan = PLAN.read_text(encoding="utf-8")
    return [row for row in registry_rows() if row[0] not in plan]


def render() -> str:
    rows = supplemental_rows()
    lines = [
        "# Registry Traceability Supplement",
        "",
        "The comprehensive plan carries the original generated registry appendix for revision 0.4.",
        "This deterministic supplement contains stable IDs introduced after that snapshot, avoiding",
        "a multi-hundred-kilobyte plan rewrite for every registry-only correction. Registries remain",
        "normative; a future plan revision may absorb these rows and empty this supplement.",
        "",
        BEGIN,
        "| Stable ID | Normative registry | Compact label |",
        "|---|---|---|",
    ]
    lines.extend(f"| `{identifier}` | `{registry}` | {label} |" for identifier, registry, label in rows)
    lines.extend([END, ""])
    return "\n".join(lines)


def update(check: bool) -> int:
    rendered = render()
    existing = SUPPLEMENT.read_text(encoding="utf-8") if SUPPLEMENT.is_file() else ""
    if check:
        if existing != rendered:
            print("registry traceability supplement is stale")
            return 1
        print(f"registry traceability supplement is current ({len(supplemental_rows())} IDs)")
        return 0
    SUPPLEMENT.write_text(rendered, encoding="utf-8")
    print(f"updated registry traceability supplement ({len(supplemental_rows())} IDs)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate or verify stable IDs introduced after the plan's embedded appendix"
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    return update(arguments.check)


if __name__ == "__main__":
    raise SystemExit(main())
