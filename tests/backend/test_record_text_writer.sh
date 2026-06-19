#!/usr/bin/env bash

set -euo pipefail

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I../../core/include \
    test_record_text_writer.c \
    ../../core/src/alfred_record_adapter.c \
    ../../core/src/alfred_record_diagnostic.c \
    ../../core/src/alfred_record_text.c \
    -o /tmp/alfred_test_record_text_writer

/tmp/alfred_test_record_text_writer

echo "PASS backend record text writer"
