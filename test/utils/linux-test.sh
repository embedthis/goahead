#!/bin/bash
#
#   linux-test.sh - Build and run the suite inside the Linux test container.
#
#   Invoked by bin/test-linux.sh; not useful on its own. Two mounts are in place:
#
#       /src    the host working tree, read only
#       /work   a container volume holding the Linux build
#
#   The tree is staged from /src into /work rather than built in place. build/obj is per platform
#   (build/obj/goahead-lib-macosx-debug) but build/bin is NOT -- the top-level Makefile sets
#   BIN := $(TOP)/build/bin unconditionally. A Linux build in the host tree would overwrite the
#   macOS binaries and leave make believing they were current; the host suite would then run ELF
#   binaries and fail in a way that looks nothing like its cause. The volume also survives between
#   runs, so the second build is incremental.
#
#   Arguments are passed to tm. With none, "test" runs everything under test/, which is what
#   "make test" does.
#
set -u

if [ ! -d /src ] || [ ! -d /work ] ; then
    echo "      [Error] linux-test.sh runs inside the test container only" >&2
    exit 255
fi

#
#   .testme must be excluded, not merely refreshed. TestMe caches each compiled .tst.c beside its
#   source and reuses it when it is newer than the source, so a host run leaves Mach-O binaries that
#   this container would happily decide were up to date and then fail to exec. GoAhead has C tests
#   under test/regress/, so this is not hypothetical.
#
#   node_modules is excluded for the same reason one layer up: a macOS install tree is not usable
#   here. The container installs its own into the volume, below.
#
#   .local.mk is not in the repository and is not part of anyone else's build. It is included by the
#   top-level Makefile when present and may call host-only tools. Staging it would also mean this
#   container builds something neither CI nor a customer does.
#
#   --delete-excluded, not just --delete: an excluded file is protected from deletion by default, so
#   anything staged by an earlier version of this script would live in the volume forever. The three
#   things the container owns and the host must never supply -- the Linux build, TestMe's compile
#   cache and the installed test dependencies -- are protected explicitly instead.
#
echo "       [Run] stage /src into /work"
rsync -a --delete --delete-excluded \
    --filter 'protect /build/' \
    --filter 'protect /node_modules/' \
    --filter 'protect .testme/' \
    --exclude '.git' \
    --exclude '.local.mk' \
    --exclude '.testme' \
    --exclude '.testme-pidfile' \
    --exclude '.DS_Store' \
    --exclude 'build' \
    --exclude 'node_modules' \
    /src/ /work/ || exit 255

cd /work || exit 255

# Install dependencies beside the sources. The volume retains node_modules, but the container loses ~/.bun/bin,
# so run TestMe's installer again when dependencies are already current.
if [ ! -d /work/node_modules ] ; then
    echo "       [Run] bun install"
fi
bun install --frozen-lockfile || exit 255
if ! command -v tm >/dev/null 2>&1 ; then
    echo "       [Run] install tm from node_modules/@embedthis/testme"
    (cd node_modules/@embedthis/testme && bun bin/install.mjs) || exit 255
fi
echo "      [Info] bun $(bun --version), tm $(tm --version 2>/dev/null)"

if [ "${1:-}" = "--asan" ] ; then
    shift
    echo "       [Run] make test-asan [linux]"
    exec make test-asan ${GOAHEAD_LINUX_CC:+CC="$GOAHEAD_LINUX_CC"} ARGS="$*"
fi

echo "       [Run] make build [linux]"
make build || exit 255

#
#   Report what was actually exercised. The image is arm64 and CI's ubuntu runner is x86_64, so this
#   line is the difference between the two results.
#
echo "      [Info] $(uname -s) $(uname -m), gcc $(gcc -dumpversion 2>/dev/null), $(openssl version 2>/dev/null)"

# Use make test to detect server crashes and sanitizer reports after the last test.
echo "       [Run] make test ${*}"
exec make test ARGS="$*"
