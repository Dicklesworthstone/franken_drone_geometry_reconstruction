#!/usr/bin/env python3
from __future__ import annotations
import json
import re
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
contract = json.loads((ROOT / "architecture/agent_turn_contract.json").read_text())
schema = json.loads((ROOT / "schemas/agent_turn.schema.json").read_text())
ops = tomllib.load((ROOT / "registries/agent_operations.toml").open("rb"))["operation"]
profiles = tomllib.load((ROOT / "registries/agent_profiles.toml").open("rb"))["profile"]
errors: list[str] = []


def check_vocabulary(node: object, location: str, errors: list[str]) -> None:
    if isinstance(node, dict):
        props = node.get("properties")
        if isinstance(props, dict):
            for name, child in props.items():
                if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
                    errors.append(f"{location}: noncanonical field {name}")
                check_vocabulary(child, f"{location}.{name}", errors)
        defs = node.get("$defs")
        if isinstance(defs, dict):
            for name, child in defs.items():
                if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
                    errors.append(f"{location}: noncanonical definition {name}")
                check_vocabulary(child, f"{location}.$defs.{name}", errors)
        for key, child in node.items():
            if key not in {"properties", "$defs"}:
                check_vocabulary(child, f"{location}.{key}", errors)
    elif isinstance(node, list):
        for index, child in enumerate(node):
            check_vocabulary(child, f"{location}[{index}]", errors)
required = set(schema["required"])
for field in contract["field_order"]:
    if field not in schema["properties"]:
        errors.append(f"agent-turn field {field} is absent from public schema")
for field in ("schema", "operation", "phase", "status", "error", "recovery", "session_id", "turn_id", "anchor", "continuity", "profile", "focus", "decision_frame", "ledgers", "changes", "attention", "affordances", "recommendations", "uncertainty", "coverage", "budget", "references"):
    if field not in required:
        errors.append(f"agent-turn required field missing: {field}")
if {row["name"] for row in profiles} != set(contract["profiles"]):
    errors.append("profile registry and agent-turn contract differ")
logical = [row["logical_name"] for row in ops]
if logical != ["fdgr.open_session","fdgr.orient","fdgr.query","fdgr.propose","fdgr.compare","fdgr.commit","fdgr.watch","fdgr.cancel","fdgr.explain","fdgr.handoff","fdgr.doctor"]:
    errors.append("agent operation narrow waist/order drifted")
for path in sorted((ROOT / "schemas").glob("*.json")):
    data=json.loads(path.read_text())
    if data.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        errors.append(f"{path.name}: wrong JSON Schema dialect")
    schema_const = data.get("properties", {}).get("schema", {}).get("const")
    if not isinstance(schema_const, str) or not re.fullmatch(r"fdgr\.[a-z][a-z0-9_]*/1", schema_const):
        errors.append(f"{path.name}: noncanonical payload schema identity")
    check_vocabulary(data, path.name, errors)

anchor_fields = {
    "agent_turn.schema.json": ["anchor"],
    "attention_item.schema.json": ["first_seen_anchor", "last_changed_anchor"],
    "context_pack.schema.json": ["anchor"],
    "decision_frame.schema.json": ["anchor"],
    "episode_capsule.schema.json": ["start_anchor", "end_anchor"],
    "handoff_capsule.schema.json": ["anchor"],
    "obligation_progress.schema.json": ["anchor"],
    "pilot_instruction.schema.json": ["basis_anchor"],
    "plan_candidate.schema.json": ["basis_anchor"],
    "spatial_handle.schema.json": ["anchor"],
}
for filename, fields in anchor_fields.items():
    data = json.loads((ROOT / "schemas" / filename).read_text())
    for field in fields:
        if data.get("properties", {}).get(field) != {"$ref": "anchor_vector.schema.json"}:
            errors.append(f"{filename}: {field} does not use the canonical anchor-vector schema")

if errors:
    raise SystemExit("\n".join(f"ERROR: {e}" for e in errors))
print(f"PASS: agent contracts ({len(ops)} operations, {len(profiles)} profiles, {len(contract['required_invariants'])} agent invariants)")
