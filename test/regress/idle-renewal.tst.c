/* Only a read that delivers request bytes renews the idle mark and starts the request clock. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
static cchar *readData;
static ssize fakeRead(int sid, void *buf, ssize size)
{
    ssize len;

    if (!readData) {
        return 0;
    }
    len = (ssize) slen(readData);
    memcpy(buf, readData, (size_t) len);
    readData = 0;
    return len;
}
static WebsSocket stubSocket;
static WebsSocket *testSocketPtr(int sid) { return &stubSocket; }
static void testHandler(int sid, int mask, SocketHandler handler, void *arg) {}
#define socketPtr testSocketPtr
#define socketCreateHandler testHandler
#define socketRead fakeRead
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
int main(int argc, char **argv)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    ttrue(wid >= 0, "allocate request");
    wp = webs[wid];
    wp->timestamp = 1000;
    wp->started = 0;

    readData = 0;
    readEvent(wp);
    teq(wp->timestamp, 1000, "read event with no request data leaves the idle mark");
    teq(wp->started, 0, "read event with no request data does not start the request clock");

    readData = "G";
    readEvent(wp);
    ttrue(wp->timestamp > 1000, "request data renews the idle mark");
    ttrue(wp->started != 0, "request data starts the request clock");
    websFree(wp);
    return 0;
}
