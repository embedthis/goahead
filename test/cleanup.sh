#!/bin/bash
#
#   TestMe cleanup script
#

rm -f *.log .goahead-test.pid
rm -rf pki
rm -f cgi-bin/*
rm -rf "web/$(printf 'caf\303\251')"
rm -f web/fifo.txt

exit 0
