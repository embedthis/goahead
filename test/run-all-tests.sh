#!/bin/bash
#
#   run-all-tests.sh - Run all TypeScript tests and report results
#
#   usage: run-all-tests.sh [--strict-skips] [--help]
#
#   --strict-skips  Fail the run if a feature that is on by default (authentication, CGI, JavaScript
#                   templates, upload or SSL) is off in the build. The tests gated on such a feature
#                   skip, and tm reports a skipped test as a pass, so without this a configuration
#                   that turns a feature off loses its coverage while the run still passes. Sets
#                   TM_STRICT_SKIPS, which basic/feature-gates.tst.ts checks. CI runs this way.
#
#   Exits with tm's status.
#

cd "$(dirname "$0")"

for arg in "$@" ; do
    case "$arg" in
    --strict-skips)
        export TM_STRICT_SKIPS=1
        ;;
    --help|-h)
        sed -n '3,14p' "$(basename "$0")" | sed -e 's/^#//' -e 's/^   //'
        exit 0
        ;;
    *)
        echo "run-all-tests.sh: unknown option $arg (see --help)" >&2
        exit 2
        ;;
    esac
done

echo "Running all TypeScript tests..."
echo "================================"

set -o pipefail
tm **/*.tst.ts 2>&1 | tee test-results.log
status=$?

echo ""
echo "================================"
echo "Final Summary:"
grep -A10 "TEST SUMMARY" test-results.log | tail -8
exit $status
