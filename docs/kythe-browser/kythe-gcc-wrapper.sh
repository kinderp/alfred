#!/usr/bin/env bash

set -euo pipefail

if [ -z "${KYTHE_DIR:-}" ]; then
    echo "KYTHE_DIR non impostata. Esegui setup-kythe.sh o carica .work/env.sh." >&2
    exit 1
fi

if [ -z "${KYTHE_OUTPUT_DIRECTORY:-}" ]; then
    echo "KYTHE_OUTPUT_DIRECTORY non impostata." >&2
    exit 1
fi

REAL_CC="${KYTHE_REAL_CC:-gcc}"
EXTRACTOR="$KYTHE_DIR/extractors/cxx_extractor"

mkdir -p "$KYTHE_OUTPUT_DIRECTORY"

COMPILE_ONLY=0
for arg in "$@"; do
    if [ "$arg" = "-c" ]; then
        COMPILE_ONLY=1
        break
    fi
done

if [ "$COMPILE_ONLY" -eq 1 ]; then
    EXTRACTOR_LOG="$KYTHE_OUTPUT_DIRECTORY/extractor-errors.log"
    if ! "$EXTRACTOR" "$@" >>"$EXTRACTOR_LOG" 2>&1; then
        {
            printf 'cxx_extractor failed for command:'
            printf ' %q' "$@"
            printf '\n'
        } >>"$EXTRACTOR_LOG"
    fi
fi

exec "$REAL_CC" "$@"
