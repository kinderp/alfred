#!/usr/bin/env bash

set -euo pipefail

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I../../core/include \
    test_record_raw_adapter.c \
    ../../core/src/alfred_record_adapter.c \
    -o /tmp/alfred_test_record_raw_adapter

/tmp/alfred_test_record_raw_adapter

echo "PASS backend record raw adapter"
