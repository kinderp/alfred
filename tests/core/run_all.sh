#!/usr/bin/env bash

set -euo pipefail

echo "========================"
echo " ALFRED CORE TEST SUITE"
echo "========================"

for test_script in test_*.sh; do
    if [[ "$test_script" == "test_lib.sh" ]]; then
        continue
    fi

    echo "Running $test_script"
    bash "$test_script"
    echo ""
done

echo "ALL CORE TESTS PASSED"
