#!/bin/sh
set -eu

usage() {
    echo "Usage: sh tools/codex/install-sol-ultra.sh [--force]" >&2
}

force=0
if [ "$#" -gt 1 ]; then
    usage
    exit 2
fi

if [ "$#" -eq 1 ]; then
    case "$1" in
        --force)
            force=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 2
            ;;
    esac
fi

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
profile_dir="$script_dir/sol-ultra"

codex_home="${CODEX_HOME:-$HOME/.codex}"
agents_dir="$codex_home/agents"
backup_dir="$codex_home/backups"
target_config="$codex_home/config.toml"
source_config="$profile_dir/config.toml"

if [ ! -f "$source_config" ]; then
    echo "Missing source config: $source_config" >&2
    exit 1
fi

if [ -f "$target_config" ] && [ "$force" -ne 1 ]; then
    echo "Existing Codex config found: $target_config" >&2
    echo "Re-run with --force to back it up and install the Sol Ultra profile." >&2
    exit 2
fi

mkdir -p "$codex_home" "$agents_dir" "$backup_dir"

if [ -f "$target_config" ]; then
    stamp=$(date -u +%Y%m%dT%H%M%SZ)
    backup_path="$backup_dir/config.toml.before-sol-ultra-$stamp"
    cp "$target_config" "$backup_path"
    chmod 600 "$backup_path"
    echo "Backed up existing config to: $backup_path"
fi

install -m 600 "$source_config" "$target_config"

for agent_file in "$profile_dir"/agents/*.toml; do
    [ -f "$agent_file" ] || continue
    install -m 644 "$agent_file" "$agents_dir/$(basename "$agent_file")"
done

echo "Installed Codex Sol Ultra profile in: $codex_home"
echo "Restart Codex, then validate with:"
echo "  codex --strict-config --help"
echo "  codex features list"
