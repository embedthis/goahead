/*
    The whole-request deadline closes active requests even when client activity renews the idle timeout.
 */

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
    A request that began age seconds ago, is writing its response and heard from the client just now
 */
static int responding(int age)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->path = sclone("/slow");
    wp->started = websTime() - age;
    wp->timestamp = websTime();
    wp->state = WEBS_RUNNING;
    wp->flags |= WEBS_HEADERS_CREATED;
    wp->timeout = websStartEvent(WEBS_TIMEOUT, checkTimeout, wp);
    checkTimeout(wp, wp->timeout);
    return wid;
}

//  Freeing the last request may release the request table itself
static bool isOpen(int wid)
{
    return webs && wid < websMax && webs[wid] != NULL;
}

int main(int argc, char **argv)
{
    int wid;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");

#if ME_GOAHEAD_LIMIT_RESPONSE_TIMEOUT > 0
    wid = responding(ME_GOAHEAD_LIMIT_RESPONSE_TIMEOUT - 10);
    ttrue(isOpen(wid), "an active request within the deadline is kept");
    websFree(webs[wid]);

    wid = responding(ME_GOAHEAD_LIMIT_RESPONSE_TIMEOUT + 1);
    ttrue(!isOpen(wid), "an active request past the deadline is closed");
#else
    wid = responding(100000);
    ttrue(isOpen(wid), "with the deadline disabled an active request is kept");
    websFree(webs[wid]);
#endif

    websClose();
    return 0;
}
