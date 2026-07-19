#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
GENERATOR="$ROOT_DIR/tools/ci/compatibility_evidence.py"
TEST_DIR=$(mktemp -d /tmp/alfred_compatibility_evidence.XXXXXX)
SOURCE_REVISION=0123456789abcdef0123456789abcdef01234567

cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

generate() {
    local output=$1
    local install_outcome=$2
    local smoke_outcome=$3
    local compiler=${4:-cc}

    CC="$compiler" python3 "$GENERATOR" \
        --output "$output" \
        --lane ubuntu-24.04 \
        --container-image ubuntu:24.04 \
        --container-mode github-actions-job-container \
        --kernel-scope shared-github-runner-kernel \
        --build-profile release \
        --sanitizers-enabled false \
        --source-revision "$SOURCE_REVISION" \
        --ci-run-id 123456 \
        --ci-run-attempt 2 \
        --staged-install-outcome "$install_outcome" \
        --mvp-smoke-outcome "$smoke_outcome"
}

EVIDENCE="$TEST_DIR/nested/evidence.json"
generate "$EVIDENCE" success skipped

python3 - "$EVIDENCE" "$SOURCE_REVISION" <<'PY'
import datetime
import json
import sys

path, source_revision = sys.argv[1:]
with open(path, encoding="utf-8") as stream:
    evidence = json.load(stream)

def require(condition, message):
    if not condition:
        raise SystemExit(message)


require(set(evidence) == {
    "schema", "schema_version", "generated_at_utc", "source_revision",
    "ci", "lane", "environment", "build", "results",
}, "unexpected top-level evidence fields")
require(evidence["schema"] == "alfred.compatibility-evidence", "unexpected schema")
require(evidence["schema_version"] == 0, "unexpected schema version")
require(
    evidence["source_revision"] == source_revision,
    "unexpected source revision",
)
require(evidence["ci"] == {
    "provider": "github-actions", "run_id": "123456", "run_attempt": "2"
}, "unexpected CI provenance")
require(evidence["lane"] == "ubuntu-24.04", "unexpected lane")
require(set(evidence["environment"]) == {
    "container_image", "container_mode", "distribution_id",
    "distribution_version", "architecture", "libc", "compiler",
    "source_tree_filesystem_type", "kernel_release", "kernel_scope",
}, "unexpected environment fields")
require(set(evidence["environment"]["libc"]) == {"name", "version"},
        "unexpected libc fields")
require(set(evidence["environment"]["compiler"]) == {"id", "version"},
        "unexpected compiler fields")
require(evidence["environment"]["container_image"] == "ubuntu:24.04",
        "unexpected container image")
require(evidence["environment"]["kernel_scope"] == "shared-github-runner-kernel",
        "unexpected kernel scope")
require(evidence["environment"]["compiler"]["id"] in {"gcc", "clang"},
        "unexpected compiler identity")
require(evidence["build"] == {"profile": "release", "sanitizers_enabled": False},
        "unexpected build metadata")
require(evidence["results"] == {"staged_install": "passed", "mvp_smoke": "not_run"},
        "unexpected normalized results")
try:
    datetime.datetime.fromisoformat(
        evidence["generated_at_utc"].replace("Z", "+00:00")
    )
except (KeyError, TypeError, ValueError) as error:
    raise SystemExit("invalid generation timestamp") from error

serialized = json.dumps(evidence)
for forbidden in ("hostname", "username", "workspace", "environment_variables"):
    require(forbidden not in serialized, f"forbidden field present: {forbidden}")
PY

python3 - "$GENERATOR" "$TEST_DIR" <<'PY'
import importlib.util
import os
import sys

generator_path, unrelated_cwd = sys.argv[1:]
spec = importlib.util.spec_from_file_location("compatibility_evidence", generator_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

commands = []

def record_command(command):
    commands.append(command)
    return "testfs"

module.command_output = record_command
previous_cwd = os.getcwd()
try:
    os.chdir(unrelated_cwd)
    filesystem_type = module.source_tree_filesystem_type()
finally:
    os.chdir(previous_cwd)

expected = [["stat", "-f", "-c", "%T", module.SOURCE_TREE_ROOT]]
if filesystem_type != "testfs":
    raise SystemExit("source-tree filesystem probe did not return its bounded value")
if commands != expected:
    raise SystemExit(f"source-tree filesystem probe used unexpected command: {commands!r}")
if not os.path.isabs(module.SOURCE_TREE_ROOT):
    raise SystemExit("source-tree filesystem probe target is not absolute")
PY

OVERSIZED_GROUP_PROBE="$TEST_DIR/oversized-group-probe"
DESCENDANT_PID_FILE="$TEST_DIR/oversized-group-descendant.pid"
cat >"$OVERSIZED_GROUP_PROBE" <<'PY'
#!/usr/bin/env python3
import os
import time

descendant_pid = os.fork()
if descendant_pid == 0:
    time.sleep(30)
    os._exit(0)

with open(os.environ["ALFRED_TEST_DESCENDANT_PID_FILE"], "w", encoding="ascii") as stream:
    stream.write(f"{descendant_pid}\n")
os.write(1, b"x" * 8192)
os._exit(0)
PY
chmod +x "$OVERSIZED_GROUP_PROBE"
ALFRED_TEST_DESCENDANT_PID_FILE="$DESCENDANT_PID_FILE" \
    python3 - "$GENERATOR" "$OVERSIZED_GROUP_PROBE" "$DESCENDANT_PID_FILE" <<'PY'
import importlib.util
import os
import sys
import time

generator_path, oversized_group_probe, descendant_pid_file = sys.argv[1:]
spec = importlib.util.spec_from_file_location("compatibility_evidence", generator_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

if module.command_output([oversized_group_probe]) != module.UNKNOWN:
    raise SystemExit("oversized process-group probe did not degrade to unknown")

with open(descendant_pid_file, encoding="ascii") as stream:
    descendant_pid = int(stream.read())

for _ in range(50):
    try:
        with open(f"/proc/{descendant_pid}/stat", encoding="ascii") as stream:
            process_state = stream.read().split()[2]
    except FileNotFoundError:
        break
    if process_state == "Z":
        break
    time.sleep(0.02)
else:
    raise SystemExit("oversized process-group probe left a running descendant")
PY

NOISY_COMPILER="$TEST_DIR/noisy-cc"
cat >"$NOISY_COMPILER" <<'PY'
#!/usr/bin/env python3
import os

os.write(1, b"x" * 8192)
PY
chmod +x "$NOISY_COMPILER"
NOISY_EVIDENCE="$TEST_DIR/noisy-probe.json"
generate "$NOISY_EVIDENCE" success success "$NOISY_COMPILER"
python3 - "$NOISY_EVIDENCE" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    evidence = json.load(stream)
if evidence["environment"]["compiler"] != {"id": "unknown", "version": "unknown"}:
    raise SystemExit("noisy compiler probe did not degrade to unknown")
PY

UNKNOWN_EVIDENCE="$TEST_DIR/unknown.json"
PATH=/nonexistent /usr/bin/python3 "$GENERATOR" \
    --output "$UNKNOWN_EVIDENCE" \
    --lane debian-13 \
    --container-image debian:13-slim \
    --container-mode github-actions-job-container \
    --kernel-scope shared-github-runner-kernel \
    --build-profile release \
    --sanitizers-enabled false \
    --source-revision "$SOURCE_REVISION" \
    --ci-run-id 1 \
    --ci-run-attempt 1 \
    --staged-install-outcome failure \
    --mvp-smoke-outcome cancelled

python3 - "$UNKNOWN_EVIDENCE" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    evidence = json.load(stream)
if evidence["environment"]["compiler"] != {"id": "unknown", "version": "unknown"}:
    raise SystemExit("missing unknown compiler fallback")
if evidence["environment"]["source_tree_filesystem_type"] != "unknown":
    raise SystemExit("missing unknown filesystem fallback")
if evidence["results"] != {"staged_install": "failed", "mvp_smoke": "cancelled"}:
    raise SystemExit("unexpected failure/cancelled status mapping")
PY

UNKNOWN_STATUS_EVIDENCE="$TEST_DIR/unknown-status.json"
generate "$UNKNOWN_STATUS_EVIDENCE" unknown unknown
python3 - "$UNKNOWN_STATUS_EVIDENCE" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    evidence = json.load(stream)
if evidence["results"] != {"staged_install": "unknown", "mvp_smoke": "unknown"}:
    raise SystemExit("unexpected unknown status mapping")
PY

INVALID_EVIDENCE="$TEST_DIR/invalid.json"
printf 'must-not-be-overwritten\n' >"$INVALID_EVIDENCE"
if python3 "$GENERATOR" \
    --output "$INVALID_EVIDENCE" \
    --lane 'invalid lane' \
    --container-image ubuntu:24.04 \
    --container-mode github-actions-job-container \
    --kernel-scope shared-github-runner-kernel \
    --build-profile release \
    --sanitizers-enabled false \
    --source-revision "$SOURCE_REVISION" \
    --ci-run-id 1 \
    --ci-run-attempt 1 \
    --staged-install-outcome success \
    --mvp-smoke-outcome success >/dev/null 2>&1; then
    printf 'FAIL: invalid lane was accepted\n' >&2
    exit 1
fi
grep -qx 'must-not-be-overwritten' "$INVALID_EVIDENCE"

if generate "$TEST_DIR/invalid-status.json" success unexpected >/dev/null 2>&1; then
    printf 'FAIL: invalid step outcome was accepted\n' >&2
    exit 1
fi
test ! -e "$TEST_DIR/invalid-status.json"

if python3 "$GENERATOR" \
    --output "$TEST_DIR/invalid-revision.json" \
    --lane ubuntu-24.04 \
    --container-image ubuntu:24.04 \
    --container-mode github-actions-job-container \
    --kernel-scope shared-github-runner-kernel \
    --build-profile release \
    --sanitizers-enabled false \
    --source-revision not-a-revision \
    --ci-run-id 1 \
    --ci-run-attempt 1 \
    --staged-install-outcome success \
    --mvp-smoke-outcome success >/dev/null 2>&1; then
    printf 'FAIL: invalid source revision was accepted\n' >&2
    exit 1
fi
test ! -e "$TEST_DIR/invalid-revision.json"

printf 'PASS: compatibility evidence v0 contract\n'
