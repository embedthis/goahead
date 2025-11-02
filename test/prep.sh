#!/bin/bash
#
#   prep.sh - TestMe setup script to start web
#
BIN=$(cd "$(dirname "${BIN}")" && pwd)/$(basename "${BIN}")

if [ "$TESTME_OS" = "windows" ] ; then
    EXE=".exe"
    echo "Running prep-test.bat"
    (cd .. ; test/utils/prep-test.bat)
else
    EXE=""
    (cd .. ; test/utils/prep-test.sh)
fi

set -x
echo "@@@ Copying cgitest to cgi-bin @@@"
pwd
ls -l 

mkdir -p cgi-bin
cp ../certs/self.key  ../certs/self.crt .
cp ../build/${TESTME_PLATFORM}-${TESTME_PROFILE}/bin/cgitest* cgi-bin
