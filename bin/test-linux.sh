#!/bin/bash
#
#   test-linux.sh - Run the GoAhead unit test suite under Linux on a macOS host
#
#   Linux was previously reachable only through a CI round trip: push, wait, read a log. That is a
#   poor loop for a defect that only appears on Linux -- select, glibc, /proc, fork/exec for CGI --
#   and it is no loop at all for a change being iterated on. Apple's "container" tool runs a real
#   Linux VM locally, so the same build and the same suite CI runs can run here in about the time a
#   local build takes.
#
#   Two properties of the container are worth having for their own sake, beyond Linux coverage:
#
#   - The suite's ports (18080, 14443, 18090, 14453) are bound inside the container's own network
#     namespace, so a stray goahead-test on the host cannot collide with them and a stray
#     goahead-test in a container dies with the container.
#   - There is no macOS sandbox in a Linux VM, so the sandbox's file and socket restrictions cannot
#     colour the result.
#
#   ARCHITECTURE. Apple's container runs arm64 Linux natively. CI's ubuntu-latest runner is x86_64,
#   so the default gets Linux coverage, not CI parity -- most obviously, plain char is unsigned on
#   ARM and signed on x86, and the suite compiles its C tests with -Wsign-conversion. Treat a green
#   arm64 run as necessary, not sufficient. GOAHEAD_LINUX_ARCH=amd64 runs the x86_64 image under
#   Rosetta for parity; it is slower, and needs its own image and volume (GOAHEAD_LINUX_IMAGE and
#   GOAHEAD_LINUX_VOLUME) so it does not replace the arm64 ones.
#
#   This verifies the tree it is run from. Run it from the working tree while developing, and from
#   the unpacked release archive before a release: the archive is what a customer builds.
#
#   Usage:
#       bin/test-linux.sh                   build the image if needed, then run the whole suite
#       bin/test-linux.sh auth              pass anything else through to tm
#       bin/test-linux.sh regress/bad-path
#       bin/test-linux.sh --rebuild         force the image to be rebuilt first
#       bin/test-linux.sh --clean           discard the Linux build volume first
#       bin/test-linux.sh --asan [tests]    run against a sanitized build (make test-asan)
#       bin/test-linux.sh --stack=KB        limit the test server's stack to KB KiB (ulimit -s)
#       GOAHEAD_LINUX_CC=clang bin/test-linux.sh --asan    the same with clang, as in the CI clang leg
#       CFLAGS="-DME_GOAHEAD_CGI=0" bin/test-linux.sh --clean   a feature set, as in a CI features leg
#       GOAHEAD_LINUX_ARCH=amd64 GOAHEAD_LINUX_IMAGE=goahead-linux-test-amd64 \
#           GOAHEAD_LINUX_VOLUME=goahead-linux-build-amd64 bin/test-linux.sh    x86_64 under Rosetta
#
#   CFLAGS is passed to the build in the container. Objects in the build volume do not record the flags
#   they were built with, so start from --clean, or a GOAHEAD_LINUX_VOLUME of its own, when they change.
#
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${GOAHEAD_LINUX_IMAGE:-goahead-linux-test}"
VOLUME="${GOAHEAD_LINUX_VOLUME:-goahead-linux-build}"
DOCKERFILE="${ROOT}/test/utils/Dockerfile.linux"
BUN_VERSION="$(cat "${ROOT}/.bun-version")"
STAMP="${ROOT}/build/linux-image.sha"

ARCH="${GOAHEAD_LINUX_ARCH:-}"
CPUS="${GOAHEAD_LINUX_CPUS:-4}"
MEMORY="${GOAHEAD_LINUX_MEMORY:-4G}"

REBUILD=0
CLEAN=0
STACK=""
ARGS=()
for arg in "$@"; do
    case "$arg" in
        --rebuild) REBUILD=1 ;;
        --clean) CLEAN=1 ;;
        --stack=*) STACK="${arg#--stack=}" ;;
        *) ARGS+=("$arg") ;;
    esac
done

if ! command -v container >/dev/null 2>&1 ; then
    echo "      [Error] Apple's container tool is not installed" >&2
    echo "              See https://github.com/apple/container -- macOS 26 or later on Apple silicon" >&2
    exit 255
fi

#
#   container talks to a privileged helper over a Unix socket, so this and everything below it must
#   run outside a restrictive sandbox. "Operation not permitted" here means the sandbox, not the
#   daemon.
#
if ! container system status >/dev/null 2>&1 ; then
    echo "       [Run] container system start"
    container system start || exit 255
fi

# Rebuild the toolchain image when its Dockerfile or Bun version changes. Cleaning build/ removes the checksum.
WANT="$( (cat "${DOCKERFILE}" ; echo "${BUN_VERSION}") | shasum -a 256 | cut -d' ' -f1)"
HAVE=""
[ -f "${STAMP}" ] && HAVE="$(cat "${STAMP}")"

if ! container image inspect "${IMAGE}" >/dev/null 2>&1 ; then
    REBUILD=1
elif [ "${WANT}" != "${HAVE}" ] ; then
    REBUILD=1
fi

if [ ${REBUILD} -ne 0 ] ; then
    echo "       [Run] build ${IMAGE}"
    container build ${ARCH:+--arch "${ARCH}"} -t "${IMAGE}" --build-arg BUN_VERSION="${BUN_VERSION}" -f "${DOCKERFILE}" \
        "${ROOT}/test/utils" || exit 255
    mkdir -p "${ROOT}/build"
    echo "${WANT}" > "${STAMP}"
fi

#
#   The Linux build lives in a volume rather than in the host tree. build/obj is per platform but
#   build/bin is not, so a Linux build in the host tree would overwrite the macOS binaries and make
#   would then consider them up to date. Keeping the volume between runs is also what makes the
#   second run an incremental build, and what keeps node_modules installed.
#
if [ ${CLEAN} -ne 0 ] ; then
    echo "       [Run] delete volume ${VOLUME}"
    container volume delete "${VOLUME}" >/dev/null 2>&1
fi

if ! container volume inspect "${VOLUME}" >/dev/null 2>&1 ; then
    echo "       [Run] create volume ${VOLUME}"
    container volume create "${VOLUME}" >/dev/null || exit 255
fi

container run --rm \
    ${ARCH:+--arch "${ARCH}" --rosetta} \
    --cpus "${CPUS}" \
    --memory "${MEMORY}" \
    -e GOAHEAD_LINUX_CC="${GOAHEAD_LINUX_CC:-}" \
    ${CFLAGS:+-e CFLAGS="${CFLAGS}"} \
    -v "${ROOT}:/src:ro" \
    -v "${VOLUME}:/work" \
    -w /work \
    ${STACK:+-e GOAHEAD_TEST_STACK_KB="${STACK}"} \
    "${IMAGE}" \
    bash /src/test/utils/linux-test.sh ${ARGS[@]+"${ARGS[@]}"}
