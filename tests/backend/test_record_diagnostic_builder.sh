#!/usr/bin/env bash

set -euo pipefail

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I../../core/include \
    test_record_diagnostic_builder.c \
    ../../core/src/alfred_record_diagnostic.c \
    -o /tmp/alfred_test_record_diagnostic_builder

/tmp/alfred_test_record_diagnostic_builder

echo "PASS backend record diagnostic builder"
