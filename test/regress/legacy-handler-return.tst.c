/* A legacy URL handler returns 1 when it serves the request and 0 to decline, so routing continues */

#define ME_GOAHEAD_LEGACY 1

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
/*
    The library's handlers register with the library's own route table. This test routes through its own copy of
    route.c, so it opens none of them.
 */
#define websCgiOpen() 0
#define websOptionsOpen() 0
#define websActionOpen()
#define websFileOpen()
#define websUploadOpen()
#define websJstOpen() 0
#define websOpenAuth(minimal) 0
#include "../../src/http.c"
#include "../../src/route.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int declineCalls;
static int serveCalls;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int declineHandler(Webs *wp, char *prefix, char *dir, int flags)
{
    declineCalls++;
    return 0;
}

static int serveHandler(Webs *wp, char *prefix, char *dir, int flags)
{
    serveCalls++;
    return 1;
}

int main(int argc, char **argv)
{
    Webs *wp;
    int  wid;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    //  Each legacy handler is inserted before those already defined
    teq(websUrlHandlerDefine("/", 0, 0, serveHandler, 0), 0, "define the serving legacy handler");
    teq(websUrlHandlerDefine("/decline/", 0, 0, declineHandler, 0), 0, "define the declining legacy handler");

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->protocol = sclone("http");
    wp->url = sclone("/decline/x");
    wp->path = sclone("/decline/x");
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "127.0.0.1");

    websRouteRequest(wp);
    ttrue(wp->route && smatch(wp->route->prefix, "/decline/"), "the declining handler's route is selected first");
    ttrue(!websRunRequest(wp), "a handler that returns 0 has declined the request");
    teq(declineCalls, 1, "the declining handler ran");

    websRouteRequest(wp);
    ttrue(wp->route && smatch(wp->route->prefix, "/"), "routing resumes at the next route");
    ttrue(websRunRequest(wp), "a handler that returns 1 has served the request");
    teq(serveCalls, 1, "the serving handler ran");

    websFree(wp);
    websClose();
    return 0;
}
