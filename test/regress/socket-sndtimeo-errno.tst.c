/* Only a stack that lacks SO_SNDTIMEO earns the "no send timeout" warning; a dead connection does not. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if ME_UNIX_LIKE && ME_GOAHEAD_LOGGING
static int failWith;

static int fakeSetsockopt(int sock, int level, int option, const void *value, socklen_t len)
{
    if (failWith) {
        errno = failWith;
        return -1;
    }
    return 0;
}
#define setsockopt fakeSetsockopt
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/socket.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef setsockopt

static int warnings;

static void countWarnings(int level, cchar *msg)
{
    if (msg && strstr(msg, "no send timeout")) {
        warnings++;
    }
}


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
    WebsLogHandler prior;
    int            sid;

    sid = openSocket();
    ttrue(sid >= 0, "open a socket");
    prior = logSetHandler(countWarnings);

    failWith = 0;
    teq(socketSetSendTimeout(sid, 1000), 0, "a stack that accepts the option reports success");

    failWith = EINVAL;
    teq(socketSetSendTimeout(sid, 1000), -1, "a failure on a dead connection is reported to the caller");
    teq(warnings, 0, "a dead connection does not accuse the network stack of lacking the option");

    failWith = ENOPROTOOPT;
    teq(socketSetSendTimeout(sid, 1000), -1, "a stack without the option reports failure");
    teq(warnings, 1, "a stack without the option is warned about once");
    socketSetSendTimeout(sid, 1000);
    teq(warnings, 1, "the warning is not repeated");

    logSetHandler(prior);
    socketFree(sid);
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX with logging only");
    return 0;
}
#endif
