/*
    Report a weaker write route before an overlapping route that gates all methods.
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

static int  warnings;
static char lastMessage[1024];

static void captureLog(int level, cchar *msg)
{
    warnings++;
    scopy(lastMessage, sizeof(lastMessage), msg ? msg : "");
}


static bool serve(Webs *wp)
{
    return 1;
}


static int checkRoutes(void (*addRoutes)(void))
{
    WebsLogHandler priorHandler;
    int            problems;

    warnings = 0;
    lastMessage[0] = '\0';
    ttrue(websOpenRoute() == 0, "websOpenRoute failed");
    websDefineHandler("file", 0, serve, 0, 0);
    (*addRoutes)();
    priorHandler = logSetHandler(captureLog);
    problems = websCheckRoutes();
    logSetHandler(priorHandler);
    websCloseRoute();
    return problems;
}


static WebsHash set(cchar *keys)
{
    WebsHash hash;
    char     *copy, *key, *tok;

    hash = hashCreate(-1);
    copy = sclone(keys);
    for (key = stok(copy, "|", &tok); key; key = stok(0, "|", &tok)) {
        hashEnter(hash, key, valueInteger(0), 0);
    }
    wfree(copy);
    return hash;
}


static void addAuth(cchar *uri, cchar *handler, cchar *authType, cchar *ability, WebsHash methods)
{
    WebsRoute *route;

    route = websAddRoute(uri, handler, -1);
    ttrue(route != 0, "add route %s", uri);
    websSetRouteMatch(route, 0, 0, methods, -1, ability ? set(ability) : -1, -1);
    websSetRouteAuth(route, authType);
}


//  A broader write route for viewers before an admin directory that requires manage for everything
static void openWriteViewThenManage(void)
{
    addAuth("/", "file", "basic", "view", set("PUT|DELETE"));
    addAuth("/admin/", "file", "basic", "manage", -1);
}


//  The later route is a gate without methods=: it gates every method
static void openWriteViewThenManageGate(void)
{
    addAuth("/", "file", "basic", "view", set("PUT"));
    addAuth("/admin/", "continue", "basic", "manage", -1);
}


//  Equal requirements: the write route meets what the later route asks
static void openWriteManageThenManage(void)
{
    addAuth("/", "file", "basic", "manage", set("PUT|DELETE"));
    addAuth("/admin/", "file", "basic", "manage", -1);
}


//  The later route names GET only, so a PUT never concerns it
static void openWriteViewThenGetManage(void)
{
    addAuth("/", "file", "basic", "view", set("PUT|DELETE"));
    addAuth("/admin/", "file", "basic", "manage", set("GET"));
}


//  The earlier route has no methods=: it gates PUT with view and passes it on to the later manage route
static void openViewThenWriteManage(void)
{
    addAuth("/", "file", "basic", "view", -1);
    addAuth("/admin/", "file", "basic", "manage", set("PUT|DELETE"));
}


int main(int argc, char **argv)
{
    int problems;

    problems = checkRoutes(openWriteViewThenManage);
    ttrue(problems >= 1, "a write-only view route before a nested manage route without methods= must be reported, "
          "got %d", problems);
    ttrue(strstr(lastMessage, "\"/\"") && strstr(lastMessage, "\"/admin/\""),
          "the warning must name both routes, got [%s]", lastMessage);

    problems = checkRoutes(openWriteViewThenManageGate);
    ttrue(problems >= 1, "a write-only view route before a nested manage gate must be reported, got %d", problems);

    teqi(checkRoutes(openWriteManageThenManage), 0, "a write route meeting the later requirement is clean, got [%s]",
         lastMessage);
    teqi(checkRoutes(openWriteViewThenGetManage), 0, "a later route naming GET only is clean, got [%s]", lastMessage);
    teqi(checkRoutes(openViewThenWriteManage), 0, "a gating view route before a manage write route is clean, got [%s]",
         lastMessage);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH and ME_GOAHEAD_LOGGING");
    return 0;
}
#endif
