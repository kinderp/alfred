#!/usr/bin/env bash

set -eu

usage() {
    cat <<'EOF'
Usage:
  upload_audit_artifacts.sh --date YYYY-MM-DD [options]

Options:
  --date YYYY-MM-DD       Audit date used to find /tmp artifacts.
  --issue N               GitHub issue number to update with the Drive link.
  --repo owner/name       GitHub repository. Default: kinderp/alfred.
  --remote name:          rclone remote. Default: gdrive:
  --dest path             Drive destination under remote. Default: alfred/audits.
  --dry-run               Create the archive but do not upload or update GitHub.
  -h, --help              Show this help.

Example:
  tests/exploratory/nightly/upload_audit_artifacts.sh \
    --date 2026-06-25 \
    --issue 29

Prerequisites:
  - rclone configured with a Google Drive remote, for example gdrive:
  - zip available locally
  - gh authenticated if --issue is used
EOF
}

DATE=
ISSUE=
REPO=kinderp/alfred
REMOTE=gdrive:
DEST=alfred/audits
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --date)
            DATE=${2:-}
            shift 2
            ;;
        --issue)
            ISSUE=${2:-}
            shift 2
            ;;
        --repo)
            REPO=${2:-}
            shift 2
            ;;
        --remote)
            REMOTE=${2:-}
            shift 2
            ;;
        --dest)
            DEST=${2:-}
            shift 2
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown argument: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "$DATE" ]]; then
    printf 'Missing required --date YYYY-MM-DD\n\n' >&2
    usage >&2
    exit 2
fi

if ! command -v zip >/dev/null 2>&1; then
    printf 'zip is required but was not found in PATH\n' >&2
    exit 1
fi

if [[ "$DRY_RUN" == 0 ]] && ! command -v rclone >/dev/null 2>&1; then
    printf 'rclone is required for upload but was not found in PATH\n' >&2
    exit 1
fi

pattern="/tmp/alfred-nightly-audit-${DATE}*"
archive="/tmp/alfred-nightly-audit-${DATE}.zip"

shopt -s nullglob
artifact_candidates=( $pattern )
shopt -u nullglob

artifacts=()
for candidate in "${artifact_candidates[@]}"; do
    if [[ -d "$candidate" ]]; then
        artifacts+=( "$candidate" )
    fi
done

if [[ ${#artifacts[@]} -eq 0 ]]; then
    printf 'No audit artifact directories found for pattern: %s\n' "$pattern" >&2
    exit 1
fi

rm -f "$archive"
zip -r "$archive" "${artifacts[@]}"

printf 'Created archive: %s\n' "$archive"

if [[ "$DRY_RUN" == 1 ]]; then
    printf 'Dry run enabled; upload and GitHub update skipped.\n'
    exit 0
fi

rclone mkdir "${REMOTE}${DEST}"
rclone copy "$archive" "${REMOTE}${DEST}/"

remote_file="${REMOTE}${DEST}/$(basename "$archive")"
drive_link=$(rclone link "$remote_file")

printf 'Uploaded archive: %s\n' "$remote_file"
printf 'Drive link: %s\n' "$drive_link"

if [[ -n "$ISSUE" ]]; then
    if ! command -v gh >/dev/null 2>&1; then
        printf 'gh is required to update issue %s but was not found in PATH\n' \
            "$ISSUE" >&2
        exit 1
    fi

    comment=$(cat <<EOF
Audit artifacts uploaded to Google Drive:

- Date: ${DATE}
- Archive: \`$(basename "$archive")\`
- Link: ${drive_link}

The archive contains the local \`/tmp/alfred-nightly-audit-${DATE}*\` folders
used during the exploratory audit.
EOF
)

    gh issue comment "$ISSUE" --repo "$REPO" --body "$comment"
    printf 'Updated issue: https://github.com/%s/issues/%s\n' "$REPO" "$ISSUE"
fi
