#!/bin/bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

set -x
echo "@@ Starting GoAhead test server on port localhost:18080"
which goahead-test

if curl -s http://localhost:18080 / >/dev/null 2>&1; then
    echo "GoAhead is already running on port localhost:18080"
    sleep 999999 &
else
    goahead-test -v &
fi

PID=$!

cleanup() {
    kill $PID
    kill -9 $PID
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID
