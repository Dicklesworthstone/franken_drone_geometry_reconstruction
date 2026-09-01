#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import sys
import tomllib
from pathlib import Path

from registry_contracts import (
    REQUIRED_FORBIDDEN_CRATES,
    REQUIRED_FUNDAMENTAL_EXCEPTIONS,
    validate_registry_contracts,
)

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / "architecture/dependency_allowlist.toml"
CLOSURE = ROOT / "release/source_closure.lock.json"
errors: list[str] = []


def fail(message: str) -> None:
    errors.append(message)


def dependency_tables(document: dict) -> list[dict]:
    tables: list[dict] = []
    for key in ("dependencies", "dev-dependencies", "build-dependencies"):
        value = document.get(key)
        if isinstance(value, dict):
            tables.append(value)
    workspace = document.get("workspace")
    if isinstance(workspace, dict):
        value = workspace.get("dependencies")
        if isinstance(value, dict):
            tables.append(value)
    target = document.get("target")
    if isinstance(target, dict):
        for cfg in target.values():
            if isinstance(cfg, dict):
                for key in ("dependencies", "dev-dependencies", "build-dependencies"):
                    value = cfg.get(key)
                    if isinstance(value, dict):
                        tables.append(value)
    return tables


for error in validate_registry_contracts(ROOT):
    fail(error)
with POLICY.open("rb") as handle:
    policy = tomllib.load(handle)
if policy.get("revision") != 1 or policy.get("policy") != "closed_universe":
    fail("dependency policy identity/revision is invalid")
for required, expected in (
    ("exclusive_async_runtime", "asupersync"),
    ("fdgr_unsafe_code", "forbid"),
    ("c_cpp_ffi", False),
    ("in_process_python", False),
    ("hosted_github_actions_authority", False),
):
    if policy.get(required) != expected:
        fail(f"dependency policy {required} must equal {expected!r}")
forbidden = {
    row["name"]
    for row in policy.get("forbidden", [])
    if isinstance(row, dict) and isinstance(row.get("name"), str)
}
fundamental = {
    row["name"]
    for row in policy.get("fundamental_exception", [])
    if isinstance(row, dict) and isinstance(row.get("name"), str)
}
if forbidden != REQUIRED_FORBIDDEN_CRATES:
    fail(f"dependency policy forbidden set mismatch: {sorted(forbidden)}")
if fundamental != REQUIRED_FUNDAMENTAL_EXCEPTIONS:
    fail(f"dependency policy fundamental exception set mismatch: {sorted(fundamental)}")
allowed_current = fundamental | {"asupersync"}
local_names = {path.parent.name for path in ROOT.glob("crates/*/Cargo.toml")}

for manifest in [ROOT / "Cargo.toml", *sorted(ROOT.glob("crates/*/Cargo.toml"))]:
    with manifest.open("rb") as handle:
        document = tomllib.load(handle)
    for table in dependency_tables(document):
        for alias, spec in table.items():
            package = spec.get("package") if isinstance(spec, dict) else None
            name = package if isinstance(package, str) else alias
            if alias in forbidden or name in forbidden:
                suffix = f" through alias {alias}" if alias != name else ""
                fail(f"{manifest.relative_to(ROOT)}: forbidden dependency {name}{suffix}")
            if name in local_names:
                if not (isinstance(spec, dict) and isinstance(spec.get("path"), str)):
                    fail(f"{manifest.relative_to(ROOT)}: local dependency {name} must use an explicit path")
                continue
            if name not in allowed_current:
                fail(f"{manifest.relative_to(ROOT)}: dependency {name} is outside the admitted universe")
            if isinstance(spec, str):
                fail(f"{manifest.relative_to(ROOT)}: dependency {name} must use an exact table specification")
            if isinstance(spec, dict):
                version = spec.get("version")
                if version is not None and not (
                    isinstance(version, str) and version.startswith("=") and len(version) > 1
                ):
                    fail(f"{manifest.relative_to(ROOT)}: dependency {name} must pin an exact =version")
                if "git" in spec and (
                    not isinstance(spec.get("rev"), str)
                    or not re.fullmatch(r"[0-9a-f]{40}", spec["rev"])
                ):
                    fail(f"{manifest.relative_to(ROOT)}: git dependency {name} needs a 40-hex rev")
                if spec.get("default-features") is not False:
                    fail(f"{manifest.relative_to(ROOT)}: external dependency {name} must disable default features")

closure = json.loads(CLOSURE.read_text(encoding="utf-8"))
if closure.get("schema") != "fdgr.source_closure/1":
    fail("source closure has the wrong schema")
for source in closure.get("planned_owned_sources", []):
    for key in ("name", "repository", "commit", "tree"):
        if not isinstance(source.get(key), str) or not source[key]:
            fail(f"source closure entry missing {key}")
    if not re.fullmatch(r"[0-9a-f]{40}", source.get("commit", "")) or not re.fullmatch(
        r"[0-9a-f]{40}", source.get("tree", "")
    ):
        fail(f"source closure entry {source.get('name')} has a noncanonical identity")

for source in sorted(ROOT.glob("crates/*/src/**/*.rs")):
    text = source.read_text(encoding="utf-8")
    for label, pattern in {
        "extern C ABI": r'extern\s+"C"',
        "unsafe item": r"\bunsafe\b",
        "in-process Python/PyO3": r"\bpyo3\b|Python::with_gil|prepare_freethreaded_python",
        "Tokio symbol": r"\btokio::",
        "Rayon symbol": r"\brayon::",
    }.items():
        if re.search(pattern, text):
            fail(f"{source.relative_to(ROOT)}: forbidden {label}")

if errors:
    for error in sorted(set(errors)):
        print(f"ERROR: {error}", file=sys.stderr)
    raise SystemExit(1)
print(
    f"PASS: dependency policy ({len(forbidden)} forbidden names, "
    f"{len(closure.get('planned_owned_sources', []))} exact research source identities)"
)
