/*
    Report earlier serving routes with weaker authentication or abilities than a later route.
    Continue gates and disjoint selectors must not produce warnings.
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


static void addAuth(cchar *uri, cchar *handler, cchar *authType, cchar *ability, WebsHash methods)
{
    WebsRoute *route;

    route = websAddRoute(uri, handler, -1);
    ttrue(route != 0, "add route %s", uri);
    websSetRouteMatch(route, 0, 0, methods, -1, ability ? set(ability) : -1, -1);
    websSetRouteAuth(route, authType);
}


//  The view route takes every request for the admin directory before the manage route is consulted
static void openViewThenManage(void)
{
    addAuth("/auth/basic/", "file", "basic", "view", -1);
    addAuth("/auth/basic/admin/", "file", "basic", "manage", -1);
}


//  The earlier route accepts either ability, the later requires one of them
static void openEitherThenManage(void)
{
    addAuth("/auth/basic/", "file", "basic", "view|manage", -1);
    addAuth("/auth/basic/admin/", "file", "basic", "manage", -1);
}


//  The same prefix, so the later route is never reached
static void openViewThenManageSamePrefix(void)
{
    addAuth("/auth/basic/", "file", "basic", "view", -1);
    addAuth("/auth/basic/", "file", "basic", "manage", -1);
}


#if ME_GOAHEAD_DIGEST
//  A Basic route takes requests a Digest route was meant to authenticate
static void openBasicThenDigest(void)
{
    addAuth("/auth/", "file", "basic", "manage", -1);
    addAuth("/auth/digest/", "file", "digest", "manage", -1);
}
#endif


//  The stricter route first is the documented order
static void openManageThenView(void)
{
    addAuth("/auth/basic/admin/", "file", "basic", "manage", -1);
    addAuth("/auth/basic/", "file", "basic", "view", -1);
}


//  Equal requirements: the later route is redundant, not weakened
static void openManageThenManage(void)
{
    addAuth("/auth/basic/", "file", "basic", "manage", -1);
    addAuth("/auth/basic/admin/", "file", "basic", "manage", -1);
}


//  The earlier route requires one ability, the later accepts it among alternatives
static void openManageThenEither(void)
{
    addAuth("/auth/basic/", "file", "basic", "manage", -1);
    addAuth("/auth/basic/admin/", "file", "basic", "view|manage", -1);
}


//  A gate passes the request on to the later route, which then applies its own requirement
static void openGateThenManage(void)
{
    addAuth("/auth/basic/", "continue", "basic", "view", -1);
    addAuth("/auth/basic/admin/", "file", "basic", "manage", -1);
}


//  Disjoint methods do not overlap: GET needs view, POST needs manage
static void openDisjointMethods(void)
{
    addAuth("/api/", "file", "basic", "view", set("GET"));
    addAuth("/api/", "file", "basic", "manage", set("POST"));
}


int main(int argc, char **argv)
{
    int problems;

    problems = checkRoutes(openViewThenManage);
    ttrue(problems >= 1, "a view route before a nested manage route must be reported, got %d", problems);
    ttrue(strstr(lastMessage, "\"/auth/basic/\"") && strstr(lastMessage, "\"/auth/basic/admin/\""),
          "the warning must name both routes, got [%s]", lastMessage);

    problems = checkRoutes(openEitherThenManage);
    ttrue(problems >= 1, "a view|manage route before a nested manage route must be reported, got %d", problems);

    problems = checkRoutes(openViewThenManageSamePrefix);
    ttrue(problems >= 1, "a view route before a manage route on the same prefix must be reported, got %d", problems);

#if ME_GOAHEAD_DIGEST
    problems = checkRoutes(openBasicThenDigest);
    ttrue(problems >= 1, "a Basic route before a nested Digest route must be reported, got %d", problems);
#endif

    teqi(checkRoutes(openManageThenView), 0, "the stricter route first is clean, got [%s]", lastMessage);
    teqi(checkRoutes(openManageThenManage), 0, "equal requirements are clean, got [%s]", lastMessage);
    teqi(checkRoutes(openManageThenEither), 0, "a requirement met by the earlier route is clean, got [%s]",
         lastMessage);
    teqi(checkRoutes(openGateThenManage), 0, "a gate before the stricter route is clean, got [%s]", lastMessage);
    teqi(checkRoutes(openDisjointMethods), 0, "disjoint method selectors are clean, got [%s]", lastMessage);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH and ME_GOAHEAD_LOGGING");
    return 0;
}
#endif
