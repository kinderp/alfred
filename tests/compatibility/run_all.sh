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

assert set(evidence) == {
    "schema", "schema_version", "generated_at_utc", "source_revision",
    "ci", "lane", "environment", "build", "results",
}
assert evidence["schema"] == "alfred.compatibility-evidence"
assert evidence["schema_version"] == 0
assert evidence["source_revision"] == source_revision
assert evidence["ci"] == {
    "provider": "github-actions", "run_id": "123456", "run_attempt": "2"
}
assert evidence["lane"] == "ubuntu-24.04"
assert set(evidence["environment"]) == {
    "container_image", "container_mode", "distribution_id",
    "distribution_version", "architecture", "libc", "compiler",
    "filesystem_type", "kernel_release", "kernel_scope",
}
assert set(evidence["environment"]["libc"]) == {"name", "version"}
assert set(evidence["environment"]["compiler"]) == {"id", "version"}
assert evidence["environment"]["container_image"] == "ubuntu:24.04"
assert evidence["environment"]["kernel_scope"] == "shared-github-runner-kernel"
assert evidence["environment"]["compiler"]["id"] in {"gcc", "clang"}
assert evidence["build"] == {"profile": "release", "sanitizers_enabled": False}
assert evidence["results"] == {"staged_install": "passed", "mvp_smoke": "not_run"}
datetime.datetime.fromisoformat(evidence["generated_at_utc"].replace("Z", "+00:00"))

serialized = json.dumps(evidence)
for forbidden in ("hostname", "username", "workspace", "environment_variables"):
    assert forbidden not in serialized
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
assert evidence["environment"]["compiler"] == {"id": "unknown", "version": "unknown"}
assert evidence["environment"]["filesystem_type"] == "unknown"
assert evidence["results"] == {"staged_install": "failed", "mvp_smoke": "cancelled"}
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
