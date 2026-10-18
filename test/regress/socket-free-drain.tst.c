/* Closing a socket discards a bounded amount of unread input, however fast the peer keeps sending. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_UNIX_LIKE
static int  recvCalls;
static bool flood;

static ssize fakeRecv(Socket sock, void *buf, size_t len, int flags)
{
    recvCalls++;
    return flood ? (ssize) len : 0;
}
#define recv fakeRecv
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/socket.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef recv

#ifndef SOCKET_DRAIN_LIMIT
    #define SOCKET_DRAIN_LIMIT (ME_GOAHEAD_LIMIT_BUFFER * 64)
#endif

static int openSocket(void)
{
    int pair[2], sid;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0) {
        return -1;
    }
    close(pair[1]);
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    socketList[sid]->sock = pair[0];
    return sid;
}


int main(int argc, char **argv)
{
    int sid, bound;

    bound = 2 * (SOCKET_DRAIN_LIMIT / SOCKET_DRAIN_CHUNK + 1);

    sid = openSocket();
    ttrue(sid >= 0, "open a socket");
    flood = 1;
    recvCalls = 0;
    alarm(5);
    socketFree(sid);
    alarm(0);
    ttrue(recvCalls <= bound, "socketFree reads at most the drain limit from a flooding peer");

    sid = openSocket();
    ttrue(sid >= 0, "open a lingering socket");
    recvCalls = 0;
    alarm(5);
    lingerRead(sid, SOCKET_READABLE, NULL);
    alarm(0);
    ttrue(recvCalls <= bound / 2, "lingerRead reads at most the drain limit per event");
    ttrue(socketList[sid] != NULL, "lingerRead keeps a socket whose peer is still sending");

    flood = 0;
    lingerRead(sid, SOCKET_READABLE, NULL);
    ttrue(sid >= socketMax || socketList[sid] == NULL, "lingerRead frees the socket once the peer closes");
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX only");
    return 0;
}
#endif
