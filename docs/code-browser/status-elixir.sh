#!/usr/bin/env bash

set -euo pipefail

CONTAINER_NAME="${ALFRED_ELIXIR_CONTAINER:-alfred-elixir}"
PORT="${ALFRED_ELIXIR_PORT:-8080}"

docker ps -a \
    --filter "name=^/${CONTAINER_NAME}$" \
    --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}'

printf '\nHTTP locale:\n'
curl -L -s -o /tmp/alfred-elixir.html -w '%{http_code} %{size_download}\n' \
    "http://127.0.0.1:$PORT/alfred/workspace/source" || true
