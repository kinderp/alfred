#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
HELPER="$ROOT_DIR/tools/first-user/session-context.sh"
GUIDE="$ROOT_DIR/docs/it/first-user/README.md"
SESSION_ID="FU-TEST-$$"
OUTPUT_FILE="$(mktemp /tmp/alfred_first_user_context_output.XXXXXX)"
SESSION_ROOT=''

session_root_is_safe_for_cleanup() {
    local candidate="$1"
    local canonical

    [[ "$candidate" =~ ^/tmp/alfred-first-user-${SESSION_ID}\.[[:alnum:]]{6}$ ]] ||
        return 1
    [[ -d "$candidate" && ! -L "$candidate" && -O "$candidate" ]] ||
        return 1
    canonical="$(cd -- "$candidate" && pwd -P)" || return 1
    [[ "$candidate" == "$canonical" ]]
}

cleanup() {
    local status=$?

    rm -f -- "$OUTPUT_FILE" || status=1
    if [[ -n "$SESSION_ROOT" ]]; then
        if session_root_is_safe_for_cleanup "$SESSION_ROOT"; then
            find "$SESSION_ROOT" -depth -delete || status=1
        else
            printf 'REFUSED unsafe test cleanup root: %q\n' \
                "$SESSION_ROOT" >&2
            status=1
        fi
    fi
    return "$status"
}
trap cleanup EXIT

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

[[ -x "$HELPER" ]] || fail 'the documented session-context helper is not executable'
"$HELPER" create "$SESSION_ID" > "$OUTPUT_FILE"

SESSION_ROOT="$(sed -n 's/^Session root (local only): //p' "$OUTPUT_FILE")"
CONTEXT_FILE="$(sed -n 's/^Session context file (local only): //p' \
    "$OUTPUT_FILE")"

[[ -n "$SESSION_ROOT" && -d "$SESSION_ROOT" ]] ||
    fail 'create did not produce a session root'
session_root_is_safe_for_cleanup "$SESSION_ROOT" ||
    fail 'create produced a session root outside the bounded cleanup contract'
if session_root_is_safe_for_cleanup /tmp; then
    fail 'cleanup safety predicate accepted /tmp'
fi
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

CLEANUP_BLOCK="$(sed -n '/^## Cleanup$/,/^## Dopo la sessione$/p' "$GUIDE")"
grep -F ': "${repo_root:?contesto sessione non caricato}"' \
    <<< "$CLEANUP_BLOCK" >/dev/null ||
    fail 'cleanup does not guard repo_root'
CLEANUP_CD_LINES="$(grep -nF 'cd "$repo_root"' <<< "$CLEANUP_BLOCK" || true)"
CLEANUP_MAKE_LINES="$(grep -nF \
    'make DESTDIR="$stage_root" PREFIX=/usr uninstall' \
    <<< "$CLEANUP_BLOCK" || true)"
[[ "$CLEANUP_CD_LINES" =~ ^[0-9]+: ]] &&
    [[ "$CLEANUP_CD_LINES" != *$'\n'* ]] ||
    fail 'cleanup must contain exactly one repository cd'
[[ "$CLEANUP_MAKE_LINES" =~ ^[0-9]+: ]] &&
    [[ "$CLEANUP_MAKE_LINES" != *$'\n'* ]] ||
    fail 'cleanup must contain exactly one staged uninstall command'
CLEANUP_CD_LINE="${CLEANUP_CD_LINES%%:*}"
CLEANUP_MAKE_LINE="${CLEANUP_MAKE_LINES%%:*}"
((CLEANUP_CD_LINE < CLEANUP_MAKE_LINE)) ||
    fail 'cleanup must return to the repository before make'

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
        session_id=stale
        repo_root=/stale
        session_root=/stale
        watch_root=/stale
        run_root=/stale
        stage_root=/stale
        session_context=/stale
        if source "$1" load "$2"; then
            exit 10
        fi
        for variable in \
            session_id \
            repo_root \
            session_root \
            watch_root \
            run_root \
            stage_root \
            session_context; do
            [[ ! -v "$variable" ]] || exit 11
        done
        ! declare -F _alfred_first_user_load >/dev/null
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
