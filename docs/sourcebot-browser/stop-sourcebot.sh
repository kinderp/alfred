#!/usr/bin/env bash

set -euo pipefail

CONTAINER_NAME="${SOURCEBOT_CONTAINER_NAME:-alfred-sourcebot}"

if docker ps -a --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
    docker rm -f "$CONTAINER_NAME" >/dev/null
    echo "Sourcebot fermato: $CONTAINER_NAME"
else
    echo "Sourcebot non risulta avviato."
fi
