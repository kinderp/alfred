#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK_DIR="$SCRIPT_DIR/.work"
ELIXIR_DIR="$WORK_DIR/elixir"
ELIXIR_DATA_DIR="$WORK_DIR/elixir-data"
ALFRED_DATA_DIR="$ELIXIR_DATA_DIR/alfred"
ALFRED_REPO_DIR="$ALFRED_DATA_DIR/repo"
ALFRED_DB_DIR="$ALFRED_DATA_DIR/data"
IMAGE_NAME="${ALFRED_ELIXIR_IMAGE:-alfred-elixir:local}"
ELIXIR_THREADS="${ELIXIR_THREADS:-1}"

mkdir -p "$WORK_DIR" "$ALFRED_DB_DIR"

if [ ! -d "$ELIXIR_DIR/.git" ]; then
    git clone --depth 1 https://github.com/bootlin/elixir.git "$ELIXIR_DIR"
elif [ "${ALFRED_ELIXIR_REFRESH:-0}" = "1" ]; then
    git -C "$ELIXIR_DIR" pull --ff-only
fi

docker build \
    -t "$IMAGE_NAME" \
    --build-arg ELIXIR_VERSION="$(git -C "$ELIXIR_DIR" rev-parse --short HEAD)" \
    -f "$ELIXIR_DIR/docker/Dockerfile" \
    "$ELIXIR_DIR"

if [ ! -d "$ALFRED_REPO_DIR" ]; then
    git -C "$ALFRED_DATA_DIR" init --bare repo
fi

if git -C "$ALFRED_REPO_DIR" remote get-url local >/dev/null 2>&1; then
    git -C "$ALFRED_REPO_DIR" remote set-url local "$PROJECT_ROOT"
else
    git -C "$ALFRED_REPO_DIR" remote add local "$PROJECT_ROOT"
fi

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

cat <<MSG
Elixir e' pronto.

Avvio:
  docs/code-browser/start-elixir.sh

URL:
  http://127.0.0.1:8080/alfred/workspace/source

MSG
