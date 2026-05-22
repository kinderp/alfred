#!/usr/bin/env bash

set -euo pipefail

CONTAINER_NAME="${ALFRED_ELIXIR_CONTAINER:-alfred-elixir}"

if docker ps --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
    docker stop "$CONTAINER_NAME" >/dev/null
    echo "Container fermato: $CONTAINER_NAME"
else
    echo "Container non in esecuzione: $CONTAINER_NAME"
fi
