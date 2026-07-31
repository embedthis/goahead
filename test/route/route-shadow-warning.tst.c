/*
    route-shadow-warning.tst.c - Regression: warn when an unauthenticated route shadows an
    authenticated one.

    Routes are matched in order and a request that misses its protected route falls through to the next
    match rather than being denied. So any spelling the file system folds to a protected name - ASCII
    case, Unicode NFC/NFD, Win32 trailing dot/space/ADS, or Unicode full case folds such as U+017F
    LONG S and U+212A KELVIN - misses the strict route and is served by
    whatever permissive route claims it next.

    Rejecting that configuration closes every folding axis at once, including axes nobody has
    enumerated, because the audit never reasons about folding. It only asks: can an unauthenticated
    route later in the table serve what an authenticated route earlier in it protects?

    Four suites:
      A) a later unauthenticated route that overlaps an authenticated one warns
      B) configurations that cannot shadow are silent
      C) the warning names the worst shadowing route, not merely the first
      D) counting - one warning per protected route, not per shadowing pair

    websCheckRoutes() returns the problem count so a future strict mode (10207) can refuse to start.
    These suites assert on that return value as well as the emitted text.
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
    Run websCheckRoutes() over a freshly built route set and return its problem count. The caller adds
    routes in the callback so each case starts from an empty table.
 */
static int checkRoutes(void (*addRoutes)(void))
{
    WebsLogHandler priorHandler;
    int            problems;

    warnings = 0;
    lastMessage[0] = '\0';

    ttrue(websOpenRoute() == 0, "websOpenRoute failed");
    if (addRoutes) {
        (*addRoutes)();
    }
    priorHandler = logSetHandler(captureLog);
    problems = websCheckRoutes();
    logSetHandler(priorHandler);
    websCloseRoute();
    return problems;
}

static WebsRoute *addRoute(cchar *uri)
{
    WebsRoute *route = websAddRoute(uri, "continue", -1);

    ttrue(route != 0, "Cannot add route %s", uri);
    return route;
}

static WebsRoute *addAuthRoute(cchar *uri)
{
    WebsRoute *route = addRoute(uri);

    websSetRouteAuth(route, "basic");
    return route;
}

/********************************* Suite A: warns *******************************/

/*
    The shape of the shipped test/route.txt: a protected prefix ahead of a bare catch-all. This is the
    configuration that made every folding axis in this family exploitable.
 */
static void openProtectedThenCatchAll(void)
{
    addAuthRoute("/auth/basic/");
    addRoute("/");
}

/*
    Shadowing does not require a bare catch-all. Any later unauthenticated route whose prefix is a
    prefix of the protected one can claim a folded variant.
 */
static void openProtectedThenParent(void)
{
    addAuthRoute("/auth/basic/admin/");
    addRoute("/auth/");
}

/*
    An abilities-only route is a confidentiality boundary even with no auth type, so it must be
    protected from shadowing too.
 */
static void openAbilitiesThenCatchAll(void)
{
    WebsRoute *route = addRoute("/manage/");
    WebsHash  abilities = hashCreate(-1);

    hashEnter(abilities, "manage", valueInteger(0), 0);
    websSetRouteMatch(route, 0, 0, -1, -1, abilities, -1);
    addRoute("/");
}

static void suiteA_warns(void)
{
    tinfo("\n--- Suite A: a later unauthenticated route that overlaps warns ---\n");

    ttrue(checkRoutes(openProtectedThenCatchAll) == 1,
          "A1 protected + catch-all: expected one problem, got %d", warnings);
    ttrue(strstr(lastMessage, "/auth/basic/") != 0 && strstr(lastMessage, "requires authentication") != 0,
          "A2 warning names the protected route, got [%s]", lastMessage);

    ttrue(checkRoutes(openProtectedThenParent) == 1,
          "A3 protected + parent prefix: expected one problem, got %d", warnings);

    ttrue(checkRoutes(openAbilitiesThenCatchAll) == 1,
          "A4 abilities-only + catch-all: expected one problem, got %d", warnings);
}

/********************************* Suite B: silent ******************************/

/*
    Order is what makes shadowing possible. A catch-all ahead of the protected route never claims the
    protected route's traffic, because the protected route is never reached at all - a different
    misconfiguration, and not this one.
 */
static void openCatchAllThenProtected(void)
{
    addRoute("/");
    addAuthRoute("/auth/basic/");
}

/*
    A later route that also authenticates cannot serve the protected content without credentials.
 */
static void openTwoProtected(void)
{
    addAuthRoute("/auth/basic/admin/");
    addAuthRoute("/auth/");
}

/*
    Disjoint prefixes cannot claim each other's traffic.
 */
static void openDisjointPrefixes(void)
{
    addAuthRoute("/admin/");
    addRoute("/public/");
}

/*
    Overlapping prefixes but disjoint document roots: the catch-all resolves into a different tree, so
    it cannot serve what the protected route protects. This is the dir= remedy the warning suggests.
 */
static void openDisjointRegions(void)
{
    WebsRoute *strict, *loose;

    strict = addAuthRoute("/admin/");
    websSetRouteMatch(strict, "/var/private", 0, -1, -1, -1, -1);
    loose = addRoute("/");
    websSetRouteMatch(loose, "/var/www", 0, -1, -1, -1, -1);
}

static void suiteB_silent(void)
{
    tinfo("\n--- Suite B: configurations that cannot shadow are silent ---\n");

    ttrue(checkRoutes(openCatchAllThenProtected) == 0,
          "B1 catch-all before protected: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openTwoProtected) == 0,
          "B2 both routes authenticated: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openDisjointPrefixes) == 0,
          "B3 disjoint prefixes: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openDisjointRegions) == 0,
          "B4 disjoint dir=: expected silence, got [%s]", lastMessage);
}

/********************************* Suite C: names the worst **********************/

/*
    test/route.txt has three routes with prefix "/" - two qualified (methods=, extensions=) and one
    bare. A qualified route claims only a subset of requests, so the bare one is the more serious
    exposure and is the one the warning must name, regardless of table order.
 */
static void openQualifiedBeforeBare(void)
{
    WebsRoute *qualified;
    WebsHash  methods;

    addAuthRoute("/auth/basic/");
    qualified = addRoute("/");
    methods = hashCreate(-1);
    hashEnter(methods, "OPTIONS", valueInteger(0), 0);
    websSetRouteMatch(qualified, 0, 0, methods, -1, -1, -1);
    addRoute("/");
}

/*
    When every shadowing route is qualified there is still an exposure - a folded request for a
    protected .jst would be executed unauthenticated - so it must still warn.
 */
static void openOnlyQualified(void)
{
    WebsRoute *qualified;
    WebsHash  extensions;

    addAuthRoute("/auth/basic/");
    qualified = addRoute("/");
    extensions = hashCreate(-1);
    hashEnter(extensions, "jst", valueInteger(0), 0);
    websSetRouteMatch(qualified, 0, 0, -1, extensions, -1, -1);
}

static void suiteC_namesWorst(void)
{
    tinfo("\n--- Suite C: the warning names the worst shadowing route ---\n");

    ttrue(checkRoutes(openQualifiedBeforeBare) == 1,
          "C1 qualified then bare: expected one problem, got %d", warnings);
    ttrue(checkRoutes(openOnlyQualified) == 1,
          "C2 only a qualified shadow: expected one problem, got %d", warnings);
}

/********************************* Suite D: counting ****************************/

/*
    One warning per protected route, not one per (protected, shadowing) pair. The shipped
    test/route.txt has six protected routes and three routes with prefix "/", which would be eighteen
    warnings if counted pairwise.
 */
static void openManyProtectedOneCatchAll(void)
{
    addAuthRoute("/auth/basic/admin/");
    addAuthRoute("/auth/digest/admin/");
    addAuthRoute("/auth/basic/");
    addRoute("/");
}

static void suiteD_counting(void)
{
    tinfo("\n--- Suite D: one warning per protected route ---\n");

    ttrue(checkRoutes(openManyProtectedOneCatchAll) == 3,
          "D1 three protected + one catch-all: expected three problems, got %d", warnings);
    ttrue(warnings == 3, "D2 emitted message count matches the return value, got %d", warnings);
}

/********************************* Main *****************************************/

int main(int argc, char **argv)
{
    tinfo("route-shadow-warning: regression - warn when an unauthenticated route shadows an "
          "authenticated one\n");

    suiteA_warns();
    suiteB_silent();
    suiteC_namesWorst();
    suiteD_counting();

    return 0;
}

#else

int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH and ME_GOAHEAD_LOGGING");
    return 0;
}

#endif /* ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING */
