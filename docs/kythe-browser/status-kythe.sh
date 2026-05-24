#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="$SCRIPT_DIR/.work"
CONTAINER_FILE="$WORK_DIR/http_server.container"
PORT="${KYTHE_PORT:-9898}"
CONTAINER_NAME="${KYTHE_CONTAINER_NAME:-alfred-kythe-http-$PORT}"

docker ps -a \
    --filter "name=^/${CONTAINER_NAME}$" \
    --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}'

if [ -f "$CONTAINER_FILE" ]; then
    printf '\nContainer registrato:\n'
    cat "$CONTAINER_FILE"
    printf '\n'
fi

printf '\nHTTP locale:\n'
curl -L -s -o /tmp/alfred-kythe.html -w '%{http_code} %{size_download}\n' "http://127.0.0.1:$PORT" || true
