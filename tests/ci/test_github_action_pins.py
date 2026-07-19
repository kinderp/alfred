#!/usr/bin/env python3
"""Reject mutable or undocumented external GitHub Actions references."""

from __future__ import annotations

import re
import sys
from pathlib import Path


FULL_SHA = re.compile(r"[0-9a-f]{40}")
RELEASE_COMMENT = re.compile(r"\bv[0-9]+\.[0-9]+\.[0-9]+\b")


def main() -> int:
    repository = Path(__file__).resolve().parents[2]
    workflow_dir = repository / ".github" / "workflows"
    workflows = sorted(workflow_dir.glob("*.yml"))
    workflows.extend(sorted(workflow_dir.glob("*.yaml")))

    failures: list[str] = []
    external_count = 0

    for workflow in workflows:
        for line_number, line in enumerate(
            workflow.read_text(encoding="utf-8").splitlines(), start=1
        ):
            match = re.match(r"^\s*(?:-\s*)?uses:\s*(.+?)\s*$", line)
            if match is None:
                continue

            value, separator, comment = match.group(1).partition("#")
            action_ref = value.strip().strip("'\"")
            comment = comment.strip() if separator else ""

            if action_ref.startswith(("./", "docker://")):
                continue

            external_count += 1
            location = f"{workflow.relative_to(repository)}:{line_number}"

            if "@" not in action_ref:
                failures.append(f"{location}: external action has no revision")
                continue

            revision = action_ref.rsplit("@", maxsplit=1)[1]
            if FULL_SHA.fullmatch(revision) is None:
                failures.append(
                    f"{location}: external action must use a full lowercase 40-character SHA"
                )

            if RELEASE_COMMENT.search(comment) is None:
                failures.append(
                    f"{location}: immutable pin must include a readable '# vX.Y.Z' comment"
                )

    if external_count == 0:
        failures.append("no external GitHub Actions references were found")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print(
        "PASS: "
        f"{external_count} external GitHub Actions references use documented full SHA pins"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
