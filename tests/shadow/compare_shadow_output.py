#!/usr/bin/env python3
"""
Compare legacy inotify semantic output with core shadow-mode output.

The tool runs alfred against a temporary directory, executes one small
filesystem scenario, reads events.log, normalizes both output streams, and
prints the differences.

This is a diagnostic tool for the integration phase. By default it exits with
status 0 even when differences are found. Use --strict when a non-empty diff
should fail the command.
"""

from __future__ import annotations

import argparse
import collections
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BINARY = REPO_ROOT / "alfred"

EVENT_MESSAGE_RE = re.compile(r"^\[[^\]]+\] \[EVENT\] (?P<message>.*)$")

LEGACY_PATH_RE = re.compile(r"^(?P<type>[A-Z_]+) path=(?P<path>.*)$")
LEGACY_MOVE_RE = re.compile(
    r"^(?P<type>[A-Z_]+) from=(?P<src>.*?) to=(?P<dst>.*)$"
)

CORE_PATH_RE = re.compile(
    r"^core seq=\d+ type=(?P<type>[A-Z_]+) path=(?P<path>.*?) pid=-?\d+$"
)
CORE_MOVE_RE = re.compile(
    r"^core seq=\d+ type=(?P<type>[A-Z_]+) "
    r"from=(?P<src>.*?) to=(?P<dst>.*?) pid=-?\d+$"
)


NormalizedEvent = tuple[str, str, str, str]


def normalize_path(path: str, watched_root: Path) -> str:
    """Replace the temporary watched root with a stable marker."""
    root = str(watched_root)

    if path == root:
        return "$ROOT"

    if path.startswith(root + os.sep):
        return "$ROOT" + path[len(root) :]

    return path


def parse_event_log(log_path: Path, watched_root: Path) -> tuple[list[NormalizedEvent], list[NormalizedEvent]]:
    """Parse events.log and return legacy and core event lists."""
    legacy: list[NormalizedEvent] = []
    core: list[NormalizedEvent] = []

    if not log_path.exists():
        raise FileNotFoundError(f"missing log file: {log_path}")

    for raw_line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = EVENT_MESSAGE_RE.match(raw_line)
        if match is None:
            continue

        message = match.group("message")

        if message.startswith("core "):
            move = CORE_MOVE_RE.match(message)
            if move is not None:
                core.append(
                    (
                        move.group("type"),
                        normalize_path(move.group("src"), watched_root),
                        normalize_path(move.group("dst"), watched_root),
                        "move",
                    )
                )
                continue

            path = CORE_PATH_RE.match(message)
            if path is not None:
                core.append(
                    (
                        path.group("type"),
                        normalize_path(path.group("path"), watched_root),
                        "",
                        "path",
                    )
                )
                continue

            continue

        move = LEGACY_MOVE_RE.match(message)
        if move is not None:
            legacy.append(
                (
                    move.group("type"),
                    normalize_path(move.group("src"), watched_root),
                    normalize_path(move.group("dst"), watched_root),
                    "move",
                )
            )
            continue

        path = LEGACY_PATH_RE.match(message)
        if path is not None:
            legacy.append(
                (
                    path.group("type"),
                    normalize_path(path.group("path"), watched_root),
                    "",
                    "path",
                )
            )

    return legacy, core


def event_to_text(event: NormalizedEvent) -> str:
    """Render a normalized event for human-readable output."""
    event_type, src, dst, shape = event

    if shape == "move":
        return f"{event_type} from={src} to={dst}"

    return f"{event_type} path={src}"


def scenario_create_file(root: Path) -> None:
    (root / "a.txt").write_text("hello\n", encoding="utf-8")


def scenario_delete_file(root: Path) -> None:
    path = root / "delete-me.txt"
    path.write_text("temporary\n", encoding="utf-8")
    time.sleep(0.1)
    path.unlink()


def scenario_rename_file(root: Path) -> None:
    src = root / "old.txt"
    dst = root / "new.txt"

    src.write_text("rename\n", encoding="utf-8")
    time.sleep(0.1)
    src.rename(dst)


def scenario_create_dir(root: Path) -> None:
    (root / "new-dir").mkdir()


def scenario_delete_dir(root: Path) -> None:
    path = root / "delete-dir"

    path.mkdir()
    time.sleep(0.1)
    path.rmdir()


def scenario_rename_dir(root: Path) -> None:
    src = root / "old-dir"
    dst = root / "new-dir"

    src.mkdir()
    time.sleep(0.1)
    src.rename(dst)


def scenario_recursive_create_nested_dir(root: Path) -> None:
    (root / "one" / "two" / "three").mkdir(parents=True)


def scenario_recursive_create_slow_nested_dir(root: Path) -> None:
    (root / "one").mkdir()
    time.sleep(0.2)
    (root / "one" / "two").mkdir()
    time.sleep(0.2)
    (root / "one" / "two" / "three").mkdir()


def scenario_move_file(root: Path) -> None:
    src_dir = root / "src"
    dst_dir = root / "dst"
    src_dir.mkdir()
    dst_dir.mkdir()

    src = src_dir / "moved.txt"
    dst = dst_dir / "moved.txt"

    src.write_text("move\n", encoding="utf-8")
    time.sleep(0.1)
    src.rename(dst)


def scenario_move_rename_file(root: Path) -> None:
    src_dir = root / "src"
    dst_dir = root / "dst"
    src_dir.mkdir()
    dst_dir.mkdir()

    src = src_dir / "old.txt"
    dst = dst_dir / "new.txt"

    src.write_text("move and rename\n", encoding="utf-8")
    time.sleep(0.1)
    src.rename(dst)


def scenario_move_rename_dir(root: Path) -> None:
    src_dir = root / "src"
    dst_dir = root / "dst"
    src_dir.mkdir()
    dst_dir.mkdir()

    src = src_dir / "before"
    dst = dst_dir / "after"

    src.mkdir()
    time.sleep(0.1)
    src.rename(dst)


def scenario_move_dir(root: Path) -> None:
    src_dir = root / "src"
    dst_dir = root / "dst"
    src_dir.mkdir()
    dst_dir.mkdir()
    time.sleep(0.2)

    src = src_dir / "item"
    dst = dst_dir / "item"

    src.mkdir()
    time.sleep(0.2)
    src.rename(dst)


def scenario_modify_close_write_file(root: Path) -> None:
    path = root / "editable.txt"

    path.write_text("initial\n", encoding="utf-8")
    time.sleep(0.2)

    with path.open("a", encoding="utf-8") as fp:
        fp.write("updated\n")
        fp.flush()
        os.fsync(fp.fileno())
        time.sleep(0.2)


SCENARIOS = {
    "create_dir": scenario_create_dir,
    "create_file": scenario_create_file,
    "delete_dir": scenario_delete_dir,
    "delete_file": scenario_delete_file,
    "modify_close_write_file": scenario_modify_close_write_file,
    "move_dir": scenario_move_dir,
    "move_file": scenario_move_file,
    "move_rename_dir": scenario_move_rename_dir,
    "move_rename_file": scenario_move_rename_file,
    "recursive_create_nested_dir": scenario_recursive_create_nested_dir,
    "recursive_create_slow_nested_dir": scenario_recursive_create_slow_nested_dir,
    "rename_dir": scenario_rename_dir,
    "rename_file": scenario_rename_file,
}


def run_alfred(binary: Path, workdir: Path, watched_root: Path) -> subprocess.Popen:
    """Start alfred with logs written inside workdir."""
    return subprocess.Popen(
        [str(binary), str(watched_root)],
        cwd=str(workdir),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def stop_alfred(proc: subprocess.Popen) -> None:
    """Request a clean shutdown and wait for process termination."""
    if proc.poll() is not None:
        return

    proc.send_signal(signal.SIGINT)

    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


def diff_events(
    legacy: list[NormalizedEvent],
    core: list[NormalizedEvent],
) -> tuple[list[NormalizedEvent], list[NormalizedEvent]]:
    """Return events only in legacy and only in core, preserving multiplicity."""
    legacy_counter = collections.Counter(legacy)
    core_counter = collections.Counter(core)

    only_legacy = list((legacy_counter - core_counter).elements())
    only_core = list((core_counter - legacy_counter).elements())

    return sorted(only_legacy), sorted(only_core)


def print_event_list(title: str, events: list[NormalizedEvent]) -> None:
    print(f"{title}:")

    if not events:
        print("  <none>")
        return

    for event in events:
        print(f"  {event_to_text(event)}")


def run_scenario(args: argparse.Namespace) -> int:
    binary = Path(args.binary).resolve()

    if not binary.exists():
        print(f"error: binary not found: {binary}", file=sys.stderr)
        print("hint: run `make` from the repository root first", file=sys.stderr)
        return 2

    scenario = SCENARIOS[args.scenario]

    with tempfile.TemporaryDirectory(prefix="alfred_shadow_") as tmp:
        workdir = Path(tmp)
        watched_root = workdir / "watched"
        watched_root.mkdir()

        proc = run_alfred(binary, workdir, watched_root)

        try:
            time.sleep(args.startup_delay)
            scenario(watched_root)
            time.sleep(args.settle_delay)
        finally:
            stop_alfred(proc)

        legacy, core = parse_event_log(workdir / "events.log", watched_root)
        only_legacy, only_core = diff_events(legacy, core)

        print(f"Scenario: {args.scenario}")
        print(f"Temporary root: {watched_root}")
        print()
        print_event_list("Legacy", legacy)
        print()
        print_event_list("Core", core)
        print()
        print_event_list("Only in legacy", only_legacy)
        print()
        print_event_list("Only in core", only_core)

        if args.keep_logs:
            keep_dir = REPO_ROOT / "tests" / "shadow" / "last-run"
            if keep_dir.exists():
                shutil.rmtree(keep_dir)
            shutil.copytree(workdir, keep_dir)
            print()
            print(f"Logs copied to: {keep_dir}")

        if args.strict and (only_legacy or only_core):
            return 1

        return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare legacy and core shadow-mode event output.",
        epilog=(
            "Default mode is diagnostic: differences are printed but the "
            "command exits with status 0. Use --strict only when a difference "
            "must fail the command. Use --keep-logs to preserve events.log, "
            "raw.log, and the watched tree under tests/shadow/last-run for "
            "manual inspection of backend diagnostics such as WATCH_ADDED."
        ),
    )
    parser.add_argument(
        "scenario",
        choices=sorted(SCENARIOS),
        help="filesystem scenario to execute",
    )
    parser.add_argument(
        "--binary",
        default=str(DEFAULT_BINARY),
        help=f"alfred binary path (default: {DEFAULT_BINARY})",
    )
    parser.add_argument(
        "--startup-delay",
        type=float,
        default=0.3,
        help="seconds to wait after starting alfred",
    )
    parser.add_argument(
        "--settle-delay",
        type=float,
        default=0.4,
        help="seconds to wait after executing the scenario",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="exit with status 1 when legacy/core output differs",
    )
    parser.add_argument(
        "--keep-logs",
        action="store_true",
        help="copy the temporary run directory to tests/shadow/last-run",
    )

    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    return run_scenario(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
