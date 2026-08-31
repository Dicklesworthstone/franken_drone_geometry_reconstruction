#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / "release" / "source_closure.lock.json"


def git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        text=True,
        capture_output=True,
        check=False,
        env={**__import__("os").environ, "GIT_NO_REPLACE_OBJECTS": "1"},
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or f"exit {result.returncode}"
        raise RuntimeError(detail)
    return result.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify exact clean local FDGR sibling source identities.")
    parser.add_argument("--sibling-root", type=Path, required=True)
    parser.add_argument(
        "--all-planned",
        action="store_true",
        help="also verify research-only planned sources; default checks production_admitted entries only",
    )
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    document = json.loads(LOCK.read_text(encoding="utf-8"))
    rows = [
        row
        for row in document.get("planned_owned_sources", [])
        if args.all_planned or row.get("production_admitted") is True
    ]
    results: list[dict[str, object]] = []
    failures: list[str] = []
    for row in rows:
        name = str(row["name"])
        repo = args.sibling_root / name
        result: dict[str, object] = {
            "name": name,
            "path": str(repo),
            "expected_commit": row["commit"],
            "expected_tree": row["tree"],
        }
        try:
            if not repo.is_dir():
                raise RuntimeError("missing checkout directory")
            if git(repo, "rev-parse", "--is-inside-work-tree") != "true":
                raise RuntimeError("path is not a Git work tree")
            commit = git(repo, "rev-parse", "HEAD")
            tree = git(repo, "rev-parse", "HEAD^{tree}")
            dirty = git(repo, "status", "--porcelain=v1", "--untracked-files=all")
            replacements = git(repo, "for-each-ref", "--format=%(refname)", "refs/replace")
            result.update({"actual_commit": commit, "actual_tree": tree, "clean": not dirty, "replace_refs": replacements.splitlines() if replacements else []})
            if commit != row["commit"]:
                raise RuntimeError(f"commit mismatch: {commit}")
            if tree != row["tree"]:
                raise RuntimeError(f"tree mismatch: {tree}")
            if dirty:
                raise RuntimeError("checkout is dirty")
            if replacements:
                raise RuntimeError("replacement refs are present")
            result["status"] = "pass"
        except RuntimeError as error:
            result["status"] = "fail"
            result["error"] = str(error)
            failures.append(f"{name}: {error}")
        results.append(result)

    receipt = {
        "schema": "fdgr.source_closure_verification/1",
        "status": "pass" if not failures else "fail",
        "scope": "all_planned" if args.all_planned else "production_admitted",
        "checked": len(rows),
        "entries": results,
    }
    if args.out:
        output = args.out if args.out.is_absolute() else ROOT / args.out
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}", file=sys.stderr)
        return 1
    print(f"PASS: verified {len(rows)} exact clean sibling source checkout(s) ({receipt['scope']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
