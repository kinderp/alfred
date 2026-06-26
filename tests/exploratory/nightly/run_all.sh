#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

status=0

for script in \
    base-jsonl-lifecycle.sh \
    moves-jsonl.sh \
    lost-scope-jsonl.sh \
    audit-raw-events.sh \
    recursive-fast-mkdir-p.sh
do
    "$SCRIPT_DIR/$script" || status=1
done

if [[ "${INCLUDE_KNOWN_FAILURES:-0}" == 1 ]]; then
    "$SCRIPT_DIR/root-file-should-fail.sh" || status=1
else
    printf 'SKIP root-file-should-fail.sh '
    printf '(known issue: https://github.com/kinderp/alfred/issues/30)\n'
fi

exit "$status"
