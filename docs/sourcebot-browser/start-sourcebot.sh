#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK_DIR="$SCRIPT_DIR/.work"
CONFIG_FILE="$SCRIPT_DIR/config.json"
PORT="${SOURCEBOT_PORT:-3000}"
CONTAINER_NAME="${SOURCEBOT_CONTAINER_NAME:-alfred-sourcebot}"
IMAGE="${SOURCEBOT_IMAGE:-ghcr.io/sourcebot-dev/sourcebot:v4.17.3}"
DATA_VOLUME="${SOURCEBOT_DATA_VOLUME:-alfred-sourcebot-data}"

mkdir -p "$WORK_DIR"

if ! git -C "$PROJECT_ROOT" config --get remote.origin.url >/dev/null; then
    echo "Sourcebot richiede remote.origin.url per indicizzare repository locali." >&2
    echo "Imposta un origin Git prima di avviare Sourcebot." >&2
    exit 1
fi

docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true

docker run -d \
    --name "$CONTAINER_NAME" \
    -p "127.0.0.1:$PORT:3000" \
    -v "$DATA_VOLUME:/data" \
    -v "$CONFIG_FILE:/etc/sourcebot/config.json:ro" \
    -v "$PROJECT_ROOT:/repos/alfred:ro" \
    -e CONFIG_PATH=/etc/sourcebot/config.json \
    -e DATA_DIR=/data \
    -e AUTH_URL="http://localhost:$PORT" \
    -e AUTH_SECRET="alfred-sourcebot-local-development-secret" \
    -e FORCE_ENABLE_ANONYMOUS_ACCESS=true \
    -e SOURCEBOT_TELEMETRY_DISABLED=true \
    "$IMAGE" >/dev/null

cat <<MSG
Sourcebot avviato.

URL:
  http://127.0.0.1:$PORT

Container:
  $CONTAINER_NAME

Data volume:
  $DATA_VOLUME

Log:
  docker logs -f $CONTAINER_NAME
MSG
