#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
HELPER="$ROOT_DIR/tools/first-user/session-context.sh"
SESSION_ID="FU-TEST-$$"
OUTPUT_FILE="$(mktemp /tmp/alfred_first_user_context_output.XXXXXX)"
SESSION_ROOT=''

cleanup() {
    rm -f -- "$OUTPUT_FILE"
    if [[ -n "$SESSION_ROOT" && -d "$SESSION_ROOT" ]]; then
        find "$SESSION_ROOT" -depth -delete
    fi
}
trap cleanup EXIT

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

bash "$HELPER" create "$SESSION_ID" > "$OUTPUT_FILE"

SESSION_ROOT="$(sed -n 's/^Session root (local only): //p' "$OUTPUT_FILE")"
CONTEXT_FILE="$(sed -n 's/^Session context file (local only): //p' \
    "$OUTPUT_FILE")"

[[ -n "$SESSION_ROOT" && -d "$SESSION_ROOT" ]] ||
    fail 'create did not produce a session root'
[[ "$CONTEXT_FILE" == "$SESSION_ROOT/session.env" ]] ||
    fail 'create printed an unexpected context path'
[[ "$(stat -c '%a' "$SESSION_ROOT")" == '700' ]] ||
    fail 'session root mode is not 0700'
[[ "$(stat -c '%a' "$CONTEXT_FILE")" == '600' ]] ||
    fail 'context file mode is not 0600'
[[ -f "$SESSION_ROOT/report.md" && -f "$SESSION_ROOT/commands.txt" ]] ||
    fail 'create did not initialize the report artifacts'

printf -v EXPECTED_BOOTSTRAP 'source %q load %q' "$HELPER" "$CONTEXT_FILE"
grep -Fx -- "$EXPECTED_BOOTSTRAP" "$OUTPUT_FILE" >/dev/null ||
    fail 'create did not print the shell-quoted bootstrap command'

for terminal in 1 2; do
    bash -c '
        set -euo pipefail
        source "$1" load "$2"
        [[ "$session_id" == "$3" ]]
        [[ "$repo_root" == "$4" ]]
        [[ "$session_root" == "$5" ]]
        [[ "$watch_root" == "$5/watch" ]]
        [[ "$run_root" == "$5/run" ]]
        [[ "$stage_root" == "$5/stage" ]]
        [[ "$session_context" == "$2" ]]
    ' "terminal-$terminal" "$HELPER" "$CONTEXT_FILE" "$SESSION_ID" \
        "$ROOT_DIR" "$SESSION_ROOT" ||
        fail "fresh terminal $terminal could not load the context"
done

cp -- "$CONTEXT_FILE" "$SESSION_ROOT/session.env.saved"

chmod 0755 -- "$SESSION_ROOT"
if bash -c 'source "$1" load "$2"' test "$HELPER" "$CONTEXT_FILE" \
    >/dev/null 2>&1; then
    fail 'load accepted a session root with mode other than 0700'
fi
chmod 0700 -- "$SESSION_ROOT"

chmod 0644 -- "$CONTEXT_FILE"
if bash -c 'source "$1" load "$2"' test "$HELPER" "$CONTEXT_FILE" \
    >/dev/null 2>&1; then
    fail 'load accepted a context file with mode other than 0600'
fi
cp -- "$SESSION_ROOT/session.env.saved" "$CONTEXT_FILE"
chmod 0600 -- "$CONTEXT_FILE"

grep -v '^run_root=' "$CONTEXT_FILE" > "$SESSION_ROOT/session.env.new"
mv -- "$SESSION_ROOT/session.env.new" "$CONTEXT_FILE"
chmod 0600 -- "$CONTEXT_FILE"
if bash -c 'source "$1" load "$2"' test "$HELPER" "$CONTEXT_FILE" \
    >/dev/null 2>&1; then
    fail 'load accepted a context missing a required key'
fi
cp -- "$SESSION_ROOT/session.env.saved" "$CONTEXT_FILE"
chmod 0600 -- "$CONTEXT_FILE"

sed 's#^watch_root=.*#watch_root=/tmp#' "$CONTEXT_FILE" \
    > "$SESSION_ROOT/session.env.new"
mv -- "$SESSION_ROOT/session.env.new" "$CONTEXT_FILE"
chmod 0600 -- "$CONTEXT_FILE"
if ! bash -c '
        unset watch_root
        if source "$1" load "$2"; then
            exit 10
        fi
        [[ ! ${watch_root+x} ]] || exit 11
    ' test "$HELPER" "$CONTEXT_FILE" >/dev/null 2>&1; then
    fail 'load accepted an out-of-scope root or assigned a partial context'
fi
cp -- "$SESSION_ROOT/session.env.saved" "$CONTEXT_FILE"
chmod 0600 -- "$CONTEXT_FILE"

MARKER="/tmp/alfred_first_user_context_eval_$$"
sed "s#^watch_root=.*#watch_root=\$(touch $MARKER)#" "$CONTEXT_FILE" \
    > "$SESSION_ROOT/session.env.new"
mv -- "$SESSION_ROOT/session.env.new" "$CONTEXT_FILE"
chmod 0600 -- "$CONTEXT_FILE"
if bash -c 'source "$1" load "$2"' test "$HELPER" "$CONTEXT_FILE" \
    >/dev/null 2>&1; then
    fail 'load accepted an executable context value'
fi
[[ ! -e "$MARKER" ]] || fail 'load evaluated context-file contents'
cp -- "$SESSION_ROOT/session.env.saved" "$CONTEXT_FILE"
chmod 0600 -- "$CONTEXT_FILE"

mv -- "$CONTEXT_FILE" "$SESSION_ROOT/session.env.real"
ln -s -- "$SESSION_ROOT/session.env.real" "$CONTEXT_FILE"
if bash -c 'source "$1" load "$2"' test "$HELPER" "$CONTEXT_FILE" \
    >/dev/null 2>&1; then
    fail 'load accepted a symlink context file'
fi
rm -- "$CONTEXT_FILE"
mv -- "$SESSION_ROOT/session.env.real" "$CONTEXT_FILE"

printf 'PASS: private context loads in separate shells and rejects tampering\n'
