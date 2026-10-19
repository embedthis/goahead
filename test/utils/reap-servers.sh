#!/usr/bin/env bash
#
#   Stop orphan test servers recorded under DIR/PID, including their process groups.
#   Usage: reap-servers.sh DIR
#   Remove all records; exit 1 if a server was still running.
#   Verify each PID still names goahead before signalling; skip it if ps cannot verify it.
#

DIR="$1"
if [ -z "${DIR}" ] ; then
    echo "usage: reap-servers.sh DIR" >&2
    exit 2
fi

found=0
for record in "${DIR}"/* ; do
    [ -f "${record}" ] || continue
    pid="$(basename "${record}")"
    test="$(head -n 1 "${record}" 2>/dev/null)"
    rm -f "${record}"
    case "${pid}" in
        ''|*[!0-9]*)
            continue
            ;;
    esac
    cmd="$(ps -p "${pid}" -o command= 2>/dev/null)"
    case "${cmd}" in
        *goahead*)
            kill -9 -- "-${pid}" 2>/dev/null
            kill -9 "${pid}" 2>/dev/null
            echo "${test} left a server running (pid ${pid}): ${cmd}"
            found=1
            ;;
    esac
done
exit ${found}
