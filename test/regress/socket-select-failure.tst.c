/*
    socket-select-failure.tst.c - A failed select() must not be reported as socket readiness
 */
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if !ME_WIN_LIKE
#include <sys/resource.h>
#include <sys/time.h>

static int injectError;

static int fakeSelect(int nfds, fd_set *readFds, fd_set *writeFds, fd_set *exceptFds, struct timeval *tv)
{
    if (injectError) {
        //  A failed select() leaves the sets as the caller passed them
        errno = injectError;
        injectError = 0;
        return -1;
    }
    return select(nfds, readFds, writeFds, exceptFds, tv);
}
#define select fakeSelect
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/socket.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef select

static WebsSocket sock0, *list[1];

static double nowMsec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void watch(int fd)
{
    memset(&sock0, 0, sizeof(sock0));
    sock0.sock = fd;
    sock0.handlerMask = SOCKET_READABLE | SOCKET_WRITABLE;
    list[0] = &sock0;
    socketList = list;
    socketMax = 1;
    socketHighestFd = fd;
}

static void testInterruptedSelect(void)
{
    int pair[2], rc;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0) {
        tfail("Cannot create socket pair");
        return;
    }
    //  Watch for readable only: nothing is pending, so nothing must be reported
    watch(pair[0]);
    sock0.handlerMask = SOCKET_READABLE;
    injectError = EINTR;
    rc = socketSelect(-1, 0);
    ttrue(rc <= 0, "an interrupted select reports no events");
    teq(sock0.currentEvents, 0, "an interrupted select must not flag an idle socket readable");

    //  A socket due a reservice is still reported when select fails
    sock0.currentEvents = 0;
    sock0.flags |= SOCKET_RESERVICE;
    injectError = EINTR;
    rc = socketSelect(-1, 0);
    teq(rc, 1, "a reservice is still reported when select fails");
    teq(sock0.currentEvents, SOCKET_READABLE, "the reserviced socket is readable");

    close(pair[0]);
    close(pair[1]);
}

static void testDescriptorAboveSetSize(void)
{
    struct rlimit r;
    double        start, elapsed;
    int           pair[2], fd, rc;

    getrlimit(RLIMIT_NOFILE, &r);
    if (r.rlim_max != RLIM_INFINITY && r.rlim_max < FD_SETSIZE + 64) {
        tskip("descriptor limit too low to place a socket above FD_SETSIZE");
        return;
    }
    if (r.rlim_cur == RLIM_INFINITY || r.rlim_cur < FD_SETSIZE + 64) {
        r.rlim_cur = FD_SETSIZE + 64;
        if (setrlimit(RLIMIT_NOFILE, &r) < 0) {
            tskip("cannot raise the descriptor limit");
            return;
        }
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0) {
        tfail("Cannot create socket pair");
        return;
    }
    fd = FD_SETSIZE + 10;
    if (dup2(pair[0], fd) < 0) {
        tfail("Cannot place a socket above FD_SETSIZE");
        return;
    }
    watch(fd);
    sock0.handlerMask = SOCKET_READABLE;

    start = nowMsec();
    rc = socketSelect(-1, 300);
    elapsed = nowMsec() - start;
    ttrue(rc <= 0, "no event for an idle socket above FD_SETSIZE");
    teq(sock0.currentEvents, 0, "an idle socket above FD_SETSIZE must not be flagged readable");
    ttrue(elapsed >= 200, "socketSelect must wait for a socket above FD_SETSIZE, not fail at once");

    ttrue(write(pair[1], "x", 1) == 1, "write to the peer");
    sock0.currentEvents = 0;
    rc = socketSelect(-1, 1000);
    teq(rc, 1, "one event once the socket above FD_SETSIZE is readable");
    teq(sock0.currentEvents, SOCKET_READABLE, "the socket above FD_SETSIZE is reported readable");

    close(fd);
    close(pair[0]);
    close(pair[1]);
}

int main(int argc, char **argv)
{
    testInterruptedSelect();
    testDescriptorAboveSetSize();
    socketList = NULL;
    socketMax = 0;
    return 0;
}
#else
int main(int argc, char **argv) { tskip("POSIX select failure injection"); return 0; }
#endif
