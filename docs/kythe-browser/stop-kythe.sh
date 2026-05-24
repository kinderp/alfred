#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER_FILE="$SCRIPT_DIR/.work/http_server.container"

if [ ! -f "$CONTAINER_FILE" ]; then
    echo "Kythe non risulta avviato."
    exit 0
fi

CONTAINER_ID="$(cat "$CONTAINER_FILE")"

if docker ps -q --no-trunc | grep -q "$CONTAINER_ID"; then
    docker stop "$CONTAINER_ID" >/dev/null
    echo "Kythe fermato: $CONTAINER_ID"
else
    echo "Container Kythe non attivo: $CONTAINER_ID"
fi

docker rm "$CONTAINER_ID" >/dev/null 2>&1 || true
rm -f "$CONTAINER_FILE"
