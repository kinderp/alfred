#!/usr/bin/env bash

set -euo pipefail

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I../../core/include \
    test_record_text_sink.c \
    ../../core/src/alfred_record_diagnostic.c \
    ../../core/src/alfred_record_sink.c \
    ../../core/src/alfred_record_text.c \
    ../../core/src/alfred_record_text_sink.c \
    -o /tmp/alfred_test_record_text_sink

/tmp/alfred_test_record_text_sink

echo "PASS backend record text sink"
