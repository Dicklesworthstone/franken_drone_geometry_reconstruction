#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import shutil
import sys
import tempfile
from pathlib import Path
from typing import Callable

from registry_contracts import validate_registry_contracts

ROOT = Path(__file__).resolve().parents[1]
Mutation = Callable[[Path], None]


def _write(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def _replace(path: Path, old: str, new: str, count: int = 1) -> None:
    text = path.read_text(encoding="utf-8")
    if old not in text:
        raise RuntimeError(f"mutation precondition missing in {path}: {old!r}")
    _write(path, text.replace(old, new, count))


def _copy_contract_root(destination: Path) -> None:
    shutil.copytree(ROOT / "registries", destination / "registries")
    (destination / "architecture").mkdir()
    shutil.copy2(
        ROOT / "architecture" / "dependency_allowlist.toml",
        destination / "architecture" / "dependency_allowlist.toml",
    )


def _contract_digest(root: Path) -> str:
    digest = hashlib.sha256()
    paths = sorted((root / "registries").glob("*.toml")) + [
        root / "architecture" / "dependency_allowlist.toml"
    ]
    for path in paths:
        relative = path.relative_to(root).as_posix().encode("utf-8")
        data = path.read_bytes()
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()


def _emit(payload: dict[str, object]) -> None:
    print(json.dumps(payload, sort_keys=True, separators=(",", ":")))


def _remove_galg_022(root: Path) -> None:
    path = root / "registries" / "graph_algorithms.toml"
    text = path.read_text(encoding="utf-8")
    start = text.index('[[family]]\nid = "GALG-022"')
    end = text.index("\n[certificate]", start)
    _write(path, text[:start] + text[end + 1 :])


def _remove_tokio_forbidden(root: Path) -> None:
    path = root / "architecture" / "dependency_allowlist.toml"
    text = path.read_text(encoding="utf-8")
    block = '''[[forbidden]]
name = "tokio"
reason = "second runtime"

'''
    if block not in text:
        raise RuntimeError("tokio forbidden block not found")
    _write(path, text.replace(block, "", 1))


def _legacy_open_question(root: Path) -> None:
    path = root / "registries" / "open_questions.toml"
    _replace(
        path,
        '''id = "OPEN-026"
name = "Decision Frame density"
statement = "What Decision Frame density maximizes first-turn decision success across model context sizes without hiding meaningful alternatives?"''',
        '''id = "OPEN-026"
question = "What Decision Frame density maximizes first-turn decision success across model context sizes without hiding meaningful alternatives?"''',
    )


def _legacy_risk(root: Path) -> None:
    path = root / "registries" / "risks.toml"
    old = next(
        block
        for block in path.read_text(encoding="utf-8").split("\n\n")
        if 'id = "RISK-021"' in block
    )
    new = '''[[risk]]
id = "RISK-021"
name = "Attention thrash"
summary = "Repeated model outputs could interrupt the agent."
mitigation = "Use hysteresis."'''
    _replace(path, old, new)


def _legacy_operation_cost(root: Path) -> None:
    path = root / "registries" / "operation_costs.toml"
    with path.open("a", encoding="utf-8") as handle:
        handle.write(
            '''\n[[operation_cost]]
id = "OP-LEGACY-COST"
name = "Legacy cost"
unit = "call"
required_dimensions = ["output_tokens"]
'''
        )


def mutation_cases() -> list[tuple[str, Mutation, str]]:
    return [
        (
            "missing_capability_authority",
            lambda root: _replace(
                root / "registries" / "capabilities.toml",
                '''id = "CAP-PILOT-GUIDANCE"
authority = "generate bounded evidence-gain pilot cards for an operator-present session; grants no aircraft-control authority"
status = "planned_sensitive"''',
                '''id = "CAP-PILOT-GUIDANCE"
status = "planned_sensitive"''',
            ),
            "id=CAP-PILOT-GUIDANCE.authority: missing required field",
        ),
        (
            "unknown_capability_field",
            lambda root: _replace(
                root / "registries" / "capabilities.toml",
                'status = "planned_sensitive"\n\n[[capability]]\nid = "CAP-SPATIAL-EXPAND"',
                'status = "planned_sensitive"\nsummary = "legacy field"\n\n[[capability]]\nid = "CAP-SPATIAL-EXPAND"',
            ),
            "id=CAP-PILOT-GUIDANCE.summary: unknown field",
        ),
        (
            "dangling_typed_id",
            lambda root: _replace(
                root / "registries" / "graph_algorithms.toml",
                'optimized_admission_gate = "GATE-015"',
                'optimized_admission_gate = "GATE-709"',
            ),
            "unknown typed ID GATE-709",
        ),
        ("graph_family_omission", _remove_galg_022, "expected 22 source families, found 21"),
        (
            "duplicate_source_ordinal",
            lambda root: _replace(
                root / "registries" / "graph_algorithms.toml",
                '''id = "GALG-022"
source_family_ordinal = 20''',
                '''id = "GALG-022"
source_family_ordinal = 21''',
            ),
            "source ordinals must uniquely cover 1..22",
        ),
        (
            "published_id_renumber",
            lambda root: _replace(
                root / "registries" / "graph_algorithms.toml",
                'id = "GALG-020"',
                'id = "GALG-023"',
            ),
            "GALG-020: expected (22, 'temporal-graphs'), found (None, None)",
        ),
        (
            "wrong_source_crosswalk",
            lambda root: _replace(
                root / "registries" / "graph_algorithms.toml",
                '''id = "GALG-022"
source_family_ordinal = 20
name = "geometric-and-visibility-graphs"''',
                '''id = "GALG-022"
source_family_ordinal = 20
name = "temporal-graphs"''',
            ),
            "GALG-022: expected (20, 'geometric-and-visibility-graphs')",
        ),
        ("legacy_open_question_shape", _legacy_open_question, "id=OPEN-026.name: missing required field"),
        ("legacy_risk_shape", _legacy_risk, "id=RISK-021.statement: missing required field"),
        ("legacy_operation_cost_collection", _legacy_operation_cost, "operation_cost: unknown root field"),
        (
            "dependency_pointer_drift",
            lambda root: _replace(
                root / "registries" / "dependency_allowlist.toml",
                'authoritative_path = "architecture/dependency_allowlist.toml"',
                'authoritative_path = "registries/dependency_allowlist.toml"',
            ),
            "authoritative_path: expected 'architecture/dependency_allowlist.toml'",
        ),
        ("forbidden_crate_omission", _remove_tokio_forbidden, "forbidden crate set must equal"),
        (
            "external_process_network_default",
            lambda root: _replace(
                root / "architecture" / "dependency_allowlist.toml",
                "network_default = false",
                "network_default = true",
            ),
            "invalid bounded sidecar record",
        ),
        (
            "owned_source_premature_admission",
            lambda root: _replace(
                root / "architecture" / "dependency_allowlist.toml",
                "production_admitted = false",
                "production_admitted = true",
            ),
            "production_admitted: must be false before admission",
        ),
    ]


def main() -> int:
    baseline_errors = validate_registry_contracts(ROOT)
    if baseline_errors:
        _emit({
            "schema": "fdgr.test_receipt/1",
            "suite": "registry_contracts",
            "verdict": "failed_baseline",
            "diagnostics": baseline_errors,
        })
        return 1
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="fdgr-registry-contracts-") as temporary:
        temp_root = Path(temporary)
        for case_id, mutation, expected in mutation_cases():
            case_root = temp_root / case_id
            case_root.mkdir()
            _copy_contract_root(case_root)
            mutation(case_root)
            diagnostics = validate_registry_contracts(case_root)
            matched = any(expected in diagnostic for diagnostic in diagnostics)
            verdict = "pass" if matched else "fail"
            if not matched:
                failures.append(case_id)
            _emit({
                "schema": "fdgr.test_event/1",
                "suite": "registry_contracts",
                "case_id": case_id,
                "expected_diagnostic": expected,
                "observed_diagnostics": diagnostics,
                "mutated_contract_digest": _contract_digest(case_root),
                "verdict": verdict,
            })
    _emit({
        "schema": "fdgr.test_receipt/1",
        "suite": "registry_contracts",
        "baseline_contract_digest": _contract_digest(ROOT),
        "case_count": len(mutation_cases()),
        "failed_cases": failures,
        "verdict": "pass" if not failures else "fail",
    })
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
