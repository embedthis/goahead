#!/bin/bash
#
#   buildLib.sh -- Build the GoAhead distribution files
#
#   Creates:
#     dist/goaheadLib.c      Amalgamated library source
#     dist/goahead.h         Main header
#     dist/js.h              JavaScript header
#     build/goahead-VERSION-src.tgz   Source distribution tarball
#
#   Usage: buildLib.sh [--dist-only]
#
#     --dist-only   Generate the dist/ files and stop; no source tarball.
#
#   The DIST environment variable overrides the output directory for the
#   amalgamation (default $TOP/dist). "make check-dist" uses DIST plus
#   --dist-only to regenerate into a scratch directory and diff it against
#   the committed dist/.
#

set -e

#
#   A set CDPATH makes cd echo the directory it lands in, which would be
#   captured by every $(cd ...) below and injected into the tar pipes.
#
unset CDPATH

TOP=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$TOP/build"
SRC="$TOP/src"
DIST="${DIST:-$TOP/dist}"
DIST_ONLY=0

if [ "$1" = "--dist-only" ] ; then
    DIST_ONLY=1
fi

# ---------------------------------------------------------------
#  Read version from package.json
# ---------------------------------------------------------------
VERSION=$(grep '"version"' "$TOP/package.json" | head -1 | sed 's/.*"\([0-9][0-9.]*\)".*/\1/')
VERSION=${VERSION:-1.0.0}

NAME="goahead-${VERSION}"
STAGE="$BUILD/stage/$NAME"
OUTPUT="$BUILD/${NAME}-src.tgz"

# ---------------------------------------------------------------
#  dist/ -- amalgamated library source and headers
# ---------------------------------------------------------------
mkdir -p "$DIST"

DST="$DIST/goaheadLib.c"
FILES="action.c alloc.c auth.c cgi.c crypt.c file.c fs.c http.c js.c jst.c \
       mbedtls.c openssl.c options.c osdep.c rom.c route.c runtime.c socket.c \
       time.c upload.c"

cat > "${DST}" << 'ENDOFFILE'
/*
    goaheadLib.c -- GoAhead Library Source

    This file is a catenation of all the source code. Amalgamating into a
    single file makes embedding simpler and the resulting application faster,
    by using compiler optimization within the GoAhead library.

    Prepared by: buildLib.sh
 */

#include "goahead.h"

#if ME_COM_GOAHEAD
ENDOFFILE

for f in ${FILES}; do
    printf '\n\n/********* Start of file src/%s ************/\n\n' "${f}" >> "${DST}"
    sed -e '/#include "goahead.h"/d' -e '/#include "js.h"/d' "${SRC}/${f}" >> "${DST}"
done

printf '\n#else\nvoid dummyGoahead(){}\n#endif /* ME_COM_GOAHEAD */\n' >> "${DST}"

echo "      [Info] Created dist/goaheadLib.c: $(wc -l < "${DST}") lines"

cp "$SRC/goahead.h" "$DIST/goahead.h"
cp "$SRC/js.h" "$DIST/js.h"
echo "      [Info] Copied dist/goahead.h, dist/js.h"

# ---------------------------------------------------------------
#  dist/ -- README only (no CLAUDE.md, AI/, or .claude/)
# ---------------------------------------------------------------
cp "$TOP/README.md" "$DIST/README.md"

if [ "$DIST_ONLY" = 1 ] ; then
    exit 0
fi

# ---------------------------------------------------------------
#  Source distribution tarball
# ---------------------------------------------------------------
echo "      [Info] Building source distribution for GoAhead ${VERSION}"

rm -fr "$BUILD/stage"
mkdir -p "$STAGE"

#
#   Root files (CLAUDE.md and .claude/ are intentionally excluded)
#
for f in README.md CONTRIBUTING.md EVAL.md LICENSE.md Makefile make.bat package.json; do
    [ -f "$TOP/$f" ] && cp "$TOP/$f" "$STAGE/"
done

#
#   src/ - full source tree (CLAUDE.md, .claude/, AI/, doc/ are excluded)
#
mkdir -p "$STAGE/src"
(cd "$TOP" && tar cf - \
    --exclude='CLAUDE.md' \
    --exclude='.claude' \
    --exclude='AI' \
    --exclude='doc' \
    --exclude='.DS_Store' \
    src/) | (cd "$STAGE" && tar xf -)

#
#   projects/ - premake5 and generated build files
#
mkdir -p "$STAGE/projects"
for f in premake5.lua windows.bat openssl-prep.bat; do
    [ -f "$TOP/projects/$f" ] && cp "$TOP/projects/$f" "$STAGE/projects/"
done
for dir in gmake2 vs2022 xcode; do
    [ -d "$TOP/projects/$dir" ] && cp -r "$TOP/projects/$dir" "$STAGE/projects/"
done

#
#   certs/
#
[ -d "$TOP/certs" ] && cp -r "$TOP/certs" "$STAGE/certs"

#
#   doc/ -- only the API and man pages are shipped
#
if [ -d "$TOP/doc/api" ]; then
    mkdir -p "$STAGE/doc"
    cp -r "$TOP/doc/api" "$STAGE/doc/api"
    cp -r "$TOP/doc/man" "$STAGE/doc/man"
fi

#
#   test/ - only files tracked in git (excludes runtime artifacts,
#   local config, untracked logs, etc.).
#
mkdir -p "$STAGE/test"
(cd "$TOP" && git ls-files -z test/ ':(exclude)test/sec/**' | tar --null -T - -cf -) | (cd "$STAGE" && tar xf -)

#
#   dist/ - amalgamated source for pak consumers
#
cp -r "$TOP/dist" "$STAGE/dist"

#
#   bin/buildLib.sh - script to regenerate dist/ amalgamation
#
mkdir -p "$STAGE/bin"
cp "$TOP/bin/buildLib.sh" "$STAGE/bin/buildLib.sh"
chmod +x "$STAGE/bin/buildLib.sh"

#
#   Clean up: belt-and-braces removal of internal-only and runtime/system
#   droppings in case a copy step (e.g. the cp -r of certs/) pulls them in.
#   CLAUDE.md, .claude/, and AI/ are internal AI-guidance artifacts and must
#   never ship in the public distribution.
#
find "$STAGE" -name ".DS_Store" -delete 2>/dev/null || true
find "$STAGE" -name "CLAUDE.md" -delete 2>/dev/null || true
find "$STAGE" -depth \( -name ".claude" -o -name "AI" \) -type d -exec rm -rf {} + 2>/dev/null || true
for d in test/web/tmp test/tmp test/cgi-bin; do
    if [ -d "$STAGE/$d" ]; then
        find "$STAGE/$d" -mindepth 1 ! -name .keep -delete 2>/dev/null || true
    fi
done

#
#   Create tarball
#
echo "      [Pack] build/${NAME}-src.tgz"
mkdir -p "$BUILD"
tar -czf "$OUTPUT" -C "$BUILD/stage" "$NAME"

cp "$OUTPUT" "$BUILD/goahead-src.tgz"

COUNT=$(tar -tzf "$OUTPUT" | wc -l | tr -d ' ')
SIZE=$(ls -lh "$OUTPUT" | awk '{print $5}')
echo "      [Done] build/goahead-src.tgz ($COUNT files, $SIZE)"

rm -fr "$BUILD/stage"
