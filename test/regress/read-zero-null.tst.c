/* A read event restores the receive terminator even when no bytes arrive. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
static ssize noRead(int sid, void *buf, ssize size) { return 0; }
static bool poisonGrow(WebsBuf *bp, ssize room)
{
    bool rc = bufGrow(bp, room);
    if (rc) *bp->endp = (char) 0xA5;
    return rc;
}
static WebsSocket stubSocket;
static WebsSocket *testSocketPtr(int sid) { return &stubSocket; }
static void testHandler(int sid, int mask, SocketHandler handler, void *arg) {}
#define socketPtr testSocketPtr
#define socketCreateHandler testHandler
#define socketRead noRead
#define bufGrow poisonGrow
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef socketPtr
#undef socketCreateHandler
#undef socketRead
#undef bufGrow
int main(int argc, char **argv)
{
    Webs *wp;
    int wid = websAlloc(-1);
    ttrue(wid >= 0, "allocate request");
    wp = webs[wid];
    bufFree(&wp->rxbuf);
    bufCreate(&wp->rxbuf, 16, -1);
    bufPutStr(&wp->rxbuf, "GET /");
    bufAddNull(&wp->rxbuf);
    readEvent(wp);
    teq(*wp->rxbuf.endp, 0, "zero-byte read restores terminator");
    tmatch((char*) wp->rxbuf.servp, "GET /", "partial request preserved");
    websFree(wp);
    return 0;
}
