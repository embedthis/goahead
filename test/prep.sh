#!/bin/bash
#
#   prep.sh - TestMe setup script to start web
#

cp ../certs/self.key  ../certs/self.crt .
cp ../build/${TESTME_PLATFORM}-${TESTME_PROFILE}/bin/cgitest* cgi-bin
