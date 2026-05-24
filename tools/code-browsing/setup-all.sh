#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

"$SCRIPT_DIR/check-docker.sh"

cd "$PROJECT_ROOT"

echo
echo "== Sourcebot =="
docs/sourcebot-browser/setup-sourcebot.sh

echo
echo "== Elixir =="
docs/code-browser/setup-elixir.sh

echo
echo "== Kythe =="
docs/kythe-browser/setup-kythe.sh
docs/kythe-browser/reindex-kythe.sh

cat <<MSG

Setup completo.

Avvio di tutti i servizi:
  tools/code-browsing/start-all.sh
MSG
