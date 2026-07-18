#!/usr/bin/env bash

# Focused staged-install contract tests. The suite never writes to the host
# installation prefix and never requires root.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/alfred_install_test.XXXXXX")"
SOURCE_DIR="$RUN_DIR/sources"

cleanup() {
    if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" == "1" ]]; then
        printf "Kept staged-install artifacts in %s\n" "$RUN_DIR"
    else
        rm -rf "$RUN_DIR"
    fi
}

fail() {
    printf "FAIL: %s\n" "$1" >&2
    find "$RUN_DIR" -mindepth 1 -maxdepth 8 -print 2>/dev/null || true
    exit 1
}

run_make() {
    make --no-print-directory -C "$ROOT_DIR" "$@"
}

assert_layout_rejected() {
    local case_name="$1"
    shift

    if run_make "$@" validate-install-layout \
        >"$RUN_DIR/layout-$case_name.out" 2>&1; then
        fail "install layout validator accepted $case_name"
    fi
}

assert_file() {
    [[ -f "$1" ]] || fail "missing staged file: $1"
}

assert_absent() {
    [[ ! -e "$1" ]] || fail "unexpected staged path: $1"
}

assert_mode() {
    local expected="$1"
    local path="$2"
    local actual

    actual="$(stat -c '%a' "$path")"
    [[ "$actual" == "$expected" ]] ||
        fail "mode for $path is $actual, expected $expected"
}

assert_empty_stage() {
    local stage="$1"

    if find "$stage" -mindepth 1 -print -quit | grep -q .; then
        fail "failed preflight modified stage: $stage"
    fi
}

assert_exact_files() {
    local stage="$1"
    shift
    local actual
    local expected

    actual="$({
        while IFS= read -r path; do
            printf '%s\n' "${path#"$stage"/}"
        done < <(find "$stage" -type f -print)
    } | LC_ALL=C sort)"
    expected="$(printf '%s\n' "$@" | LC_ALL=C sort)"

    [[ "$actual" == "$expected" ]] || {
        printf '%s\n' "expected files:" "$expected" >&2
        printf '%s\n' "actual files:" "$actual" >&2
        fail "stage contains an unexpected file set"
    }
}

assert_cli_and_manuals() {
    local binary="$1"
    local man_root="$2"
    local config="$RUN_DIR/valid.conf"

    cat > "$config" <<'EOF'
output_enabled=false
output_format=jsonl
output_buffer_size=65536
output_log=output.jsonl
event_engine=core
EOF

    "$binary" --version >/dev/null
    "$binary" --help >/dev/null
    "$binary" -c "$config" --check-config >/dev/null

    command -v man >/dev/null 2>&1 ||
        fail "man is required by the reference staged-install suite"

    man -l "$man_root/man1/alfred.1" >/dev/null
    man -l "$man_root/man5/alfred.conf.5" >/dev/null
    man -l "$man_root/man7/alfred-events.7" >/dev/null
    man -l "$man_root/it/man1/alfred.1" >/dev/null
    man -l "$man_root/it/man5/alfred.conf.5" >/dev/null
    man -l "$man_root/it/man7/alfred-events.7" >/dev/null
}

test_default_layout() {
    local stage="$RUN_DIR/default-stage"
    local binary="$stage/usr/local/bin/alfred"
    local man_root="$stage/usr/local/share/man"

    mkdir -p "$stage/usr/local/bin" "$man_root/man1"
    printf 'keep\n' > "$stage/usr/local/bin/not-alfred"
    printf 'keep\n' > "$man_root/man1/not-alfred.1"

    run_make DESTDIR="$stage" install

    assert_file "$binary"
    assert_mode 755 "$binary"
    for page in \
        "$man_root/man1/alfred.1" \
        "$man_root/man5/alfred.conf.5" \
        "$man_root/man7/alfred-events.7" \
        "$man_root/it/man1/alfred.1" \
        "$man_root/it/man5/alfred.conf.5" \
        "$man_root/it/man7/alfred-events.7"; do
        assert_file "$page"
        assert_mode 644 "$page"
    done

    assert_exact_files "$stage" \
        usr/local/bin/alfred \
        usr/local/bin/not-alfred \
        usr/local/share/man/man1/alfred.1 \
        usr/local/share/man/man1/not-alfred.1 \
        usr/local/share/man/man5/alfred.conf.5 \
        usr/local/share/man/man7/alfred-events.7 \
        usr/local/share/man/it/man1/alfred.1 \
        usr/local/share/man/it/man5/alfred.conf.5 \
        usr/local/share/man/it/man7/alfred-events.7

    assert_cli_and_manuals "$binary" "$man_root"

    run_make DESTDIR="$stage" uninstall
    assert_absent "$binary"
    assert_file "$stage/usr/local/bin/not-alfred"
    assert_file "$man_root/man1/not-alfred.1"
    assert_exact_files "$stage" \
        usr/local/bin/not-alfred \
        usr/local/share/man/man1/not-alfred.1

    run_make DESTDIR="$stage" uninstall
    assert_exact_files "$stage" \
        usr/local/bin/not-alfred \
        usr/local/share/man/man1/not-alfred.1
}

test_prefix_override_layout() {
    local stage="$RUN_DIR/prefix-override-stage"

    mkdir -p "$stage"

    run_make DESTDIR="$stage" PREFIX=/usr install

    assert_file "$stage/usr/bin/alfred"
    assert_file "$stage/usr/share/man/man1/alfred.1"
    assert_file "$stage/usr/share/man/it/man7/alfred-events.7"
    assert_absent "$stage/usr/local/bin/alfred"

    run_make DESTDIR="$stage" PREFIX=/usr uninstall
    assert_exact_files "$stage"
}

test_custom_layout() {
    local stage="$RUN_DIR/custom-stage"
    local bindir=/opt/alfred/bin
    local mandir=/opt/alfred/manual

    mkdir -p "$stage$bindir" "$stage$mandir/man1"
    printf 'keep\n' > "$stage$bindir/not-alfred"
    printf 'keep\n' > "$stage$mandir/man1/not-alfred.1"

    run_make \
        DESTDIR="$stage" \
        PREFIX=/ignored \
        BINDIR="$bindir" \
        MANDIR="$mandir" \
        install

    assert_file "$stage$bindir/alfred"
    assert_file "$stage$mandir/man1/alfred.1"
    assert_file "$stage$mandir/it/man7/alfred-events.7"

    run_make \
        DESTDIR="$stage" \
        PREFIX=/ignored \
        BINDIR="$bindir" \
        MANDIR="$mandir" \
        uninstall

    assert_absent "$stage$bindir/alfred"
    assert_absent "$stage$mandir/man1/alfred.1"
    assert_file "$stage$bindir/not-alfred"
    assert_file "$stage$mandir/man1/not-alfred.1"
    assert_exact_files "$stage" \
        opt/alfred/bin/not-alfred \
        opt/alfred/manual/man1/not-alfred.1
}

test_unreadable_binary_preflight() {
    local stage="$RUN_DIR/unreadable-stage"
    local source="$SOURCE_DIR/unreadable-alfred"

    mkdir -p "$stage" "$SOURCE_DIR"
    cp "$ROOT_DIR/alfred" "$source"
    chmod 0111 "$source"
    [[ ! -r "$source" ]] || fail "test filesystem cannot represent unreadable source"

    if run_make \
        DESTDIR="$stage" \
        PREFIX=/usr \
        ALFRED_INSTALL_BINARY_SOURCE="$source" \
        install >"$RUN_DIR/unreadable.out" 2>&1; then
        fail "install accepted an unreadable binary source"
    fi

    assert_empty_stage "$stage"
}

test_missing_manual_preflight() {
    local stage="$RUN_DIR/missing-manual-stage"
    local missing="$SOURCE_DIR/missing-alfred-events-it.7"

    mkdir -p "$stage" "$SOURCE_DIR"

    if run_make \
        DESTDIR="$stage" \
        PREFIX=/usr \
        ALFRED_MAN7_IT_SOURCE="$missing" \
        install >"$RUN_DIR/missing-manual.out" 2>&1; then
        fail "install accepted a missing manual source"
    fi

    assert_empty_stage "$stage"
}

test_invalid_layout_preflight() {
    local stage="$RUN_DIR/invalid-layout-stage"

    mkdir -p "$stage"

    assert_layout_rejected relative-bindir BINDIR=relative/bin
    assert_layout_rejected relative-mandir MANDIR=relative/man
    assert_layout_rejected parent-bindir BINDIR=/opt/alfred/../bin
    assert_layout_rejected parent-mandir MANDIR=/opt/alfred/../man
    assert_layout_rejected relative-destdir DESTDIR=relative/stage
    assert_layout_rejected parent-destdir DESTDIR=/tmp/alfred/../stage

    # The trailing slash keeps the destination inside this temporary stage if
    # a future regression accidentally lets the relative BINDIR reach install.
    if run_make \
        DESTDIR="$stage/" \
        BINDIR=relative/bin \
        install >"$RUN_DIR/invalid-layout.out" 2>&1; then
        fail "install accepted a relative BINDIR"
    fi

    assert_empty_stage "$stage"
}

trap cleanup EXIT

printf '%s\n' '=============================='
printf '%s\n' ' ALFRED STAGED-INSTALL TESTS'
printf '%s\n' '=============================='

test_default_layout
printf '%s\n' 'PASS default staged layout, CLI, manuals, and uninstall ownership'
test_prefix_override_layout
printf '%s\n' 'PASS PREFIX=/usr staged layout override'
test_custom_layout
printf '%s\n' 'PASS custom BINDIR and MANDIR layout'
test_unreadable_binary_preflight
printf '%s\n' 'PASS unreadable binary preflight'
test_missing_manual_preflight
printf '%s\n' 'PASS missing manual preflight'
test_invalid_layout_preflight
printf '%s\n' 'PASS invalid layout preflight'

printf '%s\n' 'ALL STAGED-INSTALL TESTS PASSED'
