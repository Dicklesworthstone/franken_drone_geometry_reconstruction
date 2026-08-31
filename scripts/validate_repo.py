
#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import sys
import tomllib
from urllib.parse import unquote
from collections import defaultdict, deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    'README.md',
    'ARCHITECTURE.md',
    'FRANKENSTACK_DEEP_DIVE.md',
    'COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md',
    'IMPLEMENTATION_STATUS.md',
    'DEPENDENCY_POLICY.md',
    'MODEL_REGISTRY.md',
    'DJI_ADAPTER_RESEARCH.md',
    'SECURITY.md',
    'PRIVACY.md',
    'AGENTS.md',
    'LICENSE',
    'Cargo.toml',
    'Cargo.lock',
    'rust-toolchain.toml',
    'DESIGN_INDEX.md',
    'LOCAL_QUALIFICATION_AND_RELEASE.md',
    'docs/AGENT_OPERATING_MODEL.md',
    'docs/AGENT_QUICKSTART.md',
    'docs/AGENT_ACCEPTANCE_SCENARIOS.md',
    'architecture/SEMANTICS_MANIFEST.md',
    'architecture/AGENT_ABSTRACTION_TOWER.md',
    'architecture/AGENT_NARROW_WAIST.md',
    'architecture/DECISION_FRAME.md',
    'architecture/ATTENTION_AND_EPISTEMIC_DEBT.md',
    'architecture/SPATIAL_SEMANTIC_HANDLES.md',
    'architecture/HUMAN_AGENT_FLIGHT_PROTOCOL.md',
    'architecture/AGENT_METRICS.md',
    'architecture/agent_turn_contract.json',
    'architecture/dependency_allowlist.toml',
    'architecture/deep_traceability.json',
    'architecture/qualification_lanes.toml',
    'release/source_closure.lock.json',
    'release/doodlestein_job_graph.json',
    'scripts/emit_local_qualification_receipt.py',
    'scripts/verify_source_closure.py',
]
ID_PATTERN = re.compile(r'^(?:INV|BET|GOAL|NONGOAL|CAP|EFFECT|CLAIM|ERR|SCHEMA|ADR|WP|GATE|TEST|SLO|RISK|OPEN|MODEL|OP|GEOM|GALG)-[A-Z0-9-]+$')
FORBIDDEN_RUST = {
    'unsafe block': re.compile(r'\bunsafe\s*\{'),
    'unsafe function': re.compile(r'\bunsafe\s+fn\b'),
    'unsafe impl': re.compile(r'\bunsafe\s+impl\b'),
    'unwrap': re.compile(r'\.unwrap\s*\('),
    'expect': re.compile(r'\.expect\s*\('),
    'panic macro': re.compile(r'\bpanic!\s*\('),
    'todo macro': re.compile(r'\btodo!\s*\('),
    'unimplemented macro': re.compile(r'\bunimplemented!\s*\('),
    'dbg macro': re.compile(r'\bdbg!\s*\('),
}
FORBIDDEN_CARGO = {'tokio', 'rayon', 'reqwest', 'hyper', 'axum', 'tower', 'sqlx', 'diesel', 'sea-orm'}
TEXT_FILENAMES = {'.gitignore', 'Cargo.lock'}
TEXT_SUFFIXES = {'.md', '.toml', '.json', '.jsonl', '.rs', '.py', '.sh', '.yml', '.yaml', '.example'}
MARKDOWN_LINK = re.compile(r'(?<!!)\[[^\]]*\]\(([^)]+)\)')

errors: list[str] = []
notes: list[str] = []

def fail(message: str) -> None:
    errors.append(message)

def load_toml(path: Path) -> dict:
    try:
        with path.open('rb') as handle:
            return tomllib.load(handle)
    except Exception as exc:
        fail(f'{path.relative_to(ROOT)}: invalid TOML: {exc}')
        return {}

def load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except Exception as exc:
        fail(f'{path.relative_to(ROOT)}: invalid JSON: {exc}')
        return {}

def validate_schema_vocabulary(node: object, location: str) -> None:
    if isinstance(node, dict):
        properties = node.get('properties')
        if isinstance(properties, dict):
            for name, child in properties.items():
                if not re.fullmatch(r'[a-z][a-z0-9_]*', name):
                    fail(f'{location}: noncanonical public field name {name!r}')
                validate_schema_vocabulary(child, f'{location}.properties.{name}')
        definitions = node.get('$defs')
        if isinstance(definitions, dict):
            for name, child in definitions.items():
                if not re.fullmatch(r'[a-z][a-z0-9_]*', name):
                    fail(f'{location}: noncanonical $defs name {name!r}')
                validate_schema_vocabulary(child, f'{location}.$defs.{name}')
        required = node.get('required')
        if isinstance(required, list):
            for name in required:
                if isinstance(name, str) and not re.fullmatch(r'[a-z][a-z0-9_]*', name):
                    fail(f'{location}: noncanonical required field {name!r}')
        enum = node.get('enum')
        if isinstance(enum, list):
            for value in enum:
                if isinstance(value, str) and not re.fullmatch(r'[a-z][a-z0-9_./:-]*', value):
                    fail(f'{location}: noncanonical enum value {value!r}')
        for key, child in node.items():
            if key not in {'properties', '$defs'}:
                validate_schema_vocabulary(child, f'{location}.{key}')
    elif isinstance(node, list):
        for index, child in enumerate(node):
            validate_schema_vocabulary(child, f'{location}[{index}]')

for relative in REQUIRED:
    if not (ROOT / relative).is_file():
        fail(f'missing required file: {relative}')

BEADS_TRACKED = {'bootstrap.jsonl', 'README.md', 'metadata.json', 'config.yaml', 'issues.jsonl'}

for path in ROOT.rglob('*'):
    if (
        not path.is_file()
        or any(p in {'.git', '.ee', '.fdgr', 'target', '.br_history', '__pycache__'} for p in path.parts)
        or path.name == '.DS_Store'
        or ('.beads' in path.parts and path.name not in BEADS_TRACKED)
    ):
        continue
    data = path.read_bytes()
    if b'\r\n' in data:
        fail(f'{path.relative_to(ROOT)}: CRLF line endings are forbidden')
    if data and not data.endswith(b'\n'):
        fail(f'{path.relative_to(ROOT)}: text file must end with newline')

registries: dict[str, dict] = {}
for path in sorted((ROOT / 'registries').glob('*.toml')):
    registries[path.name] = load_toml(path)
if not registries:
    fail('no machine registries found')

all_ids: dict[str, str] = {}
for filename, document in registries.items():
    if not isinstance(document.get('schema'), str):
        fail(f'registries/{filename}: missing string schema identifier')
    if document.get('revision') != 1:
        fail(f'registries/{filename}: initial registry revision must be 1')
    for key, value in document.items():
        if not isinstance(value, list):
            continue
        for entry in value:
            if not isinstance(entry, dict) or 'id' not in entry:
                continue
            identifier = entry['id']
            if not isinstance(identifier, str) or not ID_PATTERN.fullmatch(identifier):
                fail(f'registries/{filename}: invalid stable ID {identifier!r}')
                continue
            previous = all_ids.get(identifier)
            if previous is not None:
                fail(f'duplicate stable ID {identifier}: {previous} and registries/{filename}')
            all_ids[identifier] = f'registries/{filename}'

work_doc = registries.get('work_packages.toml', {})
gate_doc = registries.get('gates.toml', {})
work_items = {item.get('id'): item for item in work_doc.get('work_package', []) if isinstance(item, dict)}
gates = {item.get('id') for item in gate_doc.get('gate', []) if isinstance(item, dict)}
allowed_work_statuses = {'planned', 'draft', 'scaffolded', 'in_progress', 'blocked', 'qualified', 'retired'}
for identifier, item in work_items.items():
    if item.get('status') not in allowed_work_statuses:
        fail(f'{identifier}: unknown status {item.get("status")!r}')
    for dependency in item.get('dependencies', []):
        if dependency not in work_items:
            fail(f'{identifier}: unknown work-package dependency {dependency}')
    if item.get('acceptance_gate') not in gates:
        fail(f'{identifier}: unknown acceptance gate {item.get("acceptance_gate")!r}')

indegree = {identifier: 0 for identifier in work_items}
children: dict[str, list[str]] = defaultdict(list)
allowed_work_statuses = {'planned', 'draft', 'scaffolded', 'in_progress', 'blocked', 'qualified', 'retired'}
for identifier, item in work_items.items():
    if item.get('status') not in allowed_work_statuses:
        fail(f'{identifier}: unknown status {item.get("status")!r}')
    for dependency in item.get('dependencies', []):
        indegree[identifier] += 1
        children[dependency].append(identifier)
queue = deque(sorted(identifier for identifier, degree in indegree.items() if degree == 0))
visited: list[str] = []
while queue:
    identifier = queue.popleft()
    visited.append(identifier)
    for child in sorted(children[identifier]):
        indegree[child] -= 1
        if indegree[child] == 0:
            queue.append(child)
if len(visited) != len(work_items):
    fail('work-package dependency graph contains a cycle')

schema_ids: set[str] = set()
schema_documents: dict[str, dict] = {}
for path in sorted((ROOT / 'schemas').glob('*.json')):
    document = load_json(path)
    if not isinstance(document, dict):
        fail(f'{path.relative_to(ROOT)}: schema root must be an object')
        continue
    schema_id = document.get('$id')
    if not isinstance(schema_id, str):
        fail(f'{path.relative_to(ROOT)}: missing $id')
    elif schema_id in schema_ids:
        fail(f'{path.relative_to(ROOT)}: duplicate $id {schema_id}')
    else:
        schema_ids.add(schema_id)
    if document.get('$schema') != 'https://json-schema.org/draft/2020-12/schema':
        fail(f'{path.relative_to(ROOT)}: schema must use JSON Schema 2020-12')
    schema_const = document.get('properties', {}).get('schema', {}).get('const') if isinstance(document.get('properties'), dict) else None
    if not isinstance(schema_const, str) or not re.fullmatch(r'fdgr\.[a-z][a-z0-9_]*/1', schema_const):
        fail(f'{path.relative_to(ROOT)}: payload schema identity must use fdgr.<name>/1')
    validate_schema_vocabulary(document, str(path.relative_to(ROOT)))
    def check_refs(node: object) -> None:
        if isinstance(node, dict):
            reference = node.get('$ref')
            if isinstance(reference, str) and not reference.startswith(('#', 'http://', 'https://')):
                target = reference.split('#', 1)[0]
                if target and not (path.parent / target).is_file():
                    fail(f'{path.relative_to(ROOT)}: unresolved schema reference {reference!r}')
            for child in node.values():
                check_refs(child)
        elif isinstance(node, list):
            for child in node:
                check_refs(child)
    check_refs(document)
    schema_documents[str(path.relative_to(ROOT))] = document


capability_ids = {
    item.get('id')
    for item in registries.get('capabilities.toml', {}).get('capability', [])
    if isinstance(item, dict) and isinstance(item.get('id'), str)
}
for effect in registries.get('effects.toml', {}).get('effect', []):
    if isinstance(effect, dict) and effect.get('capability') not in capability_ids:
        fail(f"{effect.get('id')}: unknown capability {effect.get('capability')!r}")

for schema_entry in registries.get('schemas.toml', {}).get('public_schema', []):
    if not isinstance(schema_entry, dict):
        continue
    path = schema_entry.get('path')
    document = schema_documents.get(path) if isinstance(path, str) else None
    if document is None:
        fail(f"{schema_entry.get('id')}: unknown schema path {path!r}")
    elif schema_entry.get('json_schema_id') != document.get('$id'):
        fail(f"{schema_entry.get('id')}: registry $id disagrees with {path}")

for adr in registries.get('adrs.toml', {}).get('adr', []):
    if isinstance(adr, dict):
        path = adr.get('path')
        if not isinstance(path, str) or not (ROOT / path).is_file():
            fail(f"{adr.get('id')}: ADR path does not exist: {path!r}")

for model in registries.get('models.toml', {}).get('model', []):
    if isinstance(model, dict) and model.get('network_default') is not False:
        fail(f"{model.get('id')}: model workers must default to no network")

for path in sorted(ROOT.glob('crates/*/src/*.rs')):
    text = path.read_text(encoding='utf-8')
    if '#![forbid(unsafe_code)]' not in text:
        fail(f'{path.relative_to(ROOT)}: missing #![forbid(unsafe_code)]')
    for label, pattern in FORBIDDEN_RUST.items():
        if pattern.search(text):
            fail(f'{path.relative_to(ROOT)}: forbidden {label}')

for path in [ROOT / 'Cargo.toml', *sorted(ROOT.glob('crates/*/Cargo.toml'))]:
    document = load_toml(path)
    for section in ('dependencies', 'dev-dependencies', 'build-dependencies'):
        dependencies = document.get(section, {})
        if isinstance(dependencies, dict):
            for name in dependencies:
                if name in FORBIDDEN_CARGO:
                    fail(f'{path.relative_to(ROOT)}: forbidden dependency {name}')

readme = (ROOT / 'README.md').read_text(encoding='utf-8') if (ROOT / 'README.md').exists() else ''
status = (ROOT / 'IMPLEMENTATION_STATUS.md').read_text(encoding='utf-8') if (ROOT / 'IMPLEMENTATION_STATUS.md').exists() else ''
qualification = (ROOT / 'QUALIFICATION.md').read_text(encoding='utf-8') if (ROOT / 'QUALIFICATION.md').exists() else ''
for phrase in ('does **not** yet claim a live', 'Model outputs are proposals', 'Scale Is a Proof Obligation'):
    if phrase not in readme:
        fail(f'README.md: missing constitutional phrase {phrase!r}')
for phrase in (
    'DJI Fly or controller live-view acquisition | Research only',
    'No measured results',
    'pinned-toolchain compile receipt remains pending',
):
    if phrase not in status:
        fail(f'IMPLEMENTATION_STATUS.md: missing honest-status phrase {phrase!r}')
for phrase in ('Rust execution lanes pending', 'were **not executed here**', './scripts/qualify.sh'):
    if phrase not in qualification:
        fail(f'QUALIFICATION.md: missing receipt-boundary phrase {phrase!r}')

plan_path = ROOT / 'COMPREHENSIVE_PLAN_FOR_FRANKEN_DRONE_GEOMETRY_RECONSTRUCTION.md'
if plan_path.exists():
    plan = plan_path.read_text(encoding='utf-8')
    for identifier in sorted(all_ids):
        if identifier not in plan:
            fail(f'comprehensive plan does not mention normative ID {identifier}')
    if '<!-- BEGIN GENERATED REGISTRY TRACEABILITY -->' not in plan or '<!-- END GENERATED REGISTRY TRACEABILITY -->' not in plan:
        fail('comprehensive plan is missing the generated traceability appendix markers')
    if len(plan.splitlines()) < 3000:
        fail('comprehensive plan is unexpectedly short')

# Agent and source-contract integrity.
agent_contract = load_json(ROOT / 'architecture/agent_turn_contract.json')
agent_schema = schema_documents.get('schemas/agent_turn.schema.json', {})
if isinstance(agent_contract, dict) and isinstance(agent_schema, dict):
    for field in agent_contract.get('field_order', []):
        if field not in agent_schema.get('properties', {}):
            fail(f'agent turn contract field missing from schema: {field}')
    profile_names = {entry.get('name') for entry in registries.get('agent_profiles.toml', {}).get('profile', []) if isinstance(entry, dict)}
    if profile_names != set(agent_contract.get('profiles', {})):
        fail('agent profile registry and agent turn contract differ')
source_manifest = load_json(ROOT / 'research/source-inventory/source_manifest.json')
if isinstance(source_manifest, dict):
    repositories = source_manifest.get('repositories', [])
    if len(repositories) != 11:
        fail('source inventory must retain exactly eleven inspected sibling repositories')
    for repository in repositories:
        if not re.fullmatch(r'[0-9a-f]{40}', str(repository.get('commit', ''))) or not re.fullmatch(r'[0-9a-f]{40}', str(repository.get('tree', ''))):
            fail(f"source inventory has invalid identity for {repository.get('name')}")
    closure_document = load_json(ROOT / 'release/source_closure.lock.json')
    closure_rows = closure_document.get('planned_owned_sources', []) if isinstance(closure_document, dict) else []
    manifest_identities = {(row.get('name'), row.get('commit'), row.get('tree')) for row in repositories if isinstance(row, dict)}
    closure_identities = {(row.get('name'), row.get('commit'), row.get('tree')) for row in closure_rows if isinstance(row, dict)}
    if manifest_identities != closure_identities:
        fail('source inventory and source-closure lock disagree')
qualification_lanes = load_toml(ROOT / 'architecture/qualification_lanes.toml')
if qualification_lanes.get('revision') != 1 or qualification_lanes.get('hosted_github_actions_authority') is not False:
    fail('qualification lanes must be revision 1 and local-authoritative')
lane_rows = [lane for lane in qualification_lanes.get('lane', []) if isinstance(lane, dict)]
lane_ids = [lane.get('id') for lane in lane_rows]
if len(lane_ids) != len(set(lane_ids)):
    fail('qualification lane identities are not unique')
lane_indegree = {identifier: 0 for identifier in lane_ids}
lane_children: dict[str, list[str]] = defaultdict(list)
for lane in lane_rows:
    identifier = lane.get('id')
    for dependency in lane.get('requires', []):
        if dependency not in lane_indegree:
            fail(f"qualification lane {identifier} has unknown dependency {dependency}")
            continue
        lane_indegree[identifier] += 1
        lane_children[dependency].append(identifier)
lane_queue = deque(sorted(identifier for identifier, degree in lane_indegree.items() if degree == 0))
lane_visited: list[str] = []
while lane_queue:
    identifier = lane_queue.popleft()
    lane_visited.append(identifier)
    for child in sorted(lane_children[identifier]):
        lane_indegree[child] -= 1
        if lane_indegree[child] == 0:
            lane_queue.append(child)
if len(lane_visited) != len(lane_ids):
    fail('qualification-lane dependency graph contains a cycle')

doodlestein = load_json(ROOT / 'release/doodlestein_job_graph.json')
if isinstance(doodlestein, dict):
    if doodlestein.get('schema') != 'fdgr.doodlestein_job_graph/1':
        fail('Doodlestein job graph has the wrong schema')
    if doodlestein.get('authority') != 'local_receipts_only' or doodlestein.get('hosted_github_actions_authority') is not False:
        fail('Doodlestein job graph must keep local receipts as the only authority')
    job_rows = [job for job in doodlestein.get('jobs', []) if isinstance(job, dict)]
    job_ids = [job.get('id') for job in job_rows]
    if len(job_ids) != len(set(job_ids)):
        fail('Doodlestein job identities are not unique')
    job_indegree = {identifier: 0 for identifier in job_ids}
    job_children: dict[str, list[str]] = defaultdict(list)
    for job in job_rows:
        identifier = job.get('id')
        command = job.get('command')
        if not isinstance(command, str) or not command.strip():
            fail(f'Doodlestein job {identifier} has no command')
        elif command.startswith('python3 scripts/'):
            script_name = command.split()[1]
            if not (ROOT / script_name).is_file():
                fail(f'Doodlestein job {identifier} names missing script {script_name}')
        for dependency in job.get('needs', []):
            if dependency not in job_indegree:
                fail(f'Doodlestein job {identifier} has unknown dependency {dependency}')
                continue
            job_indegree[identifier] += 1
            job_children[dependency].append(identifier)
    job_queue = deque(sorted(identifier for identifier, degree in job_indegree.items() if degree == 0))
    job_visited: list[str] = []
    while job_queue:
        identifier = job_queue.popleft()
        job_visited.append(identifier)
        for child in sorted(job_children[identifier]):
            job_indegree[child] -= 1
            if job_indegree[child] == 0:
                job_queue.append(child)
    if len(job_visited) != len(job_ids):
        fail('Doodlestein job graph contains a cycle')

workflow_dir = ROOT / '.github' / 'workflows'
for workflow in sorted(workflow_dir.glob('*.y*ml')):
    text = workflow.read_text(encoding='utf-8')
    if re.search(r'^\s*uses\s*:', text, flags=re.MULTILINE):
        fail(f'{workflow.relative_to(ROOT)}: external GitHub Action use is forbidden')
    if re.search(r'runs-on\s*:\s*(?:ubuntu|windows|macos)-', text, flags=re.IGNORECASE):
        fail(f'{workflow.relative_to(ROOT)}: hosted runner labels are forbidden')

toolchain = load_toml(ROOT / 'rust-toolchain.toml')
channel = toolchain.get('toolchain', {}).get('channel') if isinstance(toolchain.get('toolchain'), dict) else None
if not isinstance(channel, str) or not re.fullmatch(r'nightly-\d{4}-\d{2}-\d{2}', channel):
    fail('rust-toolchain.toml must pin an exact dated nightly')

dependency_policy = (ROOT / 'DEPENDENCY_POLICY.md').read_text(encoding='utf-8') if (ROOT / 'DEPENDENCY_POLICY.md').exists() else ''
for name in sorted(FORBIDDEN_CARGO):
    if name.lower() not in dependency_policy.lower():
        fail(f'DEPENDENCY_POLICY.md: forbidden dependency {name} is not documented')

for path in sorted(ROOT.rglob('*.md')):
    if '.git' in path.parts:
        continue
    text = path.read_text(encoding='utf-8')
    for raw_target in MARKDOWN_LINK.findall(text):
        target = raw_target.strip().split()[0].strip('<>')
        if not target or target.startswith(('#', 'http://', 'https://', 'mailto:')):
            continue
        target_path = unquote(target.split('#', 1)[0])
        if target_path and not (path.parent / target_path).resolve().exists():
            fail(f'{path.relative_to(ROOT)}: broken relative Markdown link {raw_target!r}')

bootstrap_path = ROOT / '.beads/bootstrap.jsonl'
if not bootstrap_path.is_file():
    fail('missing generated .beads/bootstrap.jsonl')
else:
    try:
        bootstrap_rows = [json.loads(line) for line in bootstrap_path.read_text(encoding='utf-8').splitlines() if line]
    except Exception as exc:
        fail(f'.beads/bootstrap.jsonl: invalid JSONL: {exc}')
        bootstrap_rows = []
    if [row.get('external_id') for row in bootstrap_rows] != list(work_items):
        fail('.beads/bootstrap.jsonl: work-package identities or order are stale')
    for row in bootstrap_rows:
        item = work_items.get(row.get('external_id'))
        if item and (row.get('dependencies') != item.get('dependencies') or row.get('acceptance_gate') != item.get('acceptance_gate')):
            fail(f'.beads/bootstrap.jsonl: stale dependency/gate data for {row.get("external_id")}')

core_source = ROOT / 'crates/fdgr-core/src/lib.rs'
if core_source.is_file():
    source = core_source.read_text(encoding='utf-8')
    capability_block = source.split('pub fn capabilities()', 1)[-1].split('/// Doctor verdict', 1)[0]
    source_capabilities = re.findall(r'id: "([a-z0-9.]+)"', capability_block)
    if source_capabilities != sorted(source_capabilities) or len(source_capabilities) != len(set(source_capabilities)):
        fail('fdgr-core capability IDs must be unique and lexicographically ordered')

cargo_root = load_toml(ROOT / 'Cargo.toml')
workspace_package = cargo_root.get('workspace', {}).get('package', {}) if isinstance(cargo_root.get('workspace'), dict) else {}
# tomllib represents [workspace.package] under the workspace table.
if workspace_package.get('license') != 'LicenseRef-MIT-OpenAI-Anthropic-Rider':
    fail('Cargo.toml must declare workspace.package.license as "LicenseRef-MIT-OpenAI-Anthropic-Rider"')
for manifest_path in sorted(ROOT.glob('crates/*/Cargo.toml')):
    crate_manifest = load_toml(manifest_path)
    pkg = crate_manifest.get('package', {})
    if not (isinstance(pkg.get('license'), dict) and pkg['license'].get('workspace') is True):
        fail(f'{manifest_path.relative_to(ROOT)}: must inherit license from workspace (license.workspace = true)')
lock = load_toml(ROOT / 'Cargo.lock')
locked_names = {package.get('name') for package in lock.get('package', []) if isinstance(package, dict)}
expected_names = {path.parent.name for path in ROOT.glob('crates/*/Cargo.toml')}
if locked_names != expected_names:
    fail(f'Cargo.lock package set is stale: expected {sorted(expected_names)}, found {sorted(locked_names)}')

for script in sorted((ROOT / 'scripts').glob('*')):
    if script.suffix in {'.py', '.sh'} and not (script.stat().st_mode & 0o111):
        fail(f'{script.relative_to(ROOT)}: executable script lacks an executable bit')

if errors:
    for message in errors:
        print(f'ERROR: {message}', file=sys.stderr)
    print(f'FAILED: {len(errors)} repository policy error(s)', file=sys.stderr)
    raise SystemExit(1)

notes.extend([
    f'{len(registries)} TOML registries parsed',
    f'{len(schema_ids)} JSON Schemas parsed',
    f'{len(all_ids)} stable registry IDs unique and plan-traceable',
    f'{len(work_items)} work packages form an acyclic graph',
    f'{len(list(ROOT.glob("crates/*/Cargo.toml")))} executable scaffold crates checked',
])
for note in notes:
    print(f'PASS: {note}')
print('PASS: FDGR repository policy validation complete')
