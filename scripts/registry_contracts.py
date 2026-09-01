#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
import tomllib
from pathlib import Path
from typing import Any

PREFIXES = "INV|BET|GOAL|NONGOAL|CAP|EFFECT|CLAIM|ERR|SCHEMA|ADR|WP|GATE|TEST|SLO|RISK|OPEN|MODEL|OP|GEOM|GALG"
ID_BODY = r"[A-Z0-9]+(?:-[A-Z0-9]+)*"
ID_RE = re.compile(rf"^(?:{PREFIXES})-{ID_BODY}$")
ID_TOKEN_RE = re.compile(rf"(?<![A-Z0-9-])(?:{PREFIXES})-{ID_BODY}(?=$|[^A-Z0-9-]|-[a-z])")
HEX40_RE = re.compile(r"^[0-9a-f]{40}$")

REQUIRED_FORBIDDEN_CRATES = frozenset({
    "async-std", "axum", "diesel", "hyper", "opencv", "pyo3", "rayon", "reqwest",
    "rocksdb", "rusqlite", "sea-orm", "smol", "sqlx", "tokio", "tower",
})
REQUIRED_FUNDAMENTAL_EXCEPTIONS = frozenset({"serde", "serde_bytes", "serde_json"})
REQUIRED_EXTERNAL_PROCESSES = frozenset({"ffmpeg", "python_model_worker"})
REQUIRED_LINKED_DENIALS = frozenset({
    "ceres", "colmap", "cuda", "ffmpeg_libav", "opencv", "pytorch", "vendor_sdk",
})

SPECS: dict[str, tuple[set[str], str | None, set[str], set[str]]] = {
    "adrs.toml": ({"schema", "revision"}, "adr", {"id", "name", "path", "status"}, set()),
    "agent_operations.toml": ({"schema", "revision"}, "operation", {"id", "logical_name", "phase", "summary", "authority"}, set()),
    "agent_profiles.toml": ({"schema", "revision"}, "profile", {"id", "name", "purpose"}, {"target_tokens_min", "target_tokens_max", "requires_focus", "requires_explicit_scope", "unknown_projection_behavior"}),
    "capabilities.toml": ({"schema", "revision", "default_policy"}, "capability", {"id", "authority", "status"}, set()),
    "claims.toml": ({"schema", "revision"}, "claim", {"id", "name", "required_evidence", "terminal_predicate"}, set()),
    "dependency_allowlist.toml": ({"schema", "revision", "policy", "authoritative_path", "authoritative_schema", "mirror_contract"}, None, set(), set()),
    "doctrine.toml": ({"schema", "revision"}, "doctrine", {"id", "name", "statement"}, set()),
    "effects.toml": ({"schema", "revision"}, "effect", {"id", "name", "capability", "risk", "reversible", "completion"}, set()),
    "errors.toml": ({"schema", "revision"}, "error", {"id", "category", "message", "retry"}, set()),
    "gates.toml": ({"schema", "revision"}, "gate", {"id", "name", "terminal_predicate"}, set()),
    "geometry_algorithms.toml": ({"schema", "revision", "contract", "numeric_contract"}, "stage", {"id", "name", "inputs", "outputs", "reference"}, set()),
    "graph_algorithms.toml": ({"schema", "revision", "contract", "source_family_count", "optimized_family_source", "optimized_admission_gate", "optimized_admission_evidence", "published_id_policy", "certificate"}, "family", {"id", "source_family_ordinal", "name", "uses", "reference"}, set()),
    "invariants.toml": ({"schema", "revision"}, "invariant", {"id", "name", "text"}, set()),
    "models.toml": ({"schema", "revision", "snapshot_date", "default_policy"}, "model", {"id", "family", "role", "artifact", "license", "status", "network_default", "notes"}, set()),
    "open_questions.toml": ({"schema", "revision"}, "open_question", {"id", "name", "statement"}, set()),
    "operation_costs.toml": ({"schema", "revision"}, "operation", {"id", "dominant_dimensions", "required_receipts"}, set()),
    "risks.toml": ({"schema", "revision"}, "risk", {"id", "name", "statement"}, set()),
    "schemas.toml": ({"schema", "revision"}, "public_schema", {"id", "path", "json_schema_id", "status"}, set()),
    "slos.toml": ({"schema", "revision"}, "slo", {"id", "name", "statement"}, set()),
    "tests.toml": ({"schema", "revision"}, "test_family", {"id", "required_evidence"}, set()),
    "work_packages.toml": ({"schema", "revision"}, "work_package", {"id", "name", "summary", "dependencies", "acceptance_gate", "status"}, set()),
}
LIST_FIELDS = {"dependencies", "dominant_dimensions", "inputs", "outputs", "required_receipts", "uses"}
BOOL_FIELDS = {"network_default", "requires_explicit_scope", "requires_focus", "reversible"}
INT_FIELDS = {"source_family_ordinal", "target_tokens_min", "target_tokens_max"}


def _load(path: Path, errors: list[str]) -> dict[str, Any]:
    try:
        with path.open("rb") as handle:
            value = tomllib.load(handle)
    except Exception as exc:
        errors.append(f"{path}: invalid TOML: {exc}")
        return {}
    return value if isinstance(value, dict) else {}


def _strings(node: object, path: str):
    if isinstance(node, str):
        yield path, node
    elif isinstance(node, dict):
        for key in sorted(node):
            yield from _strings(node[key], f"{path}.{key}")
    elif isinstance(node, list):
        for index, value in enumerate(node):
            yield from _strings(value, f"{path}[{index}]")


def _validate_shape(filename: str, document: dict[str, Any], all_ids: dict[str, str], errors: list[str]) -> None:
    root_required, collection, required, optional = SPECS[filename]
    allowed_root = root_required | ({collection} if collection else set())
    for field in sorted(root_required - set(document)):
        errors.append(f"registries/{filename}.{field}: missing required root field")
    for field in sorted(set(document) - allowed_root):
        errors.append(f"registries/{filename}.{field}: unknown root field")
    if not isinstance(document.get("schema"), str):
        errors.append(f"registries/{filename}.schema: expected string")
    if document.get("revision") != 1:
        errors.append(f"registries/{filename}.revision: expected 1")
    if collection is None:
        return
    rows = document.get(collection)
    if not isinstance(rows, list):
        errors.append(f"registries/{filename}.{collection}: expected array of tables")
        return
    allowed = required | optional
    for index, row in enumerate(rows):
        location = f"registries/{filename}:{collection}[{index}]"
        if not isinstance(row, dict):
            errors.append(f"{location}: expected table")
            continue
        identifier = row.get("id")
        if isinstance(identifier, str):
            location += f" id={identifier}"
        for field in sorted(required - set(row)):
            errors.append(f"{location}.{field}: missing required field")
        for field in sorted(set(row) - allowed):
            errors.append(f"{location}.{field}: unknown field")
        if not isinstance(identifier, str) or not ID_RE.fullmatch(identifier):
            errors.append(f"{location}.id: invalid stable ID {identifier!r}")
        else:
            previous = all_ids.get(identifier)
            if previous is not None:
                errors.append(f"{location}.id: duplicate {identifier}; first at {previous}")
            else:
                all_ids[identifier] = location
        for field, value in row.items():
            if field in LIST_FIELDS and (not isinstance(value, list) or not all(isinstance(item, str) for item in value)):
                errors.append(f"{location}.{field}: expected array of strings")
            if field in BOOL_FIELDS and not isinstance(value, bool):
                errors.append(f"{location}.{field}: expected boolean")
            if field in INT_FIELDS and (not isinstance(value, int) or isinstance(value, bool)):
                errors.append(f"{location}.{field}: expected integer")


def _validate_profiles(document: dict[str, Any], errors: list[str]) -> None:
    profiles = {row.get("name"): row for row in document.get("profile", []) if isinstance(row, dict)}
    expected = {"pulse", "briefing", "tactical", "pilot", "forensic", "custom"}
    if set(profiles) != expected:
        errors.append(f"registries/agent_profiles.toml: expected {sorted(expected)}, found {sorted(profiles)}")
    for name in ("pulse", "briefing"):
        row = profiles.get(name, {})
        low, high = row.get("target_tokens_min"), row.get("target_tokens_max")
        if not isinstance(low, int) or not isinstance(high, int) or low <= 0 or high < low:
            errors.append(f"registries/agent_profiles.toml:{name}: invalid token interval")
    for name in ("tactical", "pilot"):
        if profiles.get(name, {}).get("requires_focus") is not True:
            errors.append(f"registries/agent_profiles.toml:{name}.requires_focus: must be true")
    if profiles.get("forensic", {}).get("requires_explicit_scope") is not True:
        errors.append("registries/agent_profiles.toml:forensic.requires_explicit_scope: must be true")
    if profiles.get("custom", {}).get("unknown_projection_behavior") != "fail_closed":
        errors.append("registries/agent_profiles.toml:custom.unknown_projection_behavior: must be fail_closed")


def _validate_graph(document: dict[str, Any], gates: set[str], errors: list[str]) -> None:
    rows = [row for row in document.get("family", []) if isinstance(row, dict)]
    if document.get("source_family_count") != 22 or len(rows) != 22:
        errors.append(f"registries/graph_algorithms.toml: expected 22 source families, found {len(rows)}")
    ordinals = [row.get("source_family_ordinal") for row in rows]
    if set(ordinals) != set(range(1, 23)) or len(ordinals) != len(set(ordinals)):
        errors.append("registries/graph_algorithms.toml: source ordinals must uniquely cover 1..22")
    by_id = {row.get("id"): row for row in rows}
    crosswalk = {
        "GALG-020": (22, "temporal-graphs"),
        "GALG-021": (21, "submodular-selection"),
        "GALG-022": (20, "geometric-and-visibility-graphs"),
    }
    for identifier, expected in crosswalk.items():
        row = by_id.get(identifier, {})
        observed = (row.get("source_family_ordinal"), row.get("name"))
        if observed != expected:
            errors.append(f"registries/graph_algorithms.toml:{identifier}: expected {expected}, found {observed}")
    gate = document.get("optimized_admission_gate")
    if gate not in gates:
        errors.append(f"registries/graph_algorithms.toml.optimized_admission_gate: unknown typed ID {gate!r}")
    if document.get("optimized_family_source") != "franken_networkx":
        errors.append("registries/graph_algorithms.toml.optimized_family_source: expected franken_networkx")
    certificate = document.get("certificate")
    if not isinstance(certificate, dict) or not isinstance(certificate.get("required"), list):
        errors.append("registries/graph_algorithms.toml.certificate.required: expected array")


def _validate_dependency_policy(root: Path, pointer: dict[str, Any], errors: list[str]) -> None:
    expected = {
        "schema": "fdgr.dependency_policy_pointer.v1",
        "revision": 1,
        "policy": "closed_universe",
        "authoritative_path": "architecture/dependency_allowlist.toml",
        "authoritative_schema": "fdgr.dependency_policy.v1",
    }
    for field, value in expected.items():
        if pointer.get(field) != value:
            errors.append(f"registries/dependency_allowlist.toml.{field}: expected {value!r}, found {pointer.get(field)!r}")
    path = pointer.get("authoritative_path")
    if not isinstance(path, str):
        return
    policy = _load(root / path, errors)
    required_root = {
        "schema", "revision", "policy", "exclusive_async_runtime", "fdgr_unsafe_code",
        "c_cpp_ffi", "in_process_python", "hosted_github_actions_authority", "source_closure",
        "forbidden_linked_components", "external_process", "platform", "fundamental_exception",
        "owned_source", "forbidden",
    }
    for field in sorted(required_root - set(policy)):
        errors.append(f"{path}.{field}: missing required field")
    for field in sorted(set(policy) - required_root):
        errors.append(f"{path}.{field}: unknown field")
    scalar = {
        "schema": "fdgr.dependency_policy.v1", "revision": 1, "policy": "closed_universe",
        "exclusive_async_runtime": "asupersync", "fdgr_unsafe_code": "forbid",
        "c_cpp_ffi": False, "in_process_python": False, "hosted_github_actions_authority": False,
    }
    for field, value in scalar.items():
        if policy.get(field) != value:
            errors.append(f"{path}.{field}: expected {value!r}, found {policy.get(field)!r}")
    forbidden_rows = policy.get("forbidden", [])
    forbidden = {row.get("name") for row in forbidden_rows if isinstance(row, dict)}
    if forbidden != REQUIRED_FORBIDDEN_CRATES or len(forbidden_rows) != len(forbidden):
        errors.append(f"{path}: forbidden crate set must equal {sorted(REQUIRED_FORBIDDEN_CRATES)}")
    fundamental_rows = policy.get("fundamental_exception", [])
    fundamental = {row.get("name") for row in fundamental_rows if isinstance(row, dict)}
    if fundamental != REQUIRED_FUNDAMENTAL_EXCEPTIONS:
        errors.append(f"{path}: fundamental exception set must equal {sorted(REQUIRED_FUNDAMENTAL_EXCEPTIONS)}")
    for index, row in enumerate(fundamental_rows):
        if not isinstance(row, dict) or set(row) != {"name", "status", "default_features"} or row.get("default_features") is not False:
            errors.append(f"{path}:fundamental_exception[{index}]: invalid exact-pin/default-features record")
    process_rows = policy.get("external_process", [])
    processes = {row.get("name") for row in process_rows if isinstance(row, dict)}
    if processes != REQUIRED_EXTERNAL_PROCESSES:
        errors.append(f"{path}: external process set must equal {sorted(REQUIRED_EXTERNAL_PROCESSES)}")
    for index, row in enumerate(process_rows):
        fields = {"name", "class", "status", "network_default", "linked_in_process"}
        if not isinstance(row, dict) or set(row) != fields or row.get("network_default") is not False or row.get("linked_in_process") is not False:
            errors.append(f"{path}:external_process[{index}]: invalid bounded sidecar record")
    denials = policy.get("forbidden_linked_components")
    if not isinstance(denials, list) or set(denials) != REQUIRED_LINKED_DENIALS:
        errors.append(f"{path}.forbidden_linked_components: expected {sorted(REQUIRED_LINKED_DENIALS)}")
    names: set[str] = set()
    for index, row in enumerate(policy.get("owned_source", [])):
        fields = {"name", "research_commit", "research_tree", "status", "production_admitted"}
        if not isinstance(row, dict) or set(row) != fields:
            errors.append(f"{path}:owned_source[{index}]: expected exactly {sorted(fields)}")
            continue
        name = row.get("name")
        if name in names:
            errors.append(f"{path}:owned_source[{index}].name: duplicate {name!r}")
        if isinstance(name, str):
            names.add(name)
        for field in ("research_commit", "research_tree"):
            if not isinstance(row.get(field), str) or not HEX40_RE.fullmatch(row[field]):
                errors.append(f"{path}:owned_source[{index}].{field}: expected 40 lowercase hex")
        if row.get("production_admitted") is not False:
            errors.append(f"{path}:owned_source[{index}].production_admitted: must be false before admission")


def validate_registry_contracts(root: Path) -> list[str]:
    errors: list[str] = []
    paths = sorted((root / "registries").glob("*.toml"))
    filenames = {path.name for path in paths}
    for filename in sorted(filenames - set(SPECS)):
        errors.append(f"registries/{filename}: no declared registry contract")
    for filename in sorted(set(SPECS) - filenames):
        errors.append(f"registries/{filename}: required registry missing")
    documents: dict[str, dict[str, Any]] = {}
    all_ids: dict[str, str] = {}
    for path in paths:
        document = _load(path, errors)
        documents[path.name] = document
        if path.name in SPECS:
            _validate_shape(path.name, document, all_ids, errors)
    for filename, document in documents.items():
        for field_path, text in _strings(document, f"registries/{filename}"):
            for identifier in ID_TOKEN_RE.findall(text):
                if identifier not in all_ids:
                    errors.append(f"{field_path}: unknown typed ID {identifier}")
    _validate_profiles(documents.get("agent_profiles.toml", {}), errors)
    gates = {row.get("id") for row in documents.get("gates.toml", {}).get("gate", []) if isinstance(row, dict)}
    _validate_graph(documents.get("graph_algorithms.toml", {}), gates, errors)
    _validate_dependency_policy(root, documents.get("dependency_allowlist.toml", {}), errors)
    capabilities = {row.get("id") for row in documents.get("capabilities.toml", {}).get("capability", []) if isinstance(row, dict)}
    for index, row in enumerate(documents.get("effects.toml", {}).get("effect", [])):
        if isinstance(row, dict) and row.get("capability") not in capabilities:
            errors.append(f"registries/effects.toml:effect[{index}].capability: unknown {row.get('capability')!r}")
    work = {row.get("id") for row in documents.get("work_packages.toml", {}).get("work_package", []) if isinstance(row, dict)}
    for index, row in enumerate(documents.get("work_packages.toml", {}).get("work_package", [])):
        if not isinstance(row, dict):
            continue
        for dependency in row.get("dependencies", []):
            if dependency not in work:
                errors.append(f"registries/work_packages.toml:work_package[{index}].dependencies: unknown {dependency!r}")
        if row.get("acceptance_gate") not in gates:
            errors.append(f"registries/work_packages.toml:work_package[{index}].acceptance_gate: unknown {row.get('acceptance_gate')!r}")
    operation_ids = [row.get("id") for row in documents.get("operation_costs.toml", {}).get("operation", []) if isinstance(row, dict)]
    if len(operation_ids) != len(set(operation_ids)):
        errors.append("registries/operation_costs.toml: duplicate operation identities")
    return sorted(set(errors))


def main(argv: list[str]) -> int:
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parents[1]
    errors = validate_registry_contracts(root)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        print(f"FAILED: {len(errors)} registry contract error(s)", file=sys.stderr)
        return 1
    print(f"PASS: {len(SPECS)} registry contracts and dependency authority validated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
