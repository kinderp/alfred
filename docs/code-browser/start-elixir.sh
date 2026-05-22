#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ELIXIR_DATA_DIR="$SCRIPT_DIR/.work/elixir-data"
IMAGE_NAME="${ALFRED_ELIXIR_IMAGE:-alfred-elixir:local}"
CONTAINER_NAME="${ALFRED_ELIXIR_CONTAINER:-alfred-elixir}"
PORT="${ALFRED_ELIXIR_PORT:-8080}"

if [ ! -d "$ELIXIR_DATA_DIR/alfred/data" ]; then
    echo "Database Elixir non trovato. Esegui prima setup-elixir.sh." >&2
    exit 1
fi

if docker ps --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
    echo "Container gia' in esecuzione: $CONTAINER_NAME"
    echo "URL: http://127.0.0.1:$PORT/alfred/workspace/source"
    exit 0
fi

if docker ps -a --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
    docker rm "$CONTAINER_NAME" >/dev/null
fi

docker run -d \
    --name "$CONTAINER_NAME" \
    -p "$PORT:80" \
    -v "$ELIXIR_DATA_DIR:/srv/elixir-data" \
    "$IMAGE_NAME" >/dev/null

echo "Elixir avviato."
echo "URL: http://127.0.0.1:$PORT/alfred/workspace/source"
