#!/bin/bash
#
#   sanitizer-canary.sh - Prove the sanitizer build reports defects.
#
#   Runs goahead-test with each --sanitizer-canary kind and requires a nonzero exit with the matching report.
#   A build without working sanitizers runs the canary to completion and fails here, so a sanitizer that
#   silently dropped out of the build cannot pass the suite.
#
#   The widened groups the Makefile probes for are proven by the compiler that built the binary (CC), not by
#   SANITIZE_FLAGS, so a probe that selected nothing fails here instead of passing with the default groups.
#
#   Usage: CC=compiler sanitizer-canary.sh [path/to/goahead-test]
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
if "${CC:-cc}" --version 2>/dev/null | grep -qi clang ; then
    canary unsigned "runtime error: unsigned integer overflow"
    canary conversion "runtime error: implicit conversion"
else
    canary bounds "out of bounds"
    canary float "outside the range of representable values"
fi
