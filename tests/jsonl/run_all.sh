#!/usr/bin/env bash

set -euo pipefail

echo "=============================="
echo " ALFRED JSONL GOLDEN TESTS"
echo "=============================="

for test_script in test_*.sh; do
    echo "Running $test_script"
    bash "$test_script"
    echo ""
done

echo "ALL JSONL GOLDEN TESTS PASSED"
