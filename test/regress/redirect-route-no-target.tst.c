/*
    redirect-route-no-target.tst.c - A redirect route without a redirect= target answers with a status-0 response.
    The redirect handler looks up the route's default target with websRedirectByStatus(wp, 0); when the route has no
    redirects at all the lookup falls through to websError(wp, 0), which writes "HTTP/1.1 0" and finalizes the
    request. A status of zero means "no target" to the handler and must report no match, so the request goes on to
    the next route, and the loader must refuse a redirect route that names no default target.
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

#define ROUTES "redirect-route-no-target.tmp"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static bool serveMatch(Webs *wp)
{
    return 1;
}


static Webs *request(cchar *path)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->protocol = sclone("http");
    wp->url = sclone(path);
    wp->path = sclone(path);
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "127.0.0.1");
    websRouteRequest(wp);
    return wp;
}


static int load(cchar *table)
{
    FILE *fp;
    int  rc;

    websCloseRoute();
    ttrue(websOpenRoute() == 0, "open the route table");
    websDefineHandler("serve", serveMatch, 0, 0, 0);
    fp = fopen(ROUTES, "w");
    ttrue(fp != 0, "create the route file");
    fputs(table, fp);
    fclose(fp);
    rc = websLoad(ROUTES);
    unlink(ROUTES);
    return rc;
}


int main(int argc, char **argv)
{
    WebsRoute *catchAll;
    Webs      *wp;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    teq(websDefineHandler("serve", serveMatch, 0, 0, 0), 0, "define the serving handler");
    ttrue(websAddRoute("/r/", "redirect", -1) != 0, "add a redirect route without a target");
    catchAll = websAddRoute("/", "serve", -1);
    ttrue(catchAll != 0, "add the catch-all route");

    wp = request("/r/x.html");
    ttrue(wp->code >= 100, "a redirect route without a target must not answer with status %d", wp->code);
    ttrue(!wp->finalized, "a redirect route without a target must not finalize the request");
    ttrue(wp->route == catchAll, "a redirect route without a target reports no match and routing continues");
    websFree(wp);

    //  The loader refuses the configuration outright
    ttrue(load("route uri=/r/ handler=redirect\nroute uri=/ handler=serve\n") < 0,
          "a redirect route without a redirect= target must refuse the table");
    ttrue(load("route uri=/r/ handler=redirect redirect=401@/login.html\nroute uri=/ handler=serve\n") < 0,
          "a redirect route without a default target must refuse the table");
    teqi(load("route uri=/r/ handler=redirect redirect=/index.html\nroute uri=/ handler=serve\n"), 0,
         "a redirect route with a default target loads");
    teqi(load("route uri=/r/ handler=redirect redirect=*@/index.html\nroute uri=/ handler=serve\n"), 0,
         "a redirect route with a wildcard target loads");
    websCloseRoute();
    return 0;
}
