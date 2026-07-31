/*
    route-auth-warning.tst.c - Regression: warn when no route requires authentication.

    The shipped route.txt is a single unauthenticated catch-all. Routes are matched in order, so with
    no authenticated route ahead of the catch-all, every route-matcher near-miss (prefix-boundary
    mismatch, canonicalization divergence, handler confusion) degrades to "served by the file handler"
    rather than "denied". websCheckRouteAuth() audits the loaded route set once at startup and warns
    when that is the shape of the configuration.

    Three suites:
      A) a route set with no auth= and no abilities= warns
      B) a route set with any authenticated route is silent
      C) edge cases - empty route set, and warning position independent of route order

    The warning is emitted via error() rather than logmsg() so it is visible at the default log level
    (ME_GOAHEAD_LOGFILE is "stderr:0", which suppresses every logmsg level). Suite B's silence is the
    load-bearing assertion: test/route.txt carries authenticated routes, so the shipped test suite must
    stay warning-free.
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

/********************************* Log capture **********************************/

static int  warnings;
static char lastMessage[512];

static void captureLog(int level, cchar *msg)
{
    warnings++;
    scopy(lastMessage, sizeof(lastMessage), msg ? msg : "");
}

/*
    Run websCheckRouteAuth() over a freshly built route set and return the number of messages logged.
    The caller adds routes in the callback so each case starts from an empty table.
 */
static int checkRoutes(void (*addRoutes)(void))
{
    WebsLogHandler priorHandler;

    warnings = 0;
    lastMessage[0] = '\0';

    ttrue(websOpenRoute() == 0, "websOpenRoute failed");
    if (addRoutes) {
        (*addRoutes)();
    }
    priorHandler = logSetHandler(captureLog);
    websCheckRouteAuth();
    logSetHandler(priorHandler);
    websCloseRoute();
    return warnings;
}

/*
    Routes are added with the "continue" handler. websOpenRoute registers "continue" and "redirect";
    the default "file" handler is only registered by websFileOpen, which this unit test does not need.
 */
static WebsRoute *addRoute(cchar *uri)
{
    WebsRoute *route = websAddRoute(uri, "continue", -1);

    ttrue(route != 0, "Cannot add route %s", uri);
    return route;
}

/********************************* Suite A: warns *******************************/

static void openUnauthenticatedCatchAll(void)
{
    addRoute("/");
}

static void openShippedShape(void)
{
    /* The shape of the shipped src/route.txt - /action, /jst and a terminal catch-all, none with auth */
    addRoute("/action");
    addRoute("/jst");
    addRoute("/");
}

static void suiteA_warns(void)
{
    tinfo("\n--- Suite A: unauthenticated route sets warn ---\n");

    ttrue(checkRoutes(openUnauthenticatedCatchAll) == 1,
          "A1 bare catch-all: expected one warning, got %d", warnings);
    ttrue(strstr(lastMessage, "no authenticated routes") != 0,
          "A2 warning names the cause, got [%s]", lastMessage);

    ttrue(checkRoutes(openShippedShape) == 1,
          "A3 shipped route.txt shape: expected one warning, got %d", warnings);
}

/********************************* Suite B: silent ******************************/

/*
    These route sets deliberately carry no trailing catch-all. A second audit covers an unauthenticated
    route that shadows an authenticated one, so "auth route + catch-all" now warns and is exercised
    by route-shadow-warning.tst.c. Suite B's subject is narrower: that each way of declaring
    a confidentiality boundary is recognised as one.
 */
static void openBasicAuth(void)
{
    websSetRouteAuth(addRoute("/auth/basic/"), "basic");
}

static void openDigestAuth(void)
{
    websSetRouteAuth(addRoute("/auth/digest/"), "digest");
}

/*
    websSetRouteAuth maps an unknown auth type - including "form", which has no askLogin/parseAuth
    callbacks - to an empty authType string. That is still a non-NULL authType and websRouteRequest
    treats it as authenticated, so the audit must too.
 */
static void openFormAuth(void)
{
    websSetRouteAuth(addRoute("/auth/form/"), "form");
}

/*
    A route may gate on abilities without declaring an auth type. websRouteRequest calls websCan()
    whenever abilities >= 0, so such a route is a confidentiality boundary.
 */
static void openAbilitiesOnly(void)
{
    WebsRoute *route = addRoute("/manage/");
    WebsHash  abilities = hashCreate(-1);

    hashEnter(abilities, "manage", valueInteger(0), 0);
    websSetRouteMatch(route, 0, 0, -1, -1, abilities, -1);
}

static void suiteB_silent(void)
{
    tinfo("\n--- Suite B: an authenticated route silences the warning ---\n");

    ttrue(checkRoutes(openBasicAuth) == 0, "B1 basic auth: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openDigestAuth) == 0, "B2 digest auth: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openFormAuth) == 0, "B3 form auth: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openAbilitiesOnly) == 0, "B4 abilities only: expected silence, got [%s]", lastMessage);
}

/********************************* Suite C: edges *******************************/

/*
    The authenticated route is last, after the catch-all. The audit scans the whole table, so
    position must not matter - a misordered config is still audited as configured.
 */
static void openAuthAfterCatchAll(void)
{
    addRoute("/");
    websSetRouteAuth(addRoute("/auth/basic/"), "basic");
}

static void suiteC_edges(void)
{
    tinfo("\n--- Suite C: edge cases ---\n");

    /* No routes at all serves nothing, so there is no boundary to warn about */
    ttrue(checkRoutes(0) == 0, "C1 empty route set: expected silence, got [%s]", lastMessage);

    ttrue(checkRoutes(openAuthAfterCatchAll) == 0,
          "C2 auth route after catch-all: expected silence, got [%s]", lastMessage);
}

/********************************* Main *****************************************/

int main(int argc, char **argv)
{
    tinfo("route-auth-warning: regression - warn when no route requires authentication\n");

    suiteA_warns();
    suiteB_silent();
    suiteC_edges();

    return 0;
}

#else

int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH and ME_GOAHEAD_LOGGING");
    return 0;
}

#endif /* ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING */
