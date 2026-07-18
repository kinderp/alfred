#!/usr/bin/env python3
"""Generate one bounded, versioned Alfred userspace compatibility record."""

import argparse
import datetime
import json
import os
import platform
import re
import shutil
import subprocess
import tempfile


SCHEMA_NAME = "alfred.compatibility-evidence"
SCHEMA_VERSION = 0
UNKNOWN = "unknown"
MAX_VALUE_LENGTH = 256
IDENTIFIER_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")
REVISION_RE = re.compile(r"^[0-9a-f]{40,64}$")
RUN_ID_RE = re.compile(r"^[0-9]{1,32}$")
STATUS_MAP = {
    "success": "passed",
    "failure": "failed",
    "skipped": "not_run",
    "cancelled": "cancelled",
    "unknown": "unknown",
}


def bounded_value(value, field_name, allow_empty=False):
    """Reject control characters and unbounded command-line metadata."""
    if not allow_empty and not value:
        raise ValueError(f"{field_name} must not be empty")
    if len(value) > MAX_VALUE_LENGTH:
        raise ValueError(f"{field_name} exceeds {MAX_VALUE_LENGTH} characters")
    if any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise ValueError(f"{field_name} contains control characters")
    return value or UNKNOWN


def identifier(value, field_name):
    """Validate a stable lowercase identifier used by the evidence schema."""
    if not IDENTIFIER_RE.fullmatch(value):
        raise ValueError(f"{field_name} is not a valid identifier")
    return value


def command_output(command):
    """Return one bounded output line, or unknown when a probe is unavailable."""
    try:
        result = subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.SubprocessError, UnicodeError):
        return UNKNOWN

    if result.returncode != 0:
        return UNKNOWN
    first_line = result.stdout.splitlines()[0].strip() if result.stdout else ""
    if not first_line:
        return UNKNOWN
    try:
        return bounded_value(first_line, "probe output")
    except ValueError:
        return UNKNOWN


def distribution_info():
    """Read the freedesktop OS identity without copying arbitrary os-release data."""
    try:
        release = platform.freedesktop_os_release()
    except (OSError, ValueError):
        release = {}
    distribution_id = release.get("ID", UNKNOWN).lower()
    distribution_version = release.get("VERSION_ID", UNKNOWN)
    if not IDENTIFIER_RE.fullmatch(distribution_id):
        distribution_id = UNKNOWN
    try:
        distribution_version = bounded_value(
            distribution_version, "distribution_version"
        )
    except ValueError:
        distribution_version = UNKNOWN
    return distribution_id, distribution_version


def libc_info():
    """Detect libc conservatively and preserve unknown instead of guessing."""
    name, version = platform.libc_ver()
    if name and version:
        try:
            return (
                bounded_value(name.lower(), "libc name"),
                bounded_value(version, "libc version"),
            )
        except ValueError:
            return UNKNOWN, UNKNOWN

    glibc = command_output(["getconf", "GNU_LIBC_VERSION"])
    if glibc != UNKNOWN:
        parts = glibc.split(maxsplit=1)
        if len(parts) == 2:
            return parts[0].lower()[:MAX_VALUE_LENGTH], parts[1][:MAX_VALUE_LENGTH]
    return UNKNOWN, UNKNOWN


def compiler_info():
    """Probe the configured C compiler and return a small stable identity."""
    compiler = os.environ.get("CC", "cc")
    if len(compiler) > MAX_VALUE_LENGTH or any(
        character.isspace() for character in compiler
    ):
        return UNKNOWN, UNKNOWN

    description = command_output([compiler, "--version"])
    version = command_output([compiler, "-dumpfullversion", "-dumpversion"])
    if description == UNKNOWN:
        return UNKNOWN, UNKNOWN

    resolved_compiler = shutil.which(compiler)
    resolved_name = (
        os.path.basename(os.path.realpath(resolved_compiler)).lower()
        if resolved_compiler
        else ""
    )
    lowered = description.lower()
    if "clang" in resolved_name or "clang" in lowered:
        compiler_id = "clang"
    elif (
        "gcc" in resolved_name
        or "gcc" in lowered
        or "free software foundation" in lowered
    ):
        compiler_id = "gcc"
    else:
        compiler_id = UNKNOWN
    return compiler_id, version


def normalize_status(raw_status, field_name):
    """Map GitHub step outcomes to the public evidence status vocabulary."""
    try:
        return STATUS_MAP[raw_status]
    except KeyError as error:
        allowed = ", ".join(sorted(STATUS_MAP))
        raise ValueError(f"{field_name} must be one of: {allowed}") from error


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True)
    parser.add_argument("--lane", required=True)
    parser.add_argument("--container-image", required=True)
    parser.add_argument(
        "--container-mode",
        choices=("github-actions-job-container", "unknown"),
        required=True,
    )
    parser.add_argument(
        "--kernel-scope",
        choices=(
            "shared-github-runner-kernel",
            "guest-kernel",
            "host-kernel",
            "unknown",
        ),
        required=True,
    )
    parser.add_argument(
        "--build-profile", choices=("release", "debug", "unknown"), required=True
    )
    parser.add_argument(
        "--sanitizers-enabled", choices=("true", "false"), required=True
    )
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--ci-run-id", required=True)
    parser.add_argument("--ci-run-attempt", required=True)
    parser.add_argument("--staged-install-outcome", required=True)
    parser.add_argument("--mvp-smoke-outcome", required=True)
    return parser.parse_args()


def build_evidence(arguments):
    """Validate supplied metadata, run bounded probes, and build schema v0."""
    lane = identifier(arguments.lane, "lane")
    container_image = bounded_value(arguments.container_image, "container_image")
    if not REVISION_RE.fullmatch(arguments.source_revision):
        raise ValueError("source_revision must be 40 to 64 lowercase hexadecimal characters")
    if not RUN_ID_RE.fullmatch(arguments.ci_run_id):
        raise ValueError("ci_run_id must contain 1 to 32 decimal digits")
    if not RUN_ID_RE.fullmatch(arguments.ci_run_attempt):
        raise ValueError("ci_run_attempt must contain 1 to 32 decimal digits")

    distribution_id, distribution_version = distribution_info()
    libc_name, libc_version = libc_info()
    compiler_id, compiler_version = compiler_info()
    try:
        architecture = bounded_value(platform.machine(), "architecture")
    except ValueError:
        architecture = UNKNOWN
    try:
        kernel_release = bounded_value(platform.release(), "kernel_release")
    except ValueError:
        kernel_release = UNKNOWN
    filesystem_type = command_output(["stat", "-f", "-c", "%T", "."])
    if filesystem_type.upper().startswith("UNKNOWN"):
        filesystem_type = UNKNOWN

    return {
        "schema": SCHEMA_NAME,
        "schema_version": SCHEMA_VERSION,
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc)
        .isoformat(timespec="seconds")
        .replace("+00:00", "Z"),
        "source_revision": arguments.source_revision,
        "ci": {
            "provider": "github-actions",
            "run_id": arguments.ci_run_id,
            "run_attempt": arguments.ci_run_attempt,
        },
        "lane": lane,
        "environment": {
            "container_image": container_image,
            "container_mode": arguments.container_mode,
            "distribution_id": distribution_id,
            "distribution_version": distribution_version,
            "architecture": architecture,
            "libc": {"name": libc_name, "version": libc_version},
            "compiler": {"id": compiler_id, "version": compiler_version},
            "filesystem_type": filesystem_type,
            "kernel_release": kernel_release,
            "kernel_scope": arguments.kernel_scope,
        },
        "build": {
            "profile": arguments.build_profile,
            "sanitizers_enabled": arguments.sanitizers_enabled == "true",
        },
        "results": {
            "staged_install": normalize_status(
                arguments.staged_install_outcome, "staged_install_outcome"
            ),
            "mvp_smoke": normalize_status(
                arguments.mvp_smoke_outcome, "mvp_smoke_outcome"
            ),
        },
    }


def write_atomic(output_path, evidence):
    """Write valid JSON beside the target and atomically replace the target."""
    absolute_output = os.path.abspath(output_path)
    output_directory = os.path.dirname(absolute_output)
    os.makedirs(output_directory, exist_ok=True)
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=output_directory, delete=False
        ) as temporary:
            temporary_path = temporary.name
            json.dump(evidence, temporary, indent=2, sort_keys=True)
            temporary.write("\n")
            temporary.flush()
            os.fsync(temporary.fileno())
        os.replace(temporary_path, absolute_output)
    finally:
        if temporary_path is not None and os.path.exists(temporary_path):
            os.unlink(temporary_path)


def main():
    try:
        arguments = parse_arguments()
        evidence = build_evidence(arguments)
        write_atomic(arguments.output, evidence)
    except ValueError as error:
        raise SystemExit(f"compatibility evidence error: {error}") from error


if __name__ == "__main__":
    main()
