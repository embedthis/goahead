/*
    Report public routes on the same prefix as a protocol-selected authentication gate.
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


static WebsHash set(cchar *key, cchar *value)
{
    WebsHash hash;

    hash = hashCreate(-1);
    hashEnter(hash, key, value ? valueString(value, VALUE_ALLOCATE) : valueInteger(0), 0);
    return hash;
}


static void addAuth(cchar *uri, cchar *handler, cchar *protocol)
{
    WebsRoute *route;

    route = websAddRoute(uri, handler, -1);
    ttrue(route != 0, "add route %s", uri);
    websSetRouteMatch(route, 0, protocol, -1, -1, set("manage", 0), -1);
    websSetRouteAuth(route, "basic");
}


//  An https-only gate, then the public route that serves the same prefix over http
static void openProtocolGateThenSame(void)
{
    addAuth("/sec/", "continue", "https");
    websAddRoute("/sec/", "file", -1);
}


//  The same with a serving route instead of a gate
static void openProtocolRouteThenSame(void)
{
    addAuth("/sec/", "file", "https");
    websAddRoute("/sec/", "file", -1);
}


//  A nested public route must also be reported.
static void openProtocolGateThenNested(void)
{
    addAuth("/sec/", "continue", "https");
    websAddRoute("/sec/sub/", "file", -1);
}


//  A gate without selectors covers every request for its prefix
static void openPlainGateThenSame(void)
{
    addAuth("/", "continue", 0);
    websAddRoute("/", "file", -1);
}


//  The shipped https redirect pattern authenticates nothing and must stay clean
static void openRedirectThenSame(void)
{
    WebsRoute *route;

    route = websAddRoute("/secure/", "redirect", -1);
    ttrue(route != 0, "add the redirect route");
    websSetRouteMatch(route, 0, "http", -1, -1, -1, set("0", "https://"));
    websAddRoute("/secure/", "file", -1);
}


int main(int argc, char **argv)
{
    int problems;

    problems = checkRoutes(openProtocolGateThenNested);
    ttrue(problems >= 1, "a protocol gate over a nested public route is reported, got %d", problems);

    problems = checkRoutes(openProtocolGateThenSame);
    ttrue(problems >= 1, "a protocol gate beside a public route on its own prefix must be reported, got %d",
          problems);
    ttrue(strstr(lastMessage, "\"/sec/\"") && strstr(lastMessage, "protocol"),
          "the warning must name the route and the protocol selector, got [%s]", lastMessage);

    problems = checkRoutes(openProtocolRouteThenSame);
    ttrue(problems >= 1, "a protocol-selected authenticated route beside a public route on its own prefix must be "
          "reported, got %d", problems);

    teqi(checkRoutes(openPlainGateThenSame), 0, "a gate without selectors beside its serving route is clean, got [%s]",
         lastMessage);
    teqi(checkRoutes(openRedirectThenSame), 0, "the http to https redirect pattern is clean, got [%s]", lastMessage);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH and ME_GOAHEAD_LOGGING");
    return 0;
}
#endif
