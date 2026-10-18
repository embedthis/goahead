/* Removing a route does not free it under a request that is still using it */
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

static cchar *servedDir;

static bool serveMatch(Webs *wp)
{
    return 1;
}

static bool serveService(Webs *wp)
{
    servedDir = wp->route->dir;
    wp->code = HTTP_CODE_OK;
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

int main(int argc, char **argv)
{
    WebsRoute *removable, *catchAll;
    Webs      *wp, *other, *fresh;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websRuntimeOpen();
    teq(websOpenRoute(), 0, "open the route table");
    teq(websDefineHandler("serve", serveMatch, serveService, 0, 0), 0, "define the serving handler");

    removable = websAddRoute("/removable/", "serve", -1);
    ttrue(removable != 0, "add the removable route");
    websSetRouteMatch(removable, "removable-docs", 0, -1, -1, -1, -1);
    catchAll = websAddRoute("/", "serve", -1);
    ttrue(catchAll != 0, "add the catch-all route");

    wp = request("/removable/page.html");
    teqp(wp->route, removable, "the request selects the removable route");

    teq(websRemoveRoute("/removable/"), 0, "remove the route while the request is in flight");
    teq(routeCount, 1, "the route is unlinked at once");
    teq(retiredCount, 1, "the route is retired, not freed, while a request holds it");
    ttrue(removable->flags & WEBS_ROUTE_REMOVED, "the retired route is marked removed");
    teqp(wp->route, removable, "the request keeps the route it was authorized against");

    fresh = request("/removable/other.html");
    teqp(fresh->route, catchAll, "a new request does not select the removed route");
    websFree(fresh);
    teq(retiredCount, 1, "a request that never held the route does not free it");

    servedDir = 0;
    ttrue(websRunRequest(wp), "the in-flight request is dispatched");
    teq(wp->code, HTTP_CODE_OK, "the in-flight request completes with 200");
    ttrue(servedDir && smatch(servedDir, "removable-docs"), "the handler reads the route's directory");

    other = request("/removable/again.html");
    ttrue(other->route == catchAll, "a second new request does not select the removed route");
    websFree(other);

    websFree(wp);
    teq(retiredCount, 0, "the retired route is freed when its last request ends");

    //  A route no request holds is freed at once
    ttrue(websAddRoute("/unused/", "serve", 0) != 0, "add an unused route");
    teq(websRemoveRoute("/unused/"), 0, "remove the unused route");
    teq(retiredCount, 0, "an unheld route is freed without retiring");

    //  Shutdown frees a retired route that is still held
    ttrue(websAddRoute("/closing/", "serve", 0) != 0, "add a route to hold across shutdown");
    wp = request("/closing/page.html");
    ttrue(wp->route != 0 && wp->route != catchAll, "the request selects the route");
    teq(websRemoveRoute("/closing/"), 0, "remove the held route");
    teq(retiredCount, 1, "the held route is retired");
    websCloseRoute();
    teq(retiredCount, 0, "shutdown frees the retired list");
    websFree(wp);
    websRuntimeClose();
    return 0;
}
