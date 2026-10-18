#!/bin/bash
#
#   sanitizer-canary.sh - Prove the sanitizer build reports defects.
#
#   Runs goahead-test with each --sanitizer-canary kind and requires a nonzero exit with the matching report.
#   A build without working sanitizers runs the canary to completion and fails here, so a sanitizer that
#   silently dropped out of the build cannot pass the suite.
#
#   Usage: sanitizer-canary.sh [path/to/goahead-test]
#
TOP="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${1:-$TOP/build/bin/goahead-test}"

canary() {
    local kind="$1" pattern="$2" out rc

    out="$("$BIN" --sanitizer-canary "$kind" 2>&1)"
    rc=$?
    if [ $rc -eq 0 ] || ! printf '%s\n' "$out" | grep -q "$pattern" ; then
        echo "sanitizer canary '$kind': no sanitizer report (exit $rc, expected \"$pattern\")" >&2
        printf '%s\n' "$out" | head -20 >&2
        exit 1
    fi
    echo "sanitizer canary '$kind': reported"
}

if [ ! -x "$BIN" ] ; then
    echo "sanitizer canary: $BIN not found" >&2
    exit 1
fi
canary heap "ERROR: AddressSanitizer: heap-buffer-overflow"
canary signed "runtime error:"
