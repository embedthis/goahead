/*
    request-deadline.tst.c - A request must arrive in full within ME_GOAHEAD_LIMIT_REQUEST_TIMEOUT
 */

#define ME_GOAHEAD_LIMIT_REQUEST_TIMEOUT 10

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

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    Refresh idle time while testing the fixed request deadline.
 */
static bool timedOut(int state, int secondsAgo)
{
    Webs *wp;
    int  wid, id;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->finalized = 1;
    wp->state = state;
    wp->started = time(0) - secondsAgo;
    websNoteRequestActivity(wp);
    id = websStartEvent(60 * 1000, checkTimeout, wp);
    checkTimeout(wp, id);
    if (webs && wid < websMax && webs[wid] == wp) {
        websStopEvent(id);
        websFree(wp);
        return 0;
    }
    return 1;
}


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    tinfo("\n--- Suite A: past the deadline ---\n");
    ttrue(timedOut(WEBS_CONTENT, 12), "a body still arriving past the request deadline must be timed out");

    tinfo("\n--- Suite B: within the deadline ---\n");
    ttrue(!timedOut(WEBS_CONTENT, 3), "a body within the request deadline must be left alone");

    tinfo("\n--- Suite C: request received ---\n");
    ttrue(!timedOut(WEBS_RUNNING, 30), "a request already received is not subject to the request deadline");
    return 0;
}
