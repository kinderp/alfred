#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK_DIR="$SCRIPT_DIR/.work"
ENV_FILE="$WORK_DIR/env.sh"
PORT="${KYTHE_PORT:-9898}"
IMAGE_NAME="${KYTHE_DOCKER_IMAGE:-alfred-kythe:local}"
CONTAINER_NAME="${KYTHE_CONTAINER_NAME:-alfred-kythe-http-$PORT}"

if [ ! -f "$ENV_FILE" ]; then
    echo "Kythe non installato. Esegui prima setup-kythe.sh." >&2
    exit 1
fi

# shellcheck disable=SC1090
. "$ENV_FILE"

SERVING="$WORK_DIR/serving"
CONTAINER_FILE="$WORK_DIR/http_server.container"

if [ ! -d "$SERVING" ]; then
    echo "Serving table non trovate. Esegui prima reindex-kythe.sh." >&2
    exit 1
fi

if [ -f "$CONTAINER_FILE" ] && docker ps -q --no-trunc | grep -q "$(cat "$CONTAINER_FILE")"; then
    echo "Kythe gia' in esecuzione su http://127.0.0.1:$PORT"
    exit 0
fi

docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true

CONTAINER_ID="$(
    docker run -d \
        --name "$CONTAINER_NAME" \
        -v "$PROJECT_ROOT:$PROJECT_ROOT:ro" \
        -w "$PROJECT_ROOT" \
        -p "127.0.0.1:$PORT:$PORT" \
        "$IMAGE_NAME" \
        bash -lc "rm -rf /tmp/alfred-kythe-serving && cp -a '$SERVING' /tmp/alfred-kythe-serving && '$KYTHE_DIR/tools/http_server' --serving_table /tmp/alfred-kythe-serving --listen 0.0.0.0:$PORT"
)"

echo "$CONTAINER_ID" > "$CONTAINER_FILE"

echo "Kythe avviato."
echo "URL: http://127.0.0.1:$PORT"
echo "Container: $CONTAINER_NAME"
echo "Log: docker logs $CONTAINER_NAME"
