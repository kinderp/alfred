#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT"

docs/kythe-browser/stop-kythe.sh
docs/code-browser/stop-elixir.sh
docs/sourcebot-browser/stop-sourcebot.sh
