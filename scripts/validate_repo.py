#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import sys
import tomllib
from collections import defaultdict, deque
from pathlib import Path
from urllib.parse import unquote

from registry_contracts import REQUIRED_FORBIDDEN_CRATES, validate_registry_contracts

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {
    "README.md", "ARCHITECTURE.md", "FRANKENSTACK_DEEP_DIVE.md",
    "COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md",
    "IMPLEMENTATION_STATUS.md", "QUALIFICATION.md", "DEPENDENCY_POLICY.md",
    "MODEL_REGISTRY.md", "DJI_ADAPTER_RESEARCH.md", "SECURITY.md", "PRIVACY.md",
    "AGENTS.md", "LICENSE", "Cargo.toml", "Cargo.lock", "rust-toolchain.toml",
    "DESIGN_INDEX.md", "LOCAL_QUALIFICATION_AND_RELEASE.md",
    "docs/AGENT_OPERATING_MODEL.md", "docs/AGENT_QUICKSTART.md",
    "docs/AGENT_ACCEPTANCE_SCENARIOS.md", "architecture/SEMANTICS_MANIFEST.md",
    "architecture/AGENT_ABSTRACTION_TOWER.md", "architecture/AGENT_NARROW_WAIST.md",
    "architecture/DECISION_FRAME.md", "architecture/ATTENTION_AND_EPISTEMIC_DEBT.md",
    "architecture/SPATIAL_SEMANTIC_HANDLES.md", "architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md",
    "architecture/AGENT_METRICS.md", "architecture/agent_turn_contract.json",
    "architecture/REGISTRY_TRACEABILITY_SUPPLEMENT.md", "architecture/dependency_allowlist.toml",
    "architecture/qualification_lanes.toml", "release/source_closure.lock.json",
    "release/doodlestein_job_graph.json", "scripts/registry_contracts.py",
    "scripts/test_registry_contracts.py", "scripts/check_dependency_policy.py",
    "scripts/generate_traceability.py", "scripts/validate_agent_contracts.py",
}
ID_RE = re.compile(r"^(?:INV|BET|GOAL|NONGOAL|CAP|EFFECT|CLAIM|ERR|SCHEMA|ADR|WP|GATE|TEST|SLO|RISK|OPEN|MODEL|OP|GEOM|GALG)-[A-Z0-9-]+$")
LINK_RE = re.compile(r"(?<!!)\[[^\]]*\]\(([^)]+)\)")
RUST_DENIALS = {
    "unsafe block": re.compile(r"\bunsafe\s*\{"), "unsafe function": re.compile(r"\bunsafe\s+fn\b"),
    "unsafe impl": re.compile(r"\bunsafe\s+impl\b"), "unwrap": re.compile(r"\.unwrap\s*\("),
    "expect": re.compile(r"\.expect\s*\("), "panic macro": re.compile(r"\bpanic!\s*\("),
    "todo macro": re.compile(r"\btodo!\s*\("), "unimplemented macro": re.compile(r"\bunimplemented!\s*\("),
    "dbg macro": re.compile(r"\bdbg!\s*\("),
}
SKIP = {".git", ".ee", ".fdgr", "target", ".br_history", "__pycache__"}
BEADS = {"bootstrap.jsonl", "README.md", "metadata.json", "config.yaml", "issues.jsonl", "beads.base.jsonl"}
errors: list[str] = []


def fail(message: str) -> None:
    errors.append(message)


def toml(path: Path) -> dict:
    try:
        with path.open("rb") as handle:
            value = tomllib.load(handle)
    except Exception as exc:
        fail(f"{path.relative_to(ROOT)}: invalid TOML: {exc}")
        return {}
    return value if isinstance(value, dict) else {}


def json_doc(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        fail(f"{path.relative_to(ROOT)}: invalid JSON: {exc}")
        return {}


def dag(rows: list[dict], dependency_field: str, label: str) -> list[str]:
    ids = [row.get("id") for row in rows]
    if any(not isinstance(value, str) for value in ids) or len(ids) != len(set(ids)):
        fail(f"{label}: invalid or duplicate identities")
        return []
    indegree = {value: 0 for value in ids}
    children: dict[str, list[str]] = defaultdict(list)
    for row in rows:
        for dependency in row.get(dependency_field, []):
            if dependency not in indegree:
                fail(f"{label} {row['id']}: unknown dependency {dependency}")
                continue
            indegree[row["id"]] += 1
            children[dependency].append(row["id"])
    queue = deque(sorted(value for value, degree in indegree.items() if degree == 0))
    seen: list[str] = []
    while queue:
        value = queue.popleft()
        seen.append(value)
        for child in sorted(children[value]):
            indegree[child] -= 1
            if indegree[child] == 0:
                queue.append(child)
    if len(seen) != len(ids):
        fail(f"{label}: dependency graph contains a cycle")
    return ids


def dependency_tables(document: dict) -> list[dict]:
    tables = [document.get(name) for name in ("dependencies", "dev-dependencies", "build-dependencies")]
    workspace = document.get("workspace")
    if isinstance(workspace, dict):
        tables.append(workspace.get("dependencies"))
    target = document.get("target")
    if isinstance(target, dict):
        for scoped in target.values():
            if isinstance(scoped, dict):
                tables.extend(scoped.get(name) for name in ("dependencies", "dev-dependencies", "build-dependencies"))
    return [table for table in tables if isinstance(table, dict)]


for relative in sorted(REQUIRED):
    if not (ROOT / relative).is_file():
        fail(f"missing required file: {relative}")
for path in ROOT.rglob("*"):
    if not path.is_file() or any(part in SKIP for part in path.parts) or path.name == ".DS_Store" or (".beads" in path.parts and path.name not in BEADS):
        continue
    data = path.read_bytes()
    if b"\r\n" in data:
        fail(f"{path.relative_to(ROOT)}: CRLF line endings are forbidden")
    if data and not data.endswith(b"\n"):
        fail(f"{path.relative_to(ROOT)}: text file must end with newline")

errors.extend(validate_registry_contracts(ROOT))
registries = {path.name: toml(path) for path in sorted((ROOT / "registries").glob("*.toml"))}
all_ids: dict[str, str] = {}
for filename, document in registries.items():
    for value in document.values():
        if isinstance(value, list):
            for row in value:
                identifier = row.get("id") if isinstance(row, dict) else None
                if identifier is None:
                    continue
                if not isinstance(identifier, str) or not ID_RE.fullmatch(identifier):
                    fail(f"registries/{filename}: invalid stable ID {identifier!r}")
                elif identifier in all_ids:
                    fail(f"duplicate stable ID {identifier}: {all_ids[identifier]} and registries/{filename}")
                else:
                    all_ids[identifier] = f"registries/{filename}"
work = [row for row in registries.get("work_packages.toml", {}).get("work_package", []) if isinstance(row, dict)]
work_ids = dag(work, "dependencies", "work packages")
gates = {row.get("id") for row in registries.get("gates.toml", {}).get("gate", []) if isinstance(row, dict)}
for row in work:
    if row.get("acceptance_gate") not in gates:
        fail(f"{row.get('id')}: unknown acceptance gate {row.get('acceptance_gate')!r}")

schema_ids: set[str] = set()
schema_documents: dict[str, dict] = {}
for path in sorted((ROOT / "schemas").glob("*.json")):
    document = json_doc(path)
    if not isinstance(document, dict):
        fail(f"{path.relative_to(ROOT)}: schema root must be object")
        continue
    schema_id = document.get("$id")
    if not isinstance(schema_id, str) or schema_id in schema_ids:
        fail(f"{path.relative_to(ROOT)}: missing or duplicate $id {schema_id!r}")
    else:
        schema_ids.add(schema_id)
    if document.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        fail(f"{path.relative_to(ROOT)}: must use JSON Schema 2020-12")
    properties = document.get("properties")
    identity = properties.get("schema", {}).get("const") if isinstance(properties, dict) else None
    if not isinstance(identity, str) or not re.fullmatch(r"fdgr\.[a-z][a-z0-9_]*/1", identity):
        fail(f"{path.relative_to(ROOT)}: payload schema identity must use fdgr.<name>/1")
    def refs(node: object) -> None:
        if isinstance(node, dict):
            reference = node.get("$ref")
            if isinstance(reference, str) and not reference.startswith(("#", "http://", "https://")):
                target = reference.split("#", 1)[0]
                if target and not (path.parent / target).is_file():
                    fail(f"{path.relative_to(ROOT)}: unresolved schema reference {reference!r}")
            for child in node.values():
                refs(child)
        elif isinstance(node, list):
            for child in node:
                refs(child)
    refs(document)
    schema_documents[str(path.relative_to(ROOT))] = document
for row in registries.get("schemas.toml", {}).get("public_schema", []):
    if isinstance(row, dict):
        document = schema_documents.get(row.get("path"))
        if document is None or row.get("json_schema_id") != document.get("$id"):
            fail(f"{row.get('id')}: schema registry/path identity mismatch")
for row in registries.get("adrs.toml", {}).get("adr", []):
    if isinstance(row, dict) and (not isinstance(row.get("path"), str) or not (ROOT / row["path"]).is_file()):
        fail(f"{row.get('id')}: ADR path missing: {row.get('path')!r}")
for row in registries.get("models.toml", {}).get("model", []):
    if isinstance(row, dict) and row.get("network_default") is not False:
        fail(f"{row.get('id')}: model workers must default to no network")

for path in sorted(ROOT.glob("crates/*/src/**/*.rs")):
    text = path.read_text(encoding="utf-8")
    if "#![forbid(unsafe_code)]" not in text:
        fail(f"{path.relative_to(ROOT)}: missing #![forbid(unsafe_code)]")
    for label, pattern in RUST_DENIALS.items():
        if pattern.search(text):
            fail(f"{path.relative_to(ROOT)}: forbidden {label}")
for path in [ROOT / "Cargo.toml", *sorted(ROOT.glob("crates/*/Cargo.toml"))]:
    for table in dependency_tables(toml(path)):
        for alias, spec in table.items():
            package = spec.get("package") if isinstance(spec, dict) else None
            name = package if isinstance(package, str) else alias
            if alias in REQUIRED_FORBIDDEN_CRATES or name in REQUIRED_FORBIDDEN_CRATES:
                fail(f"{path.relative_to(ROOT)}: forbidden dependency {name}" + (f" through alias {alias}" if alias != name else ""))

plan = (ROOT / "COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md").read_text(encoding="utf-8")
supplement = (ROOT / "architecture/REGISTRY_TRACEABILITY_SUPPLEMENT.md").read_text(encoding="utf-8")
for identifier in sorted(all_ids):
    if identifier not in plan and identifier not in supplement:
        fail(f"normative ID has no plan or supplement landing point: {identifier}")
if "<!-- BEGIN GENERATED REGISTRY TRACEABILITY -->" not in plan or "<!-- END GENERATED REGISTRY TRACEABILITY -->" not in plan:
    fail("comprehensive plan is missing traceability markers")
if "<!-- BEGIN GENERATED REGISTRY TRACEABILITY SUPPLEMENT -->" not in supplement or "<!-- END GENERATED REGISTRY TRACEABILITY SUPPLEMENT -->" not in supplement:
    fail("registry traceability supplement is missing markers")

agent = json_doc(ROOT / "architecture/agent_turn_contract.json")
agent_schema = schema_documents.get("schemas/agent_turn.schema.json", {})
if isinstance(agent, dict) and isinstance(agent_schema, dict):
    for field in agent.get("field_order", []):
        if field not in agent_schema.get("properties", {}):
            fail(f"agent turn field absent from schema: {field}")
    profiles = {row.get("name") for row in registries.get("agent_profiles.toml", {}).get("profile", []) if isinstance(row, dict)}
    if profiles != set(agent.get("profiles", {})):
        fail("agent profile registry and turn contract differ")
source = json_doc(ROOT / "research/source-inventory/source_manifest.json")
closure = json_doc(ROOT / "release/source_closure.lock.json")
if isinstance(source, dict):
    rows = source.get("repositories", [])
    if len(rows) != 11:
        fail("source inventory must retain eleven repositories")
    identities = {(row.get("name"), row.get("commit"), row.get("tree")) for row in rows if isinstance(row, dict)}
    closure_rows = closure.get("planned_owned_sources", []) if isinstance(closure, dict) else []
    if identities != {(row.get("name"), row.get("commit"), row.get("tree")) for row in closure_rows if isinstance(row, dict)}:
        fail("source inventory and closure lock disagree")

lanes = toml(ROOT / "architecture/qualification_lanes.toml")
if lanes.get("hosted_github_actions_authority") is not False:
    fail("qualification lanes must deny hosted authority")
dag([row for row in lanes.get("lane", []) if isinstance(row, dict)], "requires", "qualification lanes")
jobs = json_doc(ROOT / "release/doodlestein_job_graph.json")
if isinstance(jobs, dict):
    if jobs.get("schema") != "fdgr.doodlestein_job_graph/1" or jobs.get("authority") != "local_receipts_only" or jobs.get("hosted_github_actions_authority") is not False:
        fail("Doodlestein job graph authority/schema mismatch")
    dag([row for row in jobs.get("jobs", []) if isinstance(row, dict)], "needs", "Doodlestein jobs")
for workflow in sorted((ROOT / ".github/workflows").glob("*.y*ml")):
    text = workflow.read_text(encoding="utf-8")
    if re.search(r"^\s*uses\s*:", text, flags=re.MULTILINE) or re.search(r"runs-on\s*:\s*(?:ubuntu|windows|macos)-", text, flags=re.IGNORECASE):
        fail(f"{workflow.relative_to(ROOT)}: hosted/external action authority forbidden")
channel = toml(ROOT / "rust-toolchain.toml").get("toolchain", {}).get("channel")
if not isinstance(channel, str) or not re.fullmatch(r"nightly-\d{4}-\d{2}-\d{2}", channel):
    fail("rust-toolchain.toml must pin exact dated nightly")

for path in sorted(ROOT.rglob("*.md")):
    if any(part in SKIP for part in path.parts):
        continue
    for raw in LINK_RE.findall(path.read_text(encoding="utf-8")):
        target = raw.strip().split()[0].strip("<>")
        if not target or target.startswith(("#", "http://", "https://", "mailto:")):
            continue
        relative = unquote(target.split("#", 1)[0])
        if relative and not (path.parent / relative).resolve().exists():
            fail(f"{path.relative_to(ROOT)}: broken relative Markdown link {raw!r}")
bootstrap = ROOT / ".beads/bootstrap.jsonl"
if bootstrap.is_file():
    try:
        rows = [json.loads(line) for line in bootstrap.read_text(encoding="utf-8").splitlines() if line]
    except Exception as exc:
        fail(f".beads/bootstrap.jsonl: invalid JSONL: {exc}")
        rows = []
    if [row.get("external_id") for row in rows] != work_ids:
        fail(".beads/bootstrap.jsonl: work identities/order stale")
else:
    fail("missing .beads/bootstrap.jsonl")

cargo = toml(ROOT / "Cargo.toml")
workspace = cargo.get("workspace")
package = workspace.get("package", {}) if isinstance(workspace, dict) else {}
if package.get("license-file") != "LICENSE":
    fail("Cargo.toml must inherit LICENSE")
locked = {row.get("name") for row in toml(ROOT / "Cargo.lock").get("package", []) if isinstance(row, dict)}
expected = {path.parent.name for path in ROOT.glob("crates/*/Cargo.toml")}
if locked != expected:
    fail(f"Cargo.lock package set stale: expected {sorted(expected)}, found {sorted(locked)}")
for script in sorted((ROOT / "scripts").glob("*")):
    if script.suffix in {".py", ".sh"} and not (script.stat().st_mode & 0o111):
        fail(f"{script.relative_to(ROOT)}: script lacks executable bit")

if errors:
    for message in sorted(set(errors)):
        print(f"ERROR: {message}", file=sys.stderr)
    print(f"FAILED: {len(set(errors))} repository policy error(s)", file=sys.stderr)
    raise SystemExit(1)
print(f"PASS: {len(registries)} TOML registries parsed")
print(f"PASS: {len(schema_ids)} JSON Schemas parsed")
print(f"PASS: {len(all_ids)} stable registry IDs unique and plan-traceable")
print(f"PASS: {len(work)} work packages form an acyclic graph")
print(f"PASS: {len(list(ROOT.glob('crates/*/Cargo.toml')))} executable scaffold crates checked")
print("PASS: FDGR repository policy validation complete")
