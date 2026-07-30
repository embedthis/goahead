#!/usr/bin/env bash
#
#   setup.sh - TestMe setup script to start the test web server.
#
#   Run goahead-test as a backgrounded child and forward signals via a
#   trap. MSYS bash on Windows does not preserve the POSIX parent-child
#   relationship across `exec` of a native .exe, so the bash wrapper would
#   exit immediately and TestMe would see the service as dead. The trap +
#   wait pattern keeps bash alive on every platform; the port-cleanup
#   block above recovers from the SIGKILL-orphan edge case.
#

set -m

#   Free the test ports if a prior run left a stale server bound. The orphan is
#   always a previous goahead-test child (the SIGKILL-orphan edge case noted
#   above), so kill it by process name rather than by port. This avoids lsof,
#   which can hang uninterruptibly (D-state, unkillable by timeout) when a
#   mounted filesystem is unresponsive, wedging the whole test run.
if command -v pkill >/dev/null 2>&1; then
    pkill -f '[g]oahead-test' 2>/dev/null
    sleep 0.3
fi

goahead-test -v --cert self.crt --key self.key &
PID=$!

cleanup() {
    kill $PID 2>/dev/null
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID
