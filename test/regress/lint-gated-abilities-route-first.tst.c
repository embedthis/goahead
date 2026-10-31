/*
    Report an abilities-only route after an authentication gate that shadows a stricter route.
    Without an earlier gate, the abilities-only route admits nobody.
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


static WebsHash set(cchar *key)
{
    WebsHash hash;

    hash = hashCreate(-1);
    hashEnter(hash, key, valueInteger(0), 0);
    return hash;
}


//  authType may be null: an abilities-only route that relies on an earlier gate
static void add(cchar *uri, cchar *handler, cchar *authType, cchar *ability, WebsHash methods)
{
    WebsRoute *route;

    route = websAddRoute(uri, handler, -1);
    ttrue(route != 0, "add route %s", uri);
    websSetRouteMatch(route, 0, 0, methods, -1, ability ? set(ability) : -1, -1);
    if (authType) {
        websSetRouteAuth(route, authType);
    }
}


//  Login gate, then a site-wide view route, then an admin route that needs manage: the view route takes it all
static void openGateViewThenManage(void)
{
    add("/", "continue", "basic", 0, -1);
    add("/", "file", 0, "view", -1);
    add("/admin/", "file", 0, "manage", -1);
}


//  The same with a form login gate and an authenticated admin route
static void openFormGateViewThenManage(void)
{
    add("/", "continue", "form", 0, -1);
    add("/", "file", 0, "view", -1);
    add("/admin/", "file", "form", "manage", -1);
}


//  Equal prefixes: the later route is never reached
static void openGateViewThenManageSamePrefix(void)
{
    add("/", "continue", "basic", 0, -1);
    add("/", "file", 0, "view", -1);
    add("/", "file", 0, "manage", -1);
}


#if ME_GOAHEAD_DIGEST
//  A Basic gate authenticates the user the abilities-only route serves, but the admin route wanted Digest
static void openBasicGateViewThenDigest(void)
{
    add("/", "continue", "basic", 0, -1);
    add("/", "file", 0, "view", -1);
    add("/admin/", "file", "digest", "view", -1);
}
#endif


//  No gate: an abilities-only route admits nobody, so it shadows nothing
static void openNoGateViewThenManage(void)
{
    add("/", "file", 0, "view", -1);
    add("/admin/", "file", 0, "manage", -1);
}


//  The stricter route first is the documented order
static void openGateManageThenView(void)
{
    add("/", "continue", "basic", 0, -1);
    add("/admin/", "file", 0, "manage", -1);
    add("/", "file", 0, "view", -1);
}


//  The earlier route requires what the later one does, and the gate's scheme is the later route's
static void openGateManageThenAuthManage(void)
{
    add("/", "continue", "basic", 0, -1);
    add("/", "file", 0, "manage", -1);
    add("/admin/", "file", "basic", "manage", -1);
}


//  Disjoint methods do not overlap
static void openGateDisjointMethods(void)
{
    add("/", "continue", "basic", 0, -1);
    add("/api/", "file", 0, "view", set("GET"));
    add("/api/", "file", 0, "manage", set("POST"));
}


int main(int argc, char **argv)
{
    int problems;

    problems = checkRoutes(openGateViewThenManage);
    ttrue(problems >= 1, "an abilities-only view route after a gate and before a nested manage route must be reported, "
          "got %d", problems);
    ttrue(strstr(lastMessage, "\"/\"") && strstr(lastMessage, "\"/admin/\""),
          "the warning must name both routes, got [%s]", lastMessage);

    problems = checkRoutes(openFormGateViewThenManage);
    ttrue(problems >= 1, "a form gate with an abilities-only view route before an authenticated manage route must be "
          "reported, got %d", problems);

    problems = checkRoutes(openGateViewThenManageSamePrefix);
    ttrue(problems >= 1, "an abilities-only view route before a manage route on the same prefix must be reported, "
          "got %d", problems);

#if ME_GOAHEAD_DIGEST
    problems = checkRoutes(openBasicGateViewThenDigest);
    ttrue(problems >= 1, "a Basic-gated abilities-only route before a Digest route must be reported, got %d", problems);
#endif

    teqi(checkRoutes(openNoGateViewThenManage), 0, "an abilities-only route with no gate admits nobody and is clean, "
         "got [%s]", lastMessage);
    teqi(checkRoutes(openGateManageThenView), 0, "the stricter route first is clean, got [%s]", lastMessage);
    teqi(checkRoutes(openGateManageThenAuthManage), 0, "a requirement met by the earlier route is clean, got [%s]",
         lastMessage);
    teqi(checkRoutes(openGateDisjointMethods), 0, "disjoint method selectors are clean, got [%s]", lastMessage);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH and ME_GOAHEAD_LOGGING");
    return 0;
}
#endif
