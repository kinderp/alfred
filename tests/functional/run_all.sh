#!/usr/bin/env bash

set -e

echo "========================"
echo " FS MON TEST SUITE"
echo "========================"

for t in test_*.sh; do
    echo "Running $t"
    bash "$t"
    echo ""
done

echo "ALL TESTS PASSED ✔"
