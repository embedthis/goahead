/* A header write to a broken connection must not leak its value */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "../regress/heap-fixture.h"

#if ME_UNIX_LIKE && HEAP_MEASURABLE

#define WRITES      1000
#define RUNS        2
#define HEADROOM    16

//  A leak orphans one value per write, about 2 MB per run
#define MAX_GROWTH  (256 * 1024)

static char value[ME_GOAHEAD_LIMIT_BUFFER * 2];

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int writeToBrokenConnection(Webs *wp)
{
    wp->state = WEBS_RUNNING;
    bufFlush(&wp->output);
    bufPutBlk(&wp->output, value, bufRoom(&wp->output) - HEADROOM);
    return websWriteHeader(wp, "X-Reset", "%s", value);
}


static void campaign(void *arg)
{
    int i;

    for (i = 0; i < WRITES; i++) {
        writeToBrokenConnection(arg);
    }
}


int main(int argc, char **argv)
{
    WebsSocket *sp;
    Webs       *wp;
    ssize      growth;
    int        fds[2], sid, wid, rc;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    signal(SIGPIPE, SIG_IGN);
    memset(value, 'v', sizeof(value) - 1);

    teq(socketOpen(), 0, "open the socket layer");
    ttrue(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "create a socket pair");
    close(fds[1]);
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    sp = socketPtr(sid);
    sp->sock = fds[0];
    wid = websAlloc(sid);
    wp = webs[wid];

    rc = writeToBrokenConnection(wp);
    teq(rc, -1, "a header whose flush fails reports the failure");
    teq(wp->state, WEBS_COMPLETE, "and completes the request");

    growth = heapGrowth(campaign, wp, RUNS);
    ttrue(growth < MAX_GROWTH, "%d failed header writes grew the heap by %zd bytes", WRITES, growth);

    websFree(wp);
    socketClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("The heap in use cannot be measured on this platform");
    return 0;
}
#endif
