/*
    secure-only.tst.c - ME_GOAHEAD_SECURE_ONLY redirects plain HTTP before any route is consulted
 */

#define ME_GOAHEAD_SECURE_ONLY 1

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
#include "../../src/route.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static bool serveHandler(Webs *wp)
{
    return 1;
}


static Webs *request(bool secure)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->protocol = sclone(secure ? "https" : "http");
    wp->url = sclone("/private/page.html");
    wp->path = sclone("/private/page.html");
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "127.0.0.1");
    if (secure) {
        wp->flags |= WEBS_SECURE | WEBS_HTTPS;
    }
    websRouteRequest(wp);
    return wp;
}


int main(int argc, char **argv)
{
    Webs *wp;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    ttrue(websOpenRoute() == 0, "open routes");
    ttrue(websDefineHandler("serve", 0, serveHandler, 0, 0) == 0, "define a handler");
    ttrue(websAddRoute("/", "serve", -1) != 0, "add a catch-all route");

    wp = request(0);
    ttrue(wp->route == 0, "a plain HTTP request must not be routed");
    ttrue(wp->code == HTTP_CODE_MOVED_TEMPORARILY && wp->finalized,
          "a plain HTTP request must be answered with a redirect, got %d", wp->code);
    websFree(wp);

    wp = request(1);
    ttrue(wp->route != 0, "an HTTPS request is routed");
    websFree(wp);
    websCloseRoute();
    return 0;
}
