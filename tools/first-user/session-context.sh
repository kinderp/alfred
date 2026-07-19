#!/usr/bin/env bash

# Create and load the private path context used by a first-user session.
# `create` runs as a command; `load` must be sourced so validated variables are
# assigned in the current terminal.

_alfred_first_user_error() {
    printf 'first-user session context error: %s\n' "$1" >&2
}

_alfred_first_user_has_control_character() {
    [[ "$1" == *$'\n'* || "$1" == *$'\r'* || "$1" == *$'\t'* ]]
}

_alfred_first_user_clear_context() {
    unset -v \
        session_id \
        repo_root \
        session_root \
        watch_root \
        run_root \
        stage_root \
        session_context
}

_alfred_first_user_script_path() {
    local script_dir

    script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)" ||
        return 1
    printf '%s/session-context.sh\n' "$script_dir"
}

_alfred_first_user_repo_root() {
    local script_path
    local script_dir

    script_path="$(_alfred_first_user_script_path)" || return 1
    script_dir="${script_path%/*}"
    cd -- "$script_dir/../.." && pwd -P
}

_alfred_first_user_create() {
    local session_id
    local repo_root
    local script_path
    local session_root
    local watch_root
    local run_root
    local stage_root
    local context_file
    local bootstrap

    if [[ $# -ne 1 ]]; then
        _alfred_first_user_error \
            'usage: tools/first-user/session-context.sh create SESSION_ID'
        return 2
    fi

    session_id="$1"
    if [[ ! "$session_id" =~ ^[A-Za-z0-9-]+$ ]]; then
        _alfred_first_user_error \
            'SESSION_ID must contain only ASCII letters, digits and hyphens'
        return 2
    fi

    repo_root="$(_alfred_first_user_repo_root)" || {
        _alfred_first_user_error 'cannot resolve the repository root'
        return 1
    }
    script_path="$(_alfred_first_user_script_path)" || {
        _alfred_first_user_error 'cannot resolve the context helper path'
        return 1
    }

    if _alfred_first_user_has_control_character "$repo_root"; then
        _alfred_first_user_error \
            'the repository path must not contain tabs or newlines'
        return 2
    fi

    umask 077
    session_root="$(mktemp -d \
        "/tmp/alfred-first-user-${session_id}.XXXXXX")" || {
        _alfred_first_user_error 'cannot create the disposable session root'
        return 1
    }

    watch_root="$session_root/watch"
    run_root="$session_root/run"
    stage_root="$session_root/stage"
    context_file="$session_root/session.env"

    if ! mkdir -p -- "$watch_root" "$run_root" "$stage_root" ||
       ! cp -- "$repo_root/docs/it/first-user/report-template.md" \
           "$session_root/report.md" ||
       ! : > "$session_root/commands.txt" ||
       ! printf '%s\n' \
           'version=1' \
           "session_id=$session_id" \
           "repo_root=$repo_root" \
           "session_root=$session_root" \
           "watch_root=$watch_root" \
           "run_root=$run_root" \
           "stage_root=$stage_root" \
           > "$context_file" ||
       ! chmod 0600 -- "$context_file"; then
        _alfred_first_user_error 'cannot initialize the session files'
        find "$session_root" -depth -delete 2>/dev/null || true
        return 1
    fi

    printf -v bootstrap 'source %q load %q' "$script_path" "$context_file"
    printf 'Session root (local only): %s\n' "$session_root"
    printf 'Session context file (local only): %s\n' "$context_file"
    printf 'Run this exact bootstrap in this shell and every new terminal:\n'
    printf '%s\n' "$bootstrap"
}

_alfred_first_user_load() {
    local context_file
    local context_dir
    local canonical_context
    local mode
    local context_size
    local line
    local key
    local value
    local version_value=''
    local session_id_value=''
    local repo_root_value=''
    local session_root_value=''
    local watch_root_value=''
    local run_root_value=''
    local stage_root_value=''
    local seen_version=0
    local seen_session_id=0
    local seen_repo_root=0
    local seen_session_root=0
    local seen_watch_root=0
    local seen_run_root=0
    local seen_stage_root=0
    local actual_repo_root
    local path
    local canonical_path
    local session_mode

    if [[ $# -ne 1 ]]; then
        _alfred_first_user_error \
            'usage: source tools/first-user/session-context.sh load CONTEXT_FILE'
        return 2
    fi

    context_file="$1"
    if [[ "$context_file" != /* ]] ||
       _alfred_first_user_has_control_character "$context_file"; then
        _alfred_first_user_error \
            'the context file path must be absolute and contain no tabs or newlines'
        return 2
    fi
    if [[ ! -f "$context_file" || -L "$context_file" || ! -O "$context_file" ]]; then
        _alfred_first_user_error \
            'the context must be an owner-controlled regular file, not a symlink'
        return 2
    fi

    mode="$(stat -c '%a' -- "$context_file" 2>/dev/null)" || {
        _alfred_first_user_error 'cannot inspect context file permissions'
        return 1
    }
    if [[ "$mode" != '600' ]]; then
        _alfred_first_user_error 'the context file mode must be 0600'
        return 2
    fi
    context_size="$(stat -c '%s' -- "$context_file" 2>/dev/null)" || {
        _alfred_first_user_error 'cannot inspect context file size'
        return 1
    }
    if [[ ! "$context_size" =~ ^[0-9]+$ ]] ||
       ((context_size == 0 || context_size > 32768)); then
        _alfred_first_user_error \
            'the context file size must be between 1 and 32768 bytes'
        return 2
    fi

    context_dir="$(cd -- "$(dirname -- "$context_file")" && pwd -P)" || {
        _alfred_first_user_error 'cannot resolve the context directory'
        return 1
    }
    canonical_context="$context_dir/$(basename -- "$context_file")"
    if [[ "$context_file" != "$canonical_context" ]]; then
        _alfred_first_user_error 'the context file path must be canonical'
        return 2
    fi

    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" != *=* ]]; then
            _alfred_first_user_error 'the context contains a malformed line'
            return 2
        fi
        key="${line%%=*}"
        value="${line#*=}"
        if [[ -z "$value" ]] || _alfred_first_user_has_control_character "$value"; then
            _alfred_first_user_error \
                "the context contains an empty or unsafe value for $key"
            return 2
        fi

        case "$key" in
            version)
                ((seen_version += 1))
                version_value="$value"
                ;;
            session_id)
                ((seen_session_id += 1))
                session_id_value="$value"
                ;;
            repo_root)
                ((seen_repo_root += 1))
                repo_root_value="$value"
                ;;
            session_root)
                ((seen_session_root += 1))
                session_root_value="$value"
                ;;
            watch_root)
                ((seen_watch_root += 1))
                watch_root_value="$value"
                ;;
            run_root)
                ((seen_run_root += 1))
                run_root_value="$value"
                ;;
            stage_root)
                ((seen_stage_root += 1))
                stage_root_value="$value"
                ;;
            *)
                _alfred_first_user_error \
                    "the context contains an unknown key: $key"
                return 2
                ;;
        esac
    done < "$context_file"

    if [[ "$seen_version" -ne 1 || "$seen_session_id" -ne 1 ||
          "$seen_repo_root" -ne 1 || "$seen_session_root" -ne 1 ||
          "$seen_watch_root" -ne 1 || "$seen_run_root" -ne 1 ||
          "$seen_stage_root" -ne 1 ]]; then
        _alfred_first_user_error \
            'the context must contain each required key exactly once'
        return 2
    fi
    if [[ "$version_value" != '1' ]]; then
        _alfred_first_user_error 'unsupported context version'
        return 2
    fi
    if [[ ! "$session_id_value" =~ ^[A-Za-z0-9-]+$ ]]; then
        _alfred_first_user_error 'the context contains an invalid session ID'
        return 2
    fi

    actual_repo_root="$(_alfred_first_user_repo_root)" || {
        _alfred_first_user_error 'cannot resolve the helper repository root'
        return 1
    }
    if [[ "$repo_root_value" != "$actual_repo_root" ]]; then
        _alfred_first_user_error \
            'the context repository root does not match this helper'
        return 2
    fi
    if [[ ! "$session_root_value" =~ ^/tmp/alfred-first-user-${session_id_value}\.[[:alnum:]]{6}$ ]]; then
        _alfred_first_user_error \
            'the session root does not match the bounded /tmp session pattern'
        return 2
    fi
    if [[ ! -O "$session_root_value" || -L "$session_root_value" ]]; then
        _alfred_first_user_error \
            'the session root must be an owner-controlled directory, not a symlink'
        return 2
    fi
    session_mode="$(stat -c '%a' -- "$session_root_value" 2>/dev/null)" || {
        _alfred_first_user_error 'cannot inspect session root permissions'
        return 1
    }
    if [[ "$session_mode" != '700' ]]; then
        _alfred_first_user_error 'the session root mode must be 0700'
        return 2
    fi

    for path in \
        "$repo_root_value" \
        "$session_root_value" \
        "$watch_root_value" \
        "$run_root_value" \
        "$stage_root_value"; do
        if [[ "$path" != /* || ! -d "$path" ]]; then
            _alfred_first_user_error \
                'every context root must be an existing absolute directory'
            return 2
        fi
        canonical_path="$(cd -- "$path" && pwd -P)" || {
            _alfred_first_user_error 'cannot resolve a context root'
            return 1
        }
        if [[ "$path" != "$canonical_path" ]]; then
            _alfred_first_user_error 'every context root must be canonical'
            return 2
        fi
    done

    if [[ "$watch_root_value" != "$session_root_value/watch" ||
          "$run_root_value" != "$session_root_value/run" ||
          "$stage_root_value" != "$session_root_value/stage" ]]; then
        _alfred_first_user_error \
            'watch, run and stage must be their expected session-root descendants'
        return 2
    fi
    if [[ "$context_file" != "$session_root_value/session.env" ]]; then
        _alfred_first_user_error \
            'the context file must be session_root/session.env'
        return 2
    fi

    declare -g session_id="$session_id_value"
    declare -g repo_root="$repo_root_value"
    declare -g session_root="$session_root_value"
    declare -g watch_root="$watch_root_value"
    declare -g run_root="$run_root_value"
    declare -g stage_root="$stage_root_value"
    declare -g session_context="$context_file"

    printf 'session context OK\n'
}

_alfred_first_user_cleanup_functions() {
    unset -f \
        _alfred_first_user_error \
        _alfred_first_user_has_control_character \
        _alfred_first_user_clear_context \
        _alfred_first_user_script_path \
        _alfred_first_user_repo_root \
        _alfred_first_user_create \
        _alfred_first_user_load \
        _alfred_first_user_source_entry \
        _alfred_first_user_cleanup_functions
}

_alfred_first_user_source_entry() {
    local status

    if ! _alfred_first_user_clear_context; then
        _alfred_first_user_error \
            'cannot clear a previously loaded session context'
        status=1
    elif [[ "${1:-}" != 'load' ]]; then
        _alfred_first_user_error \
            'source this helper with: source session-context.sh load CONTEXT_FILE'
        status=2
    else
        shift
        if _alfred_first_user_load "$@"; then
            status=0
        else
            status=$?
        fi
    fi

    _alfred_first_user_cleanup_functions
    return "$status"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    if [[ "${1:-}" != 'create' ]]; then
        _alfred_first_user_error \
            'run this helper with: session-context.sh create SESSION_ID'
        exit 2
    fi
    shift
    _alfred_first_user_create "$@"
    exit $?
fi

_alfred_first_user_source_entry "$@"
return $?
