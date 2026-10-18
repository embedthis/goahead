/* The route lint reports a nested later route an authenticated route's selectors miss */

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
    websDefineHandler("jst", 0, serve, 0, 0);
    websDefineHandler("file", 0, serve, 0, 0);
    (*addRoutes)();
    priorHandler = logSetHandler(captureLog);
    problems = websCheckRoutes();
    logSetHandler(priorHandler);
    websCloseRoute();
    return problems;
}


static WebsHash set(cchar *a, cchar *b)
{
    WebsHash hash;

    hash = hashCreate(-1);
    hashEnter(hash, a, valueInteger(0), 0);
    if (b) {
        hashEnter(hash, b, valueInteger(0), 0);
    }
    return hash;
}


static void addGate(cchar *protocol, WebsHash methods)
{
    WebsRoute *route;

    route = websAddRoute("/admin/", "continue", -1);
    websSetRouteMatch(route, 0, protocol, methods, -1, set("manage", 0), -1);
    websSetRouteAuth(route, "basic");
}


static void addNested(WebsHash methods)
{
    WebsRoute *route;

    route = websAddRoute("/admin/reports/", "jst", -1);
    websSetRouteMatch(route, 0, 0, methods, set("jst", 0), -1, -1);
    websAddRoute("/pub/", "file", -1);
}


//  A gate that lists GET only, so a POST reaches the nested route anonymously
static void openGetOnlyGate(void)
{
    addGate(0, set("GET", 0));
    addNested(-1);
}


//  A gate for https only, so a plain http request reaches the nested route anonymously
static void openProtocolGate(void)
{
    addGate("https", -1);
    addNested(-1);
}


//  A gate without methods= authenticates every method
static void openPlainGate(void)
{
    addGate(0, -1);
    addNested(-1);
}


//  A gate with methods=GET|POST covers HEAD too, which follows GET
static void openGetPostGate(void)
{
    addGate(0, set("GET", "POST"));
    addNested(-1);
}


//  A nested route limited to the gate's own methods is covered
static void openMatchingMethods(void)
{
    addGate(0, set("GET", "POST"));
    addNested(set("GET", "POST"));
}


int main(int argc, char **argv)
{
    int problems;

    problems = checkRoutes(openGetOnlyGate);
    ttrue(problems >= 1, "a GET-only gate over a nested route must be reported, got %d", problems);
    ttrue(strstr(lastMessage, "\"/admin/reports/\"") && strstr(lastMessage, "POST"),
        "the warning must name the nested route and the method, got [%s]", lastMessage);

    problems = checkRoutes(openProtocolGate);
    ttrue(problems >= 1, "a protocol=https gate over a nested route must be reported, got %d", problems);
    ttrue(strstr(lastMessage, "Drop protocol="), "the warning must say how to fix it, got [%s]", lastMessage);

    teq(checkRoutes(openPlainGate), 0, "a gate without methods= covers every method, got [%s]", lastMessage);
    teq(checkRoutes(openGetPostGate), 0, "a GET|POST gate covers HEAD, got [%s]", lastMessage);
    teq(checkRoutes(openMatchingMethods), 0, "a nested route with the gate's methods is covered, got [%s]",
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
