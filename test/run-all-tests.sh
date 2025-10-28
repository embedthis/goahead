#!/bin/bash
#
# Run all TypeScript tests and report results
#

cd "$(dirname "$0")"

echo "Running all TypeScript tests..."
echo "================================"

# Run all tests
tm **/*.tst.ts 2>&1 | tee test-results.log

# Extract summary
echo ""
echo "================================"
echo "Final Summary:"
grep -A10 "TEST SUMMARY" test-results.log | tail -8
