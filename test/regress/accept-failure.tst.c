/* A connection websAccept cannot start leaves its socket for socketAccept to free */

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

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

int main(int argc, char **argv)
{
    WebsSocket *sp;
    int        fds[2], sid, rc;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(socketOpen(), 0, "open the socket layer");
    ttrue(pipe(fds) == 0, "create a pipe");
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    sp = socketPtr(sid);
    sp->sock = fds[0];

    rc = websAccept(sid, "127.0.0.1", 40000, 0);
    teq(rc, -1, "a connection whose address cannot be read is refused");
    ttrue(socketPtr(sid) == sp, "and its socket is left for the caller to free");
    if (socketPtr(sid) == sp) {
        socketFree(sid);
    }
    close(fds[1]);
    socketClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("A pipe that is not a socket is needed");
    return 0;
}
#endif
