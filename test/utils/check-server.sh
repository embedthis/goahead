#!/bin/bash
#
#   check-server.sh - Fail a test run on the test server's own failure
#
#   usage: check-server.sh LOG STATUSFILE
#
#   test/setup.sh writes the server's exit status to STATUSFILE once the server has stopped, which is
#   after tm has stopped the service. A crash or sanitizer report during shutdown is visible only
#   there and in the server's LOG, never in a test result. Wait for the status, then fail if it is
#   missing or nonzero, or if the log holds a sanitizer report, and print the evidence.
#
#   Run by the Makefile test and test-asan targets after tm.
#

LOG="$1"
STATUSFILE="$2"
PATTERNS='ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:'

if [ -z "${LOG}" ] || [ -z "${STATUSFILE}" ] ; then
    echo "usage: check-server.sh LOG STATUSFILE" >&2
    exit 2
fi

tries=0
while [ ! -s "${STATUSFILE}" ] && [ ${tries} -lt 100 ] ; do
    sleep 0.1
    tries=$((tries + 1))
done

failed=0
if [ ! -s "${STATUSFILE}" ] ; then
    echo "      [Error] The test server's exit status was not recorded in ${STATUSFILE}" >&2
    failed=1
else
    status="$(cat "${STATUSFILE}")"
    if [ "${status}" != "0" ] ; then
        echo "      [Error] The test server exited with status ${status}" >&2
        failed=1
    fi
fi

if [ ! -f "${LOG}" ] ; then
    echo "      [Error] The test server log ${LOG} is missing" >&2
    exit 1
fi

if grep -qE "${PATTERNS}" "${LOG}" ; then
    echo "      [Error] The test server log has a sanitizer report" >&2
    echo "---- ${LOG}: sanitizer reports ----" >&2
    grep -E -m 3 -A 40 "${PATTERNS}" "${LOG}" >&2
    failed=1
fi

if [ ${failed} -ne 0 ] ; then
    echo "---- ${LOG}: last 40 lines ----" >&2
    tail -n 40 "${LOG}" >&2
    echo "---- end ${LOG} ----" >&2
    exit 1
fi
exit 0
