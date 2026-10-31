/*
    Report weaker routes reachable through Unicode case variants on case-insensitive file systems.
    Route matching folds ASCII only.
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
    websDefineHandler("continue", 0, serve, 0, 0);
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


static void addAuth(cchar *uri, cchar *handler, cchar *dir, cchar *authType, cchar *ability)
{
    WebsRoute *route;

    route = websAddRoute(uri, handler, -1);
    ttrue(route != 0, "add route %s", uri);
    websSetRouteMatch(route, dir, 0, -1, -1, ability ? set(ability) : -1, -1);
    websSetRouteAuth(route, authType);
}


#define CAFE "/caf\xC3\xA9/"

//  The stricter non-ASCII prefix is followed by a broader route that admits a weaker user
static void openManageThenView(void)
{
    addAuth(CAFE, "file", 0, "basic", "manage");
    addAuth("/", "file", 0, "basic", "view");
}


//  The same with the broader route nested one segment up
static void openManageThenViewNested(void)
{
    addAuth("/private" CAFE, "file", 0, "basic", "manage");
    addAuth("/private/", "file", 0, "basic", "view");
}


//  Equal requirements: the later route serves the folded spelling to the same users
static void openManageThenManage(void)
{
    addAuth(CAFE, "file", 0, "basic", "manage");
    addAuth("/", "file", 0, "basic", "manage");
}


//  An ASCII prefix is folded by route matching itself, so a later weaker route is the documented order
static void openAsciiManageThenView(void)
{
    addAuth("/admin/", "file", 0, "basic", "manage");
    addAuth("/", "file", 0, "basic", "view");
}


//  The non-ASCII part is shared by both prefixes, so a folded request misses both
static void openSharedNonAsciiPrefix(void)
{
    addAuth(CAFE "admin/", "file", 0, "basic", "manage");
    addAuth(CAFE, "file", 0, "basic", "view");
}


//  A gate on the non-ASCII prefix is missed the same way, and the later weaker route then serves its directory
static void openGateThenView(void)
{
    addAuth(CAFE, "continue", 0, "basic", "manage");
    addAuth("/", "file", 0, "basic", "view");
}


//  A later gate serves nothing itself
static void openManageThenGate(void)
{
    addAuth(CAFE, "file", 0, "basic", "manage");
    addAuth("/", "continue", 0, "basic", "view");
}


int main(int argc, char **argv)
{
    int problems;

    problems = checkRoutes(openManageThenView);
    ttrue(problems >= 1, "a weaker later route under a non-ASCII prefix must be reported, got %d", problems);
    ttrue(strstr(lastMessage, "folded") != 0, "the warning must name the fold, got: %s", lastMessage);

    problems = checkRoutes(openManageThenViewNested);
    ttrue(problems >= 1, "a nested weaker later route must be reported, got %d", problems);

    ttrue(checkRoutes(openManageThenManage) == 0, "equal requirements are not reported");
    ttrue(checkRoutes(openAsciiManageThenView) == 0, "an ASCII prefix is folded by the match and is not reported");
    ttrue(checkRoutes(openSharedNonAsciiPrefix) == 0, "a shared non-ASCII prefix is missed by both routes");
    problems = checkRoutes(openGateThenView);
    ttrue(problems >= 1, "a gate on a non-ASCII prefix before a weaker serving route must be reported, got %d", problems);
    ttrue(strstr(lastMessage, "folded") != 0, "the warning must name the fold, got: %s", lastMessage);
    ttrue(checkRoutes(openManageThenGate) == 0, "a later gate serves nothing and is not reported");
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication or logging is not in this build");
    return 0;
}
#endif
