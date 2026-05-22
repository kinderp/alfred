#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ELIXIR_DATA_DIR="$SCRIPT_DIR/.work/elixir-data"
ALFRED_DATA_DIR="$ELIXIR_DATA_DIR/alfred"
ALFRED_REPO_DIR="$ALFRED_DATA_DIR/repo"
ALFRED_DB_DIR="$ALFRED_DATA_DIR/data"
IMAGE_NAME="${ALFRED_ELIXIR_IMAGE:-alfred-elixir:local}"
ELIXIR_THREADS="${ELIXIR_THREADS:-1}"

if [ ! -d "$ALFRED_REPO_DIR" ]; then
    echo "Repository Elixir non inizializzato. Esegui prima setup-elixir.sh." >&2
    exit 1
fi

mkdir -p "$ALFRED_DB_DIR"

git -C "$ALFRED_REPO_DIR" fetch --prune local \
    '+refs/heads/*:refs/heads/*' \
    '+refs/tags/*:refs/tags/*'

HEAD_SHA="$(git -C "$PROJECT_ROOT" rev-parse HEAD)"
git -C "$ALFRED_REPO_DIR" tag -f workspace "$HEAD_SHA"

docker run --rm \
    -v "$ELIXIR_DATA_DIR:/srv/elixir-data" \
    --entrypoint /bin/bash \
    "$IMAGE_NAME" \
    -lc "LXR_REPO_DIR=/srv/elixir-data/alfred/repo LXR_DATA_DIR=/srv/elixir-data/alfred/data /usr/local/elixir/venv/bin/python /usr/local/elixir/update.py $ELIXIR_THREADS"

echo "Indice Elixir aggiornato su tag workspace."
