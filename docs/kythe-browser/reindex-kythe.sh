#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK_DIR="$SCRIPT_DIR/.work"
ENV_FILE="$WORK_DIR/env.sh"
IMAGE_NAME="${KYTHE_DOCKER_IMAGE:-alfred-kythe:local}"

if [ ! -f "$ENV_FILE" ]; then
    echo "Kythe non installato. Esegui prima setup-kythe.sh." >&2
    exit 1
fi

if [ "${ALFRED_KYTHE_INSIDE_CONTAINER:-0}" != "1" ]; then
    docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
    docker run --rm \
        -u "$(id -u):$(id -g)" \
        -v "$PROJECT_ROOT:$PROJECT_ROOT" \
        -w "$PROJECT_ROOT" \
        -e ALFRED_KYTHE_INSIDE_CONTAINER=1 \
        -e KYTHE_REAL_CC="${KYTHE_REAL_CC:-gcc}" \
        -e KYTHE_CORPUS="${KYTHE_CORPUS:-alfred}" \
        "$IMAGE_NAME" \
        "$SCRIPT_DIR/reindex-kythe.sh"
    exit 0
fi

# shellcheck disable=SC1090
. "$ENV_FILE"

KYTHE_REAL_CC="${KYTHE_REAL_CC:-gcc}"
KYTHE_OUTPUT_DIRECTORY="$WORK_DIR/kzip"
GRAPHSTORE="${KYTHE_GRAPHSTORE:-/tmp/alfred-kythe-graphstore}"
SERVING="$WORK_DIR/serving"
TEMP_SERVING="${KYTHE_TEMP_SERVING:-/tmp/alfred-kythe-serving}"
ENTRIES="$WORK_DIR/entries"
WRAPPER="$SCRIPT_DIR/kythe-gcc-wrapper.sh"

rm -rf "$KYTHE_OUTPUT_DIRECTORY" "$GRAPHSTORE" "$SERVING" "$TEMP_SERVING" "$ENTRIES"
mkdir -p "$KYTHE_OUTPUT_DIRECTORY" "$ENTRIES"

export KYTHE_DIR
export KYTHE_OUTPUT_DIRECTORY
export KYTHE_ROOT_DIRECTORY="$PROJECT_ROOT"
export KYTHE_CORPUS="${KYTHE_CORPUS:-alfred}"
export KYTHE_REAL_CC

(
    cd "$PROJECT_ROOT"
    make clean
    make CC="$WRAPPER"
)

mapfile -t KZIP_FILES < <(find "$KYTHE_OUTPUT_DIRECTORY" -type f \( -name '*.kzip' -o -name '*.kindex' \) | sort)

if [ "${#KZIP_FILES[@]}" -eq 0 ]; then
    echo "Nessun file .kzip/.kindex prodotto da Kythe." >&2
    exit 1
fi

for unit in "${KZIP_FILES[@]}"; do
    base="$(basename "$unit")"
    "$KYTHE_DIR/indexers/cxx_indexer" --ignore_unimplemented "$unit" > "$ENTRIES/$base.entries"
done

cat "$ENTRIES"/*.entries |
    "$KYTHE_DIR/tools/dedup_stream" |
    "$KYTHE_DIR/tools/write_entries" --graphstore "leveldb:$GRAPHSTORE"

"$KYTHE_DIR/tools/write_tables" \
    --graphstore "leveldb:$GRAPHSTORE" \
    --out "$TEMP_SERVING"

cp -a "$TEMP_SERVING" "$SERVING"

cat <<MSG
Indice Kythe generato.

Kzip:
  $KYTHE_OUTPUT_DIRECTORY

Serving tables:
  $SERVING

Avvio:
  docs/kythe-browser/start-kythe.sh
MSG
