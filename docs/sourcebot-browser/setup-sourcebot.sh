#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE="${SOURCEBOT_IMAGE:-ghcr.io/sourcebot-dev/sourcebot:v4.17.3}"

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker non trovato nel PATH." >&2
    echo "Installa Docker Engine o Docker Desktop e riprova." >&2
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "Docker non risponde oppure l'utente corrente non ha i permessi." >&2
    echo "Verifica che il daemon sia avviato e che l'utente possa usare docker." >&2
    exit 1
fi

if ! git -C "$PROJECT_ROOT" config --get remote.origin.url >/dev/null; then
    echo "Sourcebot richiede remote.origin.url per indicizzare repository locali." >&2
    echo "Imposta un origin Git prima di avviare Sourcebot." >&2
    exit 1
fi

docker pull "$IMAGE"

cat <<MSG
Sourcebot pronto.

Immagine:
  $IMAGE

Avvio:
  docs/sourcebot-browser/start-sourcebot.sh
MSG
