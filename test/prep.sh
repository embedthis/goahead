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

#
#   Fixture for regress/file-type-gate.tst.ts. A FIFO with no writer blocks open() in the kernel,
#   which would stall the single event loop. Not available on Windows; that test axis is skipped there.
#
if [ "$TESTME_OS" != "windows" ] ; then
    rm -f web/fifo.txt
    mkfifo web/fifo.txt
fi
cp ../certs/self.key  ../certs/self.crt .
cp ../build/bin/cgitest* cgi-bin
# cgitest links against libgoahead; its rpath is @loader_path so the lib must sit beside it
if [ -f ../build/bin/libgoahead.dylib ] ; then
    cp ../build/bin/libgoahead.dylib cgi-bin/
elif [ -f ../build/bin/libgoahead.so ] ; then
    cp ../build/bin/libgoahead.so cgi-bin/
fi

#
#   Fixtures for regress/cgi-bat-gate.tst.ts. probe.bat carries the reporter's payload so a shell
#   launch would be visible in the response body; cgitest.batch.exe is the near-miss name that must
#   still launch. Made executable so the POSIX gate admits them and the 404 can only come from the
#   extension gate under test.
#
cat > cgi-bin/probe.bat <<'ENDOFBAT'
@echo off
echo bat-marker-launched
whoami
ENDOFBAT
chmod +x cgi-bin/probe.bat
cp "../build/bin/cgitest${EXE}" "cgi-bin/cgitest.batch.exe"
