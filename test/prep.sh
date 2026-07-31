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

#
#   Fixture for regress/uri-unicode-fold.tst.ts. Build the name from explicit NFC bytes so the
#   fixture does not depend on how git or the file system stores a literal non-ASCII name.
#
CAFE=$(printf 'caf\303\251')
mkdir -p "web/${CAFE}"
echo "cafe-secret" > "web/${CAFE}/x.txt"
cp ../certs/self.key  ../certs/self.crt .
cp ../build/bin/cgitest* cgi-bin
# cgitest links against libgoahead; its rpath is @loader_path so the lib must sit beside it
if [ -f ../build/bin/libgoahead.dylib ] ; then
    cp ../build/bin/libgoahead.dylib cgi-bin/
elif [ -f ../build/bin/libgoahead.so ] ; then
    cp ../build/bin/libgoahead.so cgi-bin/
fi
