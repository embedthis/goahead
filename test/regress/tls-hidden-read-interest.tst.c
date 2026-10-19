/*
    Buffered TLS input must not add socket interest beyond the registered mask.
    Check no-handler sockets, immediate service with read interest, and clearing the buffered mark.
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
#include <sys/time.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/socket.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int handlerCalls;

static double nowMsec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void handler(int sid, int mask, void *data)
{
    handlerCalls++;
}

int main(int argc, char **argv)
{
    WebsSocket *sp;
    double     start, elapsed;
    char       byte;
    int        pair[2], sid, rc;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0) {
        tfail("Cannot create socket pair");
        return 1;
    }
    socketOpen();
    sid = socketAlloc(NULL, 0, NULL, 0);
    ttrue(sid >= 0, "allocate a socket");
    sp = socketList[sid];
    sp->sock = pair[0];
    socketHighestFd = max(socketHighestFd, sp->sock);
    socketSetBlock(sid, 0);

    /*
        No handler: buffered input and a readable descriptor must not produce events.
     */
    socketCreateHandler(sid, SOCKET_READABLE, handler, NULL);
    socketHiddenData(sp, 100, SOCKET_READABLE);
    socketDeleteHandler(sid);
    teq(sp->handlerMask, 0, "a deleted handler leaves no interest, whatever the application layer holds");
    ttrue(write(pair[1], "x", 1) == 1, "write one byte to the peer");
    sp->currentEvents = 0;
    start = nowMsec();
    rc = socketSelect(-1, 300);
    elapsed = nowMsec() - start;
    teq(rc, 0, "a socket with no handler reports no events");
    teq(sp->currentEvents, 0, "a socket with no handler is not flagged readable");
    ttrue(elapsed >= 200, "socketSelect must wait out its timeout rather than return at once");
    socketProcess();
    teq(handlerCalls, 0, "no handler runs");

    /*
        Read interest: buffered input must be serviced even when the descriptor is idle.
     */
    ttrue(read(pair[0], &byte, 1) == 1, "drain the byte so the descriptor is idle");
    sp->currentEvents = 0;
    socketCreateHandler(sid, SOCKET_READABLE, handler, NULL);
    teq(sp->handlerMask, SOCKET_READABLE, "the registered interest is exactly the mask given");
    start = nowMsec();
    rc = socketSelect(-1, 1000);
    elapsed = nowMsec() - start;
    teq(rc, 1, "registering interest in a direction with hidden data reports one event");
    teq(sp->currentEvents & SOCKET_READABLE, SOCKET_READABLE, "the hidden data is reported as readable");
    ttrue(elapsed < 500, "the hidden data is serviced at once, not after the descriptor timeout");
    socketProcess();
    teq(handlerCalls, 1, "the handler runs once for the hidden data");

    /*
        Clearing buffered input must restore normal descriptor readiness checks.
     */
    socketHiddenData(sp, 0, SOCKET_READABLE);
    sp->currentEvents = 0;
    socketCreateHandler(sid, SOCKET_READABLE, handler, NULL);
    start = nowMsec();
    rc = socketSelect(-1, 300);
    elapsed = nowMsec() - start;
    teq(rc, 0, "no hidden data and an idle descriptor report nothing");
    ttrue(elapsed >= 200, "a cleared mark does not reservice the socket");

    socketFree(sid);
    close(pair[1]);
    socketClose();
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX socketpair");
    return 0;
}
#endif
