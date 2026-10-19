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

PIDFILE=".goahead-test.pid"

#   Free the test ports if a prior run of THIS directory left a stale server bound. The orphan is
#   always a previous goahead-test child of this directory (the SIGKILL-orphan edge case noted
#   above), and its pid was recorded here when it started, so signal that pid and nothing else.
#
#   Do not match by process name. A name match reaches every goahead-test on the machine, including
#   the live server of another worktree or a concurrent run of this same suite -- two sessions
#   testing the same repo then kill each other's servers mid-run, and the resulting failures look
#   like real test failures spread across unrelated files. Do not use lsof either: it can hang
#   uninterruptibly (D-state, unkillable by timeout) when a mounted filesystem is unresponsive,
#   wedging the whole test run.
#
#   Confirm the pid is still a goahead-test before signalling it, so a recycled pid belonging to an
#   unrelated process is left alone. If the platform's ps cannot report the command, do not signal:
#   failing to bind is a loud, recoverable error, whereas killing the wrong process is not.
if [ -f "$PIDFILE" ] ; then
    STALE=$(cat "$PIDFILE" 2>/dev/null)
    case "$STALE" in
        ''|*[!0-9]*)
            ;;
        *)
            CMD=$(ps -p "$STALE" -o command= 2>/dev/null)
            if [ -z "$CMD" ] ; then
                #   Cygwin/MSYS ps has no -o; fall back to its table output, which names the binary.
                CMD=$(ps -p "$STALE" 2>/dev/null)
            fi
            case "$CMD" in
                *goahead-test*)
                    kill -9 "$STALE" 2>/dev/null
                    sleep 0.3
                    ;;
            esac
            ;;
    esac
    rm -f "$PIDFILE"
fi

#   Stop private servers left by interrupted runs before they can block this run's ports.
rm -f ../server-orphans.txt
bash utils/reap-servers.sh .servers >&2

# Keep the server log and exit status outside test/ so cleanup cannot remove shutdown failures.
# check-server.sh checks both; readiness uses the healthcheck URL.
# GOAHEAD_TEST_STACK_KB limits the server stack before launch to verify the documented minimum.
# The route table is the one utils/features.sh assembled for this build's feature set.
STATUSFILE="../server-exit.txt"
rm -f "$STATUSFILE"
if [ -n "${GOAHEAD_TEST_STACK_KB:-}" ] ; then
    ulimit -s "$GOAHEAD_TEST_STACK_KB" || exit 1
fi

#   An untrapped subshell owns the server and waits for its exit status. Ignore stop signals so
#   process-group signals cannot interrupt that wait; goahead-test installs its own handlers.
#   The parent traps stop the server, including when the parent exits first.
(
    trap '' SIGINT SIGTERM SIGHUP
    goahead-test -v --cert pki/self.crt --key pki/self.key --route "${TM_ROUTES}" > ../server-log.txt 2>&1 &
    PID=$!
    echo "$PID" > "$PIDFILE"
    wait $PID
    STATUS=$?
    echo "$STATUS" > "$STATUSFILE"
) &
RECORDER=$!
until [ -s "$PIDFILE" ] || ! kill -0 $RECORDER 2>/dev/null ; do
    sleep 0.05
done
PID=$(cat "$PIDFILE" 2>/dev/null)

STOPPED=0
stop() {
    STOPPED=1
    kill $PID 2>/dev/null
}
trap stop SIGINT SIGTERM SIGQUIT
trap 'kill $PID 2>/dev/null' EXIT

wait $RECORDER
while kill -0 $RECORDER 2>/dev/null ; do
    wait $RECORDER
done
trap - SIGINT SIGTERM SIGQUIT EXIT

STATUS=$(cat "$STATUSFILE" 2>/dev/null)
if [ -z "$STATUS" ] ; then
    echo "goahead-test exit status was not recorded" >&2
    STATUS=1
elif [ "$STOPPED" = 0 ] && [ "$STATUS" != 0 ] ; then
    #   tm sees only this script's status, so report an unexpected server exit.
    echo "goahead-test exited with status $STATUS; last lines of server-log.txt:" >&2
    tail -n 40 ../server-log.txt >&2
fi
rm -f "$PIDFILE"
exit $STATUS
