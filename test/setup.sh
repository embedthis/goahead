#!/bin/bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

if curl -s http://127.0.0.1:18080 > /dev/null 2>&1; then
    echo "GoAhead is already running on port 18080"
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
