/* Following the shipped route.txt guidance protects actions and templates */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

#define SHIPPED     "../../src/route.txt"
#define STAGED      "route-guidance-staged.txt"
#define EXAMPLE     "#       route "
#define EXAMPLES    "#   Examples:\n"
#define MARKER      "#   comes before an authenticated route that covers it. Start with --no-strict to only warn.\n#\n"

static cchar *loginBlock =
    "route uri=/pub/\n"
    "route uri=/action/login methods=POST handler=action redirect=200@/ redirect=401@/pub/login.html\n"
    "route uri=/action/logout methods=POST handler=action redirect=200@/pub/login.html\n"
    "route uri=/ auth=form handler=continue redirect=401@/pub/login.html\n";

static int  warnings;
static char messages[4096];

static void captureLog(int level, cchar *msg)
{
    warnings++;
    if (slen(messages) + slen(msg) + 2 < (ssize) sizeof(messages)) {
        strcat(messages, msg);
        strcat(messages, "\n");
    }
}

static bool service(Webs *wp)
{
    return 1;
}

static void openRoutes(void)
{
    ttrue(websOpenRoute() == 0, "open the route table");
    websSetDocuments("web");
    websDefineHandler("file", 0, service, 0, 0);
    websDefineHandler("action", 0, service, 0, 0);
    websDefineHandler("jst", 0, service, 0, 0);
    websDefineHandler("cgi", 0, service, 0, 0);
}

static int checkRoutes(void)
{
    WebsLogHandler prior;
    int            problems;

    warnings = 0;
    messages[0] = '\0';
    prior = logSetHandler(captureLog);
    problems = websCheckRoutes();
    logSetHandler(prior);
    websCloseRoute();
    return problems;
}

/*
    Insert the example before the selected line or documented insertion point, then return audit problems.
 */
static int checkShipped(cchar *block, cchar *before)
{
    FILE  *fp;
    char  *text, *at, *staged;
    ssize len;
    int   rc;

    text = websReadWholeFile(SHIPPED);
    ttrue(text != 0, "read the shipped route.txt");
    if (!text) {
        return -1;
    }
    if (before) {
        at = strstr(text, before);
        at = at ? at + 1 : 0;
    } else {
        at = strstr(text, MARKER);
        at = at ? at + slen(MARKER) : 0;
    }
    ttrue(at != 0, "find where the login block goes");
    if (!at) {
        wfree(text);
        return -1;
    }
    len = at - text;
    staged = sfmt("%.*s%s%s", (int) len, text, block, at);
    rc = -1;
    if ((fp = fopen(STAGED, "w")) != 0) {
        rc = fputs(staged, fp) >= 0 ? 0 : -1;
        fclose(fp);
    }
    ttrue(rc == 0, "stage the route file");
    openRoutes();
    rc = websLoad(STAGED);
    ttrue(rc == 0, "load the staged route file");
    unlink(STAGED);
    wfree(staged);
    wfree(text);
    return checkRoutes();
}

static void suiteA_shippedGuidance(void)
{
    char *text, *marker, *action, *jst;
    int  problems;

    tinfo("\n--- Suite A: the shipped guidance followed as written ---\n");
    text = websReadWholeFile(SHIPPED);
    ttrue(text != 0, "read the shipped route.txt");
    if (text) {
        marker = strstr(text, MARKER);
        action = strstr(text, "\nroute uri=/action handler=action\n");
        jst = strstr(text, "\nroute uri=/ extensions=jst,asp handler=jst\n");
        ttrue(marker && action && jst, "the insertion point and the action and template routes are present");
        ttrue(marker && action && marker < action, "the insertion point is above the action route");
        ttrue(marker && jst && marker < jst, "the insertion point is above the template route");
        wfree(text);
    }
    problems = checkShipped(loginBlock, 0);
    teq(problems, 0, "the login block at the documented place is accepted, got: %s", messages);
}

static void suiteB_oldGuidance(void)
{
    int problems;

    tinfo("\n--- Suite B: the login block below the action and template routes ---\n");
    problems = checkShipped(loginBlock, "\nroute uri=/\n");
    teq(problems, 2, "the action and template routes are reported, got: %s", messages);
    ttrue(strstr(messages, "\"/action\" (handler=action)") != 0, "the action route is named");
    ttrue(strstr(messages, "(handler=jst)") != 0, "the template route is named");
}

static WebsRoute *add(cchar *uri, cchar *handler)
{
    WebsRoute *route = websAddRoute(uri, handler, -1);

    ttrue(route != 0, "add route %s", uri);
    return route;
}

static WebsRoute *addAuth(cchar *uri)
{
    WebsRoute *route = add(uri, "continue");

    websSetRouteAuth(route, "basic");
    return route;
}

static void suiteC_codeRoutes(void)
{
    int problems;

    tinfo("\n--- Suite C: code routes ahead of an authenticated route ---\n");

    openRoutes();
    add("/action", "action");
    addAuth("/");
    problems = checkRoutes();
    teq(problems, 1, "every action ahead of the gate warns");

    openRoutes();
    add("/admin/", "cgi");
    addAuth("/");
    problems = checkRoutes();
    teq(problems, 1, "a CGI directory ahead of the gate warns");

    openRoutes();
    add("/", "jst");
    addAuth("/private/");
    problems = checkRoutes();
    teq(problems, 1, "templates everywhere ahead of a protected directory warn");

    openRoutes();
    add("/action/login", "action");
    addAuth("/");
    problems = checkRoutes();
    teq(problems, 0, "a single action ahead of the gate is an exception, got: %s", messages);

    openRoutes();
    addAuth("/");
    add("/action", "action");
    add("/", "jst");
    problems = checkRoutes();
    teq(problems, 0, "code routes after the gate are protected by it, got: %s", messages);

    openRoutes();
    add("/public/", "jst");
    addAuth("/private/");
    problems = checkRoutes();
    teq(problems, 0, "a code route beside a protected directory does not overlap it, got: %s", messages);
}

static void suiteD_continueGate(void)
{
    int problems;

    tinfo("\n--- Suite D: a continue gate followed by the same prefix ---\n");
    openRoutes();
    addAuth("/");
    add("/", "file");
    problems = checkRoutes();
    teq(problems, 0, "a route with the gate's own prefix cannot be reached around it, got: %s", messages);

    openRoutes();
    addAuth("/private/");
    add("/", "file");
    problems = checkRoutes();
    teq(problems, 1, "a broader route after a protected directory still warns");
}

/*
    Uncomment consecutive example route lines at their documented insertion points.
 */
static void suiteE_shippedExamples(void)
{
    char *text, *line, *next, *block, *prior;
    int  blocks, problems;

    tinfo("\n--- Suite E: every shipped example is accepted ---\n");
    text = websReadWholeFile(SHIPPED);
    ttrue(text != 0, "read the shipped route.txt");
    if (!text) {
        return;
    }
    blocks = 0;
    block = 0;
    line = strstr(text, EXAMPLES);
    ttrue(line != 0, "find the examples");
    for (; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) {
            *next++ = '\0';
        }
        if (sstarts(line, EXAMPLE)) {
            prior = block;
            block = sfmt("%s%s\n", prior ? prior : "", &line[slen(EXAMPLE) - slen("route ")]);
            wfree(prior);
        } else if (block) {
            problems = checkShipped(block, 0);
            teq(problems, 0, "the example is accepted:\n%s\ngot: %s", block, messages);
            wfreeset(block);
            blocks++;
        }
    }
    ttrue(blocks >= 5, "found the shipped examples, got %d", blocks);
    wfree(text);
}

int main(int argc, char **argv)
{
    suiteA_shippedGuidance();
    suiteB_oldGuidance();
    suiteC_codeRoutes();
    suiteD_continueGate();
    suiteE_shippedExamples();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication and logging are required");
    return 0;
}
#endif
