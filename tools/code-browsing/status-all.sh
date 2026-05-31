#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT"

echo "== Sourcebot =="
docs/sourcebot-browser/status-sourcebot.sh

echo
echo "== Elixir =="
docs/code-browser/status-elixir.sh

echo
echo "== Kythe =="
docs/kythe-browser/status-kythe.sh
