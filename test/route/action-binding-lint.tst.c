/*
    action-binding-lint.tst.c - Action routes run only the actions they name, and the route check reports an
    authenticated action route that names no action beside a public one
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

static void quiet(int level, cchar *msg)
{
}

static bool serve(Webs *wp)
{
    return 1;
}

static WebsRoute *add(cchar *uri, bool protect)
{
    WebsRoute *route;

    route = websAddRoute(uri, "action", -1);
    ttrue(route != 0, "add route %s", uri);
    if (protect) {
        websSetRouteAuth(route, "basic");
    }
    return route;
}

static int check(void)
{
    WebsLogHandler prior;
    int            problems;

    prior = logSetHandler(quiet);
    problems = websCheckRoutes();
    logSetHandler(prior);
    websCloseRoute();
    return problems;
}

static void openTable(void)
{
    ttrue(websOpenRoute() == 0, "open the route table");
    websDefineHandler("action", 0, serve, 0, 0);
}

int main(int argc, char **argv)
{
    WebsRoute *login, *named, *wild;

    //  Ownership
    openTable();
    login = add("/action/login", 0);
    named = add("/auth/upload", 1);
    wild = add("/action", 0);
    ttrue(websRouteServesAction(login, "login"), "a route runs the action it names");
    ttrue(!websRouteServesAction(login, "loginAudit"), "a route never runs an action it does not name");
    ttrue(websRouteServesAction(named, "upload"), "a protected route runs the action it names");
    ttrue(!websRouteServesAction(wild, "upload"), "an unnamed route must not run an action another route names");
    ttrue(!websRouteServesAction(wild, "login"), "an unnamed route must not run an action another route names");
    ttrue(websRouteServesAction(wild, "showTest"), "an unnamed route runs an action no route names");
    websCloseRoute();

    //  Lint: an authenticated route naming no action beside a public one protects nothing
    openTable();
    add("/admin/", 1);
    add("/action", 0);
    ttrue(check() >= 1, "an authenticated unnamed action route beside a public one must be reported");

    openTable();
    add("/admin/upload", 1);
    add("/admin/delete", 1);
    add("/action", 0);
    teqi(check(), 0, "protected actions named in their own routes must not be reported");
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication and logging are required");
    return 0;
}
#endif
