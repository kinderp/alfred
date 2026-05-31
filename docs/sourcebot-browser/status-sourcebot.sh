#!/usr/bin/env bash

set -euo pipefail

CONTAINER_NAME="${SOURCEBOT_CONTAINER_NAME:-alfred-sourcebot}"
PORT="${SOURCEBOT_PORT:-3000}"

docker ps -a \
    --filter "name=^/${CONTAINER_NAME}$" \
    --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}'

printf '\nHTTP locale:\n'
curl -L -s -o /tmp/alfred-sourcebot.html -w '%{http_code} %{size_download}\n' "http://127.0.0.1:$PORT" || true
