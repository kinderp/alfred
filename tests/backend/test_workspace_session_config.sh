#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")"

cc -std=c99 -Wall -Wextra -Werror \
    -I../../app/include \
    -I../../modules/inotify/include \
    ../../app/src/config.c \
    test_workspace_session_config.c \
    -o /tmp/alfred_test_workspace_session_config

/tmp/alfred_test_workspace_session_config

echo "PASS backend workspace/session config"
