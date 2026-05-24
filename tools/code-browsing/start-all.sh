#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

"$SCRIPT_DIR/check-docker.sh"

cd "$PROJECT_ROOT"

docs/sourcebot-browser/start-sourcebot.sh
docs/code-browser/start-elixir.sh
docs/kythe-browser/start-kythe.sh

cat <<MSG

Browser del codice avviati.

Sourcebot:
  http://127.0.0.1:${SOURCEBOT_PORT:-3000}

Elixir:
  http://127.0.0.1:${ALFRED_ELIXIR_PORT:-8080}/alfred/workspace/source

Kythe API:
  http://127.0.0.1:${KYTHE_PORT:-9898}
MSG
