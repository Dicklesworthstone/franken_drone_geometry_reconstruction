#!/usr/bin/env python3
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import sys
import tomllib
from collections import deque
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
WORK_PACKAGES = ROOT / "registries" / "work_packages.toml"
SCHEMA = "fdgr.vertical_slice_closure/1"
SLICE_ID = "VS-01"
ROOT_WORK_PACKAGE = "WP-018"
REQUIRED_DIRECT_DEPENDENCIES = ("WP-009", "WP-014")
FORBIDDEN_TRANSITIVE_PACKAGES = frozenset(
    {
        "WP-015",
        "WP-016",
        "WP-017",
        "WP-019",
        "WP-020",
        "WP-021",
        "WP-022",
        "WP-023",
        "WP-024",
        "WP-025",
    }
)
REASONS = {
    "WP-000": "normative contracts and schemas precede every implementation path",
    "WP-001": "the safe-Rust workspace and closed dependency policy are required to build the path",
    "WP-002": "canonical identities and replayable evidence bind every geometry generation",
    "WP-003": "durable state semantics are inherited by publication and later effectful execution",
    "WP-004": "immutable local object custody anchors original and derived evidence",
    "WP-005": "original evidence must be preserved before geometry is derived",
    "WP-007": "bounded recorded-media inspection supplies exact source-frame evidence",
    "WP-008": "canonical media time and explicit clock epochs bind frame observations",
    "WP-009": "calibration and explicit scale-witness authority are prerequisites for honest pose claims",
    "WP-013": "deterministic quality and coverage gates choose the reference keyframes",
    "WP-014": "the model-free feature, track, and relative-pose path supplies classical geometry evidence",
    "WP-018": "the slice root performs deterministic pose-graph, refinement, bundle, and reprojection work",
}


class ClosureError(ValueError):
    pass


def load_registry(path: Path = WORK_PACKAGES) -> dict[str, dict[str, Any]]:
    with path.open("rb") as handle:
        document = tomllib.load(handle)
    rows = document.get("work_package")
    if not isinstance(rows, list):
        raise ClosureError("work-package registry has no work_package array")
    by_id: dict[str, dict[str, Any]] = {}
    for row in rows:
        if not isinstance(row, dict):
            raise ClosureError("work-package registry contains a non-table row")
        identifier = row.get("id")
        dependencies = row.get("dependencies")
        if not isinstance(identifier, str):
            raise ClosureError("work-package row has no string id")
        if identifier in by_id:
            raise ClosureError(f"duplicate work-package id {identifier}")
        if not isinstance(dependencies, list) or not all(
            isinstance(dependency, str) for dependency in dependencies
        ):
            raise ClosureError(f"{identifier} dependencies must be an array of strings")
        by_id[identifier] = copy.deepcopy(row)
    return by_id


def validate_graph(by_id: dict[str, dict[str, Any]]) -> list[str]:
    indegree = {identifier: 0 for identifier in by_id}
    children = {identifier: [] for identifier in by_id}
    for identifier in sorted(by_id):
        dependencies = by_id[identifier]["dependencies"]
        if dependencies != sorted(dependencies):
            raise ClosureError(f"{identifier} dependencies are not canonically sorted")
        if len(dependencies) != len(set(dependencies)):
            raise ClosureError(f"{identifier} repeats a dependency")
        for dependency in dependencies:
            if dependency == identifier:
                raise ClosureError(f"{identifier} depends on itself")
            if dependency not in by_id:
                raise ClosureError(f"{identifier} depends on unknown {dependency}")
            indegree[identifier] += 1
            children[dependency].append(identifier)
    queue = deque(sorted(identifier for identifier, degree in indegree.items() if degree == 0))
    ordered: list[str] = []
    while queue:
        identifier = queue.popleft()
        ordered.append(identifier)
        for child in sorted(children[identifier]):
            indegree[child] -= 1
            if indegree[child] == 0:
                queue.append(child)
    if len(ordered) != len(by_id):
        cyclic = sorted(identifier for identifier, degree in indegree.items() if degree > 0)
        raise ClosureError(f"work-package graph contains a cycle involving {cyclic}")
    return ordered


def dependency_closure(
    by_id: dict[str, dict[str, Any]], root: str = ROOT_WORK_PACKAGE
) -> list[str]:
    if root not in by_id:
        raise ClosureError(f"slice root {root} is absent")
    seen: set[str] = set()
    stack = [root]
    while stack:
        identifier = stack.pop()
        if identifier in seen:
            continue
        seen.add(identifier)
        stack.extend(reversed(by_id[identifier]["dependencies"]))
    return sorted(seen)


def validate_vs01(by_id: dict[str, dict[str, Any]]) -> list[str]:
    validate_graph(by_id)
    direct = tuple(by_id[ROOT_WORK_PACKAGE]["dependencies"])
    if direct != REQUIRED_DIRECT_DEPENDENCIES:
        raise ClosureError(
            f"{ROOT_WORK_PACKAGE} direct dependencies must be "
            f"{list(REQUIRED_DIRECT_DEPENDENCIES)}, observed {list(direct)}"
        )
    closure = dependency_closure(by_id)
    forbidden = sorted(FORBIDDEN_TRANSITIVE_PACKAGES.intersection(closure))
    if forbidden:
        raise ClosureError(
            f"{SLICE_ID} model-free closure contains later-slice packages {forbidden}"
        )
    missing_reasons = sorted(set(closure).difference(REASONS))
    if missing_reasons:
        raise ClosureError(
            f"{SLICE_ID} closure lacks prerequisite reasons for {missing_reasons}"
        )
    depth_package = by_id.get("WP-019")
    if depth_package is None or depth_package.get("dependencies") != [
        "WP-016",
        "WP-017",
        "WP-018",
    ]:
        raise ClosureError(
            "WP-019 must retain learned and classical depth prerequisites after the VS-01 split"
        )
    return closure


def registry_digest(path: Path = WORK_PACKAGES) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def receipt(by_id: dict[str, dict[str, Any]], source_digest: str) -> dict[str, Any]:
    closure = validate_vs01(by_id)
    return {
        "schema": SCHEMA,
        "slice": SLICE_ID,
        "root_work_package": ROOT_WORK_PACKAGE,
        "work_package_registry_sha256": source_digest,
        "closure_count": len(closure),
        "closure": [
            {
                "work_package": identifier,
                "reason": REASONS[identifier],
                "direct_dependencies": by_id[identifier]["dependencies"],
            }
            for identifier in closure
        ],
        "forbidden_package_classes_absent": [
            "learned_model_worker",
            "learned_geometry_lane",
            "streaming_model_research",
            "depth_ensemble",
            "geometry_fusion",
            "semantic_resolution",
            "live_device",
            "cloud_replication",
            "full_product_reporting",
        ],
        "later_depth_lane_preserved": True,
        "verdict": "pass",
    }


def expect_failure(
    label: str,
    by_id: dict[str, dict[str, Any]],
    expected_fragment: str,
) -> None:
    try:
        validate_vs01(by_id)
    except ClosureError as error:
        if expected_fragment not in str(error):
            raise ClosureError(
                f"mutation {label} failed for the wrong reason: {error}"
            ) from error
    else:
        raise ClosureError(f"mutation {label} was incorrectly accepted")


def run_self_test(source: dict[str, dict[str, Any]]) -> None:
    learned = copy.deepcopy(source)
    learned[ROOT_WORK_PACKAGE]["dependencies"] = ["WP-009", "WP-014", "WP-016"]
    expect_failure("learned model in VS-01", learned, "direct dependencies")

    missing_scale = copy.deepcopy(source)
    missing_scale[ROOT_WORK_PACKAGE]["dependencies"] = ["WP-014"]
    expect_failure("missing scale/calibration", missing_scale, "direct dependencies")

    cycle = copy.deepcopy(source)
    cycle["WP-000"]["dependencies"] = [ROOT_WORK_PACKAGE]
    expect_failure("dependency cycle", cycle, "cycle")

    dangling = copy.deepcopy(source)
    dangling[ROOT_WORK_PACKAGE]["dependencies"] = ["WP-009", "WP-014", "WP-999"]
    expect_failure("dangling dependency", dangling, "unknown WP-999")

    duplicate = copy.deepcopy(source)
    duplicate[ROOT_WORK_PACKAGE]["dependencies"] = ["WP-009", "WP-009", "WP-014"]
    expect_failure("duplicate dependency", duplicate, "repeats a dependency")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prove the deterministic model-free VS-01 work-package closure"
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()
    try:
        by_id = load_registry()
        value = receipt(by_id, registry_digest())
        if arguments.self_test:
            run_self_test(by_id)
    except (OSError, tomllib.TOMLDecodeError, ClosureError) as error:
        print(f"ERROR: vertical-slice closure rejected: {error}", file=sys.stderr)
        return 1
    rendered = json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n"
    if arguments.output is not None:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(rendered, encoding="utf-8")
    else:
        sys.stdout.write(rendered)
    if arguments.self_test:
        print("PASS: VS-01 closure mutation fixtures refused", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
