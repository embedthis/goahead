#!/bin/bash
#
#   TestMe cleanup script
#

rm -f *.log self.key self.crt
rm -f cgi-bin/*
rm -rf "web/$(printf 'caf\303\251')"
rm -f web/fifo.txt

exit 0
