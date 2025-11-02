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

mkdir -p cgi-bin web/tmp
cp ../certs/self.key  ../certs/self.crt .
cp ../build/${TESTME_PLATFORM}-${TESTME_PROFILE}/bin/cgitest* cgi-bin
