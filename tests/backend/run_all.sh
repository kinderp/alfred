#!/usr/bin/env bash

set -euo pipefail

echo "=============================="
echo " ALFRED BACKEND DIAGNOSTICS"
echo "=============================="

for test_script in test_*.sh; do
    echo "Running $test_script"
    bash "$test_script"
    echo ""
done

echo "ALL BACKEND DIAGNOSTICS PASSED"
