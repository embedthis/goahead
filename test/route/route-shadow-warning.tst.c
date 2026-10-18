/*
    route-shadow-warning.tst.c - Regression: warn when an unauthenticated route shadows an authenticated one.
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

static WebsRoute *addHandlerRoute(cchar *uri, cchar *handler);

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

/*
    Warn when a broader route bypasses a protocol restriction.
 */
static void openProtocolThenCatchAll(void)
{
    WebsRoute *route = addRoute("/secure/");

    websSetRouteMatch(route, 0, "http", -1, -1, -1, -1);
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

    ttrue(checkRoutes(openProtocolThenCatchAll) == 1,
          "A5 protocol-only + catch-all: expected one problem, got %d", warnings);
    ttrue(strstr(lastMessage, "/secure/") != 0,
          "A6 warning names the protocol-only route, got [%s]", lastMessage);
}

/********************************* Suite B: silent ******************************/


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

    strict = addHandlerRoute("/admin/", "file");
    websSetRouteAuth(strict, "basic");
    websSetRouteMatch(strict, "/var/private", 0, -1, -1, -1, -1);
    loose = addHandlerRoute("/", "file");
    websSetRouteMatch(loose, "/var/www", 0, -1, -1, -1, -1);
}


static void openGateWithDir(void)
{
    WebsRoute *gate;

    gate = addAuthRoute("/auth/basic/");
    websSetRouteMatch(gate, "private", 0, -1, -1, -1, -1);
    addHandlerRoute("/", "file");
}


static void openProtocolThenSamePrefix(void)
{
    WebsRoute *route = websAddRoute("/", "redirect", -1);

    websSetRouteMatch(route, 0, "http", -1, -1, -1, -1);
    addRoute("/");
}


static void openMethodsThenSamePrefix(void)
{
    WebsRoute *route = addAuthRoute("/");
    WebsHash  methods = hashCreate(-1);

    hashEnter(methods, "GET", valueInteger(0), 0);
    websSetRouteMatch(route, 0, 0, methods, -1, -1, -1);
    addRoute("/");
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
    ttrue(checkRoutes(openGateWithDir) == 1,
          "B4b continue gate with dir=: expected one problem, got %d", warnings);
    ttrue(strstr(lastMessage, "give \"/auth/basic/\" its own dir=") == 0 &&
          strstr(lastMessage, "Serve the directory from \"/auth/basic/\" itself") != 0,
          "B4b the warning must not recommend dir= for the gate, got [%s]", lastMessage);
    ttrue(checkRoutes(openProtocolThenSamePrefix) == 0,
          "B5 protocol-only route then the same prefix: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openMethodsThenSamePrefix) == 1,
          "B6 methods-qualified route then the same prefix: expected one problem, got %d", warnings);
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

/********************************* Suite E: earlier routes ************************/

static bool serve(Webs *wp)
{
    return 1;
}

/*
    Add a route with a named handler, defining the handler for this table
 */
static WebsRoute *addHandlerRoute(cchar *uri, cchar *handler)
{
    WebsRoute *route;

    websDefineHandler(handler, 0, serve, 0, 0);
    route = websAddRoute(uri, handler, -1);
    ttrue(route != 0, "Cannot add route %s", uri);
    return route;
}

static void openFileCatchAllFirst(void)
{
    addHandlerRoute("/", "file");
    addAuthRoute("/auth/basic/");
}

static void openNoSlashFirst(void)
{
    addHandlerRoute("/admin", "file");
    addAuthRoute("/admin/");
}

static void openCustomHandlerFirst(void)
{
    addHandlerRoute("/admin/", "custom");
    addAuthRoute("/admin/");
}

static void openMethodsQualifiedFirst(void)
{
    WebsRoute *route = addHandlerRoute("/", "file");
    WebsHash  methods = hashCreate(-1);

    hashEnter(methods, "OPTIONS", valueInteger(0), 0);
    websSetRouteMatch(route, 0, 0, methods, -1, -1, -1);
    addAuthRoute("/admin/");
}

static void openPublicExemptionFirst(void)
{
    addHandlerRoute("/pub/", "file");
    addAuthRoute("/");
}

static void openUploadFirst(void)
{
    addHandlerRoute("/reroute/", "upload");
    addAuthRoute("/reroute/");
}

static void openRedirectFirst(void)
{
    addHandlerRoute("/", "redirect");
    addAuthRoute("/auth/");
}

static void suiteE_earlier(void)
{
    tinfo("\n--- Suite E: an earlier route that claims a protected route's requests warns ---\n");

    ttrue(checkRoutes(openFileCatchAllFirst) == 1,
          "E1 file catch-all first: expected one problem, got %d", warnings);
    ttrue(strstr(lastMessage, "\"/\" (handler=file) comes before authenticated route \"/auth/basic/\"") != 0,
          "E2 warning names both routes, got [%s]", lastMessage);
    ttrue(checkRoutes(openNoSlashFirst) == 1,
          "E3 /admin before /admin/: expected one problem, got %d", warnings);
    ttrue(checkRoutes(openCustomHandlerFirst) == 1,
          "E4 embedder handler on the same prefix first: expected one problem, got %d", warnings);
    ttrue(checkRoutes(openMethodsQualifiedFirst) == 1,
          "E5 methods-qualified route first: expected one problem, got %d", warnings);

    ttrue(checkRoutes(openPublicExemptionFirst) == 0,
          "E6 public exemption before a broader gate: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openUploadFirst) == 0,
          "E7 upload handler first: expected silence, got [%s]", lastMessage);
    ttrue(checkRoutes(openRedirectFirst) == 0,
          "E8 redirect first: expected silence, got [%s]", lastMessage);
}

/********************************* Suite F: spellings of one directory **********/

static cchar *protectedDir;


static void openProtectedDirThenCatchAll(void)
{
    WebsRoute *strict;

    strict = addHandlerRoute("/admin/", "file");
    websSetRouteAuth(strict, "basic");
    websSetRouteMatch(strict, protectedDir, 0, -1, -1, -1, -1);
    addHandlerRoute("/", "file");
}

static int checkSpelling(cchar *dir)
{
    protectedDir = dir;
    return checkRoutes(openProtectedDirThenCatchAll);
}

static void suiteF_spellings(void)
{
    char *saved, *absolute, *upper;
    char cwd[1024];

    tinfo("\n--- Suite F: every spelling of the documents directory overlaps it ---\n");

    saved = websGetDocuments() ? sclone(websGetDocuments()) : 0;
    websSetDocuments("web");
    ttrue(getcwd(cwd, sizeof(cwd)) != 0, "F0 cannot get the working directory");

    ttrue(checkSpelling("web") == 1, "F1 same spelling: expected one problem, got %d", warnings);
    ttrue(checkSpelling("./web") == 1, "F2 ./web: expected one problem, got %d", warnings);
    ttrue(checkSpelling("web/") == 1, "F3 web/: expected one problem, got %d", warnings);
    ttrue(checkSpelling("web//") == 1, "F4 web//: expected one problem, got %d", warnings);
    ttrue(checkSpelling("sub/../web") == 1, "F5 sub/../web: expected one problem, got %d", warnings);
    ttrue(checkSpelling("web/sub") == 1, "F6 inside the documents directory: expected one problem, got %d",
          warnings);

    absolute = sfmt("%s/web", cwd);
    ttrue(checkSpelling(absolute) == 1, "F7 absolute spelling: expected one problem, got %d", warnings);
    wfree(absolute);

    upper = supper(sclone("web"));
#if ME_GOAHEAD_CASE_INSENSITIVE_FS
    ttrue(checkSpelling(upper) == 1, "F8 WEB on a case-insensitive file system: expected one problem, got %d",
          warnings);
#else
    ttrue(checkSpelling(upper) == 0, "F8 WEB on a case-sensitive file system: expected silence, got [%s]",
          lastMessage);
#endif
    wfree(upper);

    ttrue(checkSpelling("web/../private") == 0, "F9 web/../private: expected silence, got [%s]", lastMessage);
    ttrue(checkSpelling("private") == 0, "F10 private: expected silence, got [%s]", lastMessage);
    ttrue(checkSpelling("webx") == 0, "F11 a name sharing only a prefix: expected silence, got [%s]",
          lastMessage);

    if (saved) {
        websSetDocuments(saved);
        wfreeset(saved);
    }
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
    suiteE_earlier();
    suiteF_spellings();

    return 0;
}

#else

int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH and ME_GOAHEAD_LOGGING");
    return 0;
}

#endif /* ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING */
