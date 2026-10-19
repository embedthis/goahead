#!/bin/bash
#
#   Check server exit status, sanitizer reports and orphan servers after tm stops.
#   Usage: check-server.sh LOG STATUSFILE [ORPHANS]
#   Wait for setup.sh to record shutdown status; fail if missing or nonzero.
#

LOG="$1"
STATUSFILE="$2"
ORPHANS="$3"
PATTERNS='ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:'

if [ -z "${LOG}" ] || [ -z "${STATUSFILE}" ] ; then
    echo "usage: check-server.sh LOG STATUSFILE [ORPHANS]" >&2
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

if [ -n "${ORPHANS}" ] && [ -s "${ORPHANS}" ] ; then
    echo "      [Error] Tests left servers running; they have been stopped:" >&2
    cat "${ORPHANS}" >&2
    failed=1
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
