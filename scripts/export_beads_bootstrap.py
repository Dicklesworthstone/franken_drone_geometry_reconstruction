
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    parser = argparse.ArgumentParser(description='Emit deterministic FDGR work-package JSONL')
    parser.add_argument('--output', type=Path, help='write to a file instead of stdout')
    parser.add_argument('--check', action='store_true', help='fail if --output does not already match')
    args = parser.parse_args()
    if args.check and args.output is None:
        parser.error('--check requires --output')
    with (ROOT / 'registries/work_packages.toml').open('rb') as handle:
        document = tomllib.load(handle)
    rows = []
    for item in document['work_package']:
        rows.append({
            'external_id': item['id'],
            'title': f"{item['id']} — {item['name']}",
            'description': item['summary'],
            'status': item['status'],
            'acceptance_gate': item['acceptance_gate'],
            'dependencies': item['dependencies'],
            'labels': ['fdgr', 'work-package', item['acceptance_gate'].lower()],
        })
    rendered = ''.join(json.dumps(row, sort_keys=True, separators=(',', ':')) + '\n' for row in rows)
    if args.output:
        if args.check:
            if not args.output.is_file() or args.output.read_text(encoding='utf-8') != rendered:
                raise SystemExit(f'{args.output}: generated Beads bootstrap is stale')
            print(f'{args.output}: generated Beads bootstrap is current')
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(rendered, encoding='utf-8')
    else:
        print(rendered, end='')

if __name__ == '__main__':
    main()
