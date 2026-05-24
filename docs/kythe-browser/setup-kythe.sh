#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="$SCRIPT_DIR/.work"
KYTHE_VERSION="${KYTHE_VERSION:-v0.0.68}"
KYTHE_DIR="$WORK_DIR/kythe"
VERSION_FILE="$WORK_DIR/kythe.version"

mkdir -p "$WORK_DIR"

ARCHIVE="$WORK_DIR/kythe-$KYTHE_VERSION.tar.gz"
URL="https://github.com/kythe/kythe/releases/download/$KYTHE_VERSION/kythe-$KYTHE_VERSION.tar.gz"

INSTALLED_VERSION=""
if [ -f "$VERSION_FILE" ]; then
    INSTALLED_VERSION="$(cat "$VERSION_FILE")"
fi

if [ ! -x "$KYTHE_DIR/tools/http_server" ] || [ "$INSTALLED_VERSION" != "$KYTHE_VERSION" ]; then
    rm -rf "$KYTHE_DIR"
    curl -fL "$URL" -o "$ARCHIVE"
    mkdir -p "$KYTHE_DIR"
    tar --strip-components=1 -xzf "$ARCHIVE" -C "$KYTHE_DIR"
    printf '%s\n' "$KYTHE_VERSION" > "$VERSION_FILE"
fi

cat > "$WORK_DIR/env.sh" <<EOF
export KYTHE_VERSION="$KYTHE_VERSION"
export KYTHE_DIR="$KYTHE_DIR"
export PATH="\$KYTHE_DIR/tools:\$PATH"
EOF

cat <<MSG
Kythe installato in:
  $KYTHE_DIR

Versione:
  $KYTHE_VERSION

Prossimo passo:
  docs/kythe-browser/reindex-kythe.sh
MSG
