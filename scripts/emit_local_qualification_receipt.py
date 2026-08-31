#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATHS = (
    ROOT / "architecture",
    ROOT / "registries",
    ROOT / "schemas",
    ROOT / "docs" / "AGENT_OPERATING_MODEL.md",
    ROOT / "docs" / "AGENT_ACCEPTANCE_SCENARIOS.md",
)


def run(*args: str) -> str:
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or f"exit {result.returncode}"
        raise RuntimeError(f"{' '.join(args)}: {detail}")
    return result.stdout.strip()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def contract_root() -> str:
    digest = hashlib.sha256()
    files: list[Path] = []
    for candidate in CONTRACT_PATHS:
        if candidate.is_dir():
            files.extend(path for path in candidate.rglob("*") if path.is_file())
        elif candidate.is_file():
            files.append(candidate)
    for path in sorted(files, key=lambda item: item.relative_to(ROOT).as_posix()):
        relative = path.relative_to(ROOT).as_posix().encode("utf-8")
        payload = path.read_bytes()
        digest.update(len(relative).to_bytes(8, "big"))
        digest.update(relative)
        digest.update(len(payload).to_bytes(8, "big"))
        digest.update(payload)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Seal exact FDGR source, contract, closure, toolchain, and host identities after "
            "Doodlestein has retained successful predecessor-job receipts."
        )
    )
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    try:
        status = run("git", "status", "--porcelain=v1", "--untracked-files=all")
        if status:
            raise RuntimeError("qualification identity seal requires a clean Git tree")
        commit = run("git", "rev-parse", "HEAD")
        tree = run("git", "rev-parse", "HEAD^{tree}")
        cargo = run("cargo", "--version", "--verbose")
        rustc = run("rustc", "--version", "--verbose")
    except RuntimeError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    source_closure = ROOT / "release" / "source_closure.lock.json"
    toolchain = ROOT / "rust-toolchain.toml"
    cargo_lock = ROOT / "Cargo.lock"
    receipt = {
        "schema": "fdgr.local_qualification_identity/1",
        "status": "identity_sealed_after_predecessor_success",
        "authority_boundary": (
            "This receipt binds identities only. Qualification authority also requires the "
            "successful Doodlestein predecessor receipts named by the job graph."
        ),
        "sealed_at_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "doodlestein_run_id": os.environ.get("FDGR_DOODLESTEIN_RUN_ID"),
        "source": {"commit": commit, "tree": tree},
        "roots": {
            "cargo_lock_sha256": sha256_file(cargo_lock),
            "toolchain_file_sha256": sha256_file(toolchain),
            "source_closure_sha256": sha256_file(source_closure),
            "agent_contract_root_sha256": contract_root(),
        },
        "toolchain": {"cargo": cargo, "rustc": rustc},
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "required_predecessor_jobs": [
            "repo-policy",
            "dependency-policy",
            "generated-contracts",
            "rust-core",
            "agent-contract",
            "agent-scenarios",
        ],
    }
    output = args.out if args.out.is_absolute() else ROOT / args.out
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    try:
        display = output.relative_to(ROOT)
    except ValueError:
        display = output
    print(f"PASS: wrote identity-bound local qualification receipt to {display}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
