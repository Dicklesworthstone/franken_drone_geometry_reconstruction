#!/usr/bin/env python3
from __future__ import annotations

import argparse
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / 'COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md'
BEGIN = '<!-- BEGIN GENERATED REGISTRY TRACEABILITY -->'
END = '<!-- END GENERATED REGISTRY TRACEABILITY -->'


def registry_rows() -> list[tuple[str, str, str]]:
    rows: list[tuple[str, str, str]] = []
    for path in sorted((ROOT / 'registries').glob('*.toml')):
        with path.open('rb') as handle:
            document = tomllib.load(handle)
        for table_name, value in document.items():
            if not isinstance(value, list):
                continue
            for entry in value:
                if not isinstance(entry, dict) or not isinstance(entry.get('id'), str):
                    continue
                label = next(
                    (
                        entry[key]
                        for key in ('name', 'family', 'message', 'summary', 'statement', 'authority', 'required_evidence', 'path')
                        if isinstance(entry.get(key), str)
                    ),
                    table_name,
                )
                rows.append((entry['id'], f'registries/{path.name}', label.replace('|', '\\|')))
    return sorted(rows)


def render() -> str:
    rows = registry_rows()
    lines = [
        BEGIN,
        '# Appendix G — Machine Registry Traceability Index',
        '',
        'This index is generated from the TOML registries. It proves that every published machine',
        'identifier has a stable human-plan landing point; registry content remains normative when',
        'a compact label below omits detail.',
        '',
        '| Stable ID | Normative registry | Compact label |',
        '|---|---|---|',
    ]
    lines.extend(f'| `{identifier}` | `{registry}` | {label} |' for identifier, registry, label in rows)
    lines.extend(['', END])
    return '\n'.join(lines) + '\n'


def update_plan(check: bool) -> int:
    existing = PLAN.read_text(encoding='utf-8')
    generated = render()
    if BEGIN in existing:
        prefix, remainder = existing.split(BEGIN, 1)
        if END not in remainder:
            raise SystemExit('traceability begin marker exists without end marker')
        _, suffix = remainder.split(END, 1)
        updated = prefix.rstrip() + '\n\n' + generated + suffix.lstrip('\n')
    else:
        updated = existing.rstrip() + '\n\n' + generated
    if check:
        if updated != existing:
            print('traceability appendix is stale', flush=True)
            return 1
        print(f'traceability appendix is current ({len(registry_rows())} IDs)')
        return 0
    PLAN.write_text(updated, encoding='utf-8')
    print(f'updated traceability appendix ({len(registry_rows())} IDs)')
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description='Generate or verify the FDGR registry traceability appendix')
    parser.add_argument('--check', action='store_true', help='fail if the checked-in appendix is stale')
    arguments = parser.parse_args()
    return update_plan(arguments.check)


if __name__ == '__main__':
    raise SystemExit(main())
