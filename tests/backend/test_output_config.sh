#!/usr/bin/env bash

set -euo pipefail

cc -std=c99 -Wall -Wextra -Werror \
    -I../../app/include \
    -I../../modules/inotify/include \
    ../../app/src/config.c \
    test_output_config.c \
    -o /tmp/alfred_test_output_config

/tmp/alfred_test_output_config

echo "PASS backend output config"
