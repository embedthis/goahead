#!/bin/bash
#
#   prep.sh - TestMe setup script to start web
#
BIN=$(cd "$(dirname "${BIN}")" && pwd)/$(basename "${BIN}")

#
#   Diagnostics. testme reports only "Prep script completed successfully", so when staging goes
#   wrong the first visible symptom is a wall of unrelated test failures. Record what actually
#   happened to a file the CI job can print; also echo it, for when prep output is shown.
#
#   Written outside test/ and not named *.log: cleanup.sh does "rm -f *.log" and "rm -f cgi-bin/*"
#   after the run, so a diagnostic left in test/ is destroyed before anything can read it.
DIAG="../prep-diag.txt"
: > "$DIAG"
diag() {
    echo "$@" | tee -a "$DIAG"
}

diag "prep: TESTME_OS='${TESTME_OS}' uname='$(uname -s 2>/dev/null)'"

if [ "$TESTME_OS" = "windows" ] ; then
    EXE=".exe"
    diag "prep: taking the windows branch, running prep-test.bat"
    (cd .. ; test/utils/prep-test.bat)
    diag "prep: prep-test.bat exit=$?"
else
    EXE=""
    diag "prep: taking the posix branch, running prep-test.sh"
    (cd .. ; test/utils/prep-test.sh)
    diag "prep: prep-test.sh exit=$?"
fi
diag "prep: EXE='${EXE}'"

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
# Generate local test keys when absent; distributions contain certificates only.
if [ -f ../certs/self.key ] ; then
    cp ../certs/self.key  ../certs/self.crt .
elif [ ! -f self.key ] ; then
    echo "      [Run] generate a self-signed test certificate"
    openssl req -x509 -newkey rsa:2048 -nodes -keyout self.key -out self.crt -days 3650 \
        -subj "/CN=localhost" >/dev/null 2>&1 || {
        echo "      [Warn] cannot generate a test certificate - openssl is required for the TLS tests" >&2
    }
fi
cp ../build/bin/cgitest* cgi-bin
#
#   cgitest links against libgoahead, so the library must sit beside it: the rpath is @loader_path
#   on macOS, $ORIGIN on Linux, and on Windows the loader searches the executable's own directory.
#
#   Windows needs this more than the others, not less. The CGI child is spawned with a constructed
#   environment and PATH is in cgiHandler's envBlackList, so the child cannot fall back to finding
#   libgoahead.dll anywhere else. Without the .dll here cgitest.exe fails to start, no CGI runs at
#   all, and every CGI test fails - while the negative assertions in regress/cgi-bat-gate.tst.ts
#   pass vacuously, because nothing executing is indistinguishable from the gate refusing to.
#
#   On Windows every DLL must come, not just libgoahead.dll: that library itself imports
#   libssl-3-x64.dll and libcrypto-3-x64.dll, and with PATH stripped the loader has nowhere else to
#   find them. Copying libgoahead.dll alone leaves it unloadable and cgitest.exe still cannot start.
#
for lib in ../build/bin/libgoahead.dylib ../build/bin/libgoahead.so ../build/bin/*.dll ; do
    [ -f "$lib" ] && cp "$lib" cgi-bin/
done

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
diag "prep: cgitest.batch.exe copy exit=$? (source ../build/bin/cgitest${EXE})"

diag "prep: build/bin contains:"
ls -l ../build/bin 2>&1 | tee -a "$DIAG"
diag "prep: cgi-bin contains:"
ls -l cgi-bin 2>&1 | tee -a "$DIAG"
