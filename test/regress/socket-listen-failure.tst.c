/* A listen whose address cannot be resolved frees its socket slot */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

int main(int argc, char **argv)
{
    int sid;
#if ME_UNIX_LIKE
    int rc;
#endif

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(socketOpen(), 0, "open the socket layer");

    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    ttrue(sid >= 0 && socketPtr(sid)->sock == (Socket) -1, "a new socket holds no descriptor");
    socketFree(sid);

#if ME_UNIX_LIKE
    //  Windows resolves a port of -1, so there the resolution failure cannot be reached this way
    rc = socketListen("127.0.0.1", -1, NULL, 0);
    teq(rc, -1, "a listen whose address cannot be resolved fails");
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    teq(sid, 0, "and leaves no slot behind");
    socketFree(sid);
#endif

    socketClose();
    return 0;
}
