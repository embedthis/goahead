#!/bin/bash
#
#   TestMe cleanup script
#

rm -f *.log .goahead-test.pid
rm -rf pki
rm -f cgi-bin/*
rm -rf "web/$(printf 'caf\303\251')"
rm -f web/fifo.txt

#   Stop and report any private server a test left running. check-server.sh fails the run on the report.
STATUS=0
if ! bash utils/reap-servers.sh .servers > ../server-orphans.txt ; then
    cat ../server-orphans.txt >&2
    STATUS=1
fi
rmdir .servers 2>/dev/null

exit $STATUS
