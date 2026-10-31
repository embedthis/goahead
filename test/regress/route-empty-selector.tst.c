/*
    route-empty-selector.tst.c - A methods= or extensions= selector that names nothing never matches, so the route is
    dead and an authenticated dead route leaves its requests to the broader routes after it. "none" is documented
    for abilities=, where it means no requirement, and is dropped from every list, so methods=none and
    extensions=none leave an empty set. websSetRouteMatch already refuses a selector spelling that can never match;
    an empty selector set is the remaining spelling and must be refused the same way. abilities=none stays accepted.
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

#define ROUTES "route-empty-selector.tmp"

static void quiet(int level, cchar *msg)
{
}


static bool serve(Webs *wp)
{
    return 1;
}


static void openTable(void)
{
    websCloseRoute();
    ttrue(websOpenRoute() == 0, "open the route table");
    websDefineHandler("file", 0, serve, 0, 0);
}


static int load(cchar *table)
{
    FILE *fp;
    int  rc;

    openTable();
    fp = fopen(ROUTES, "w");
    ttrue(fp != 0, "create the route file");
    fputs(table, fp);
    fclose(fp);
    rc = websLoad(ROUTES);
    unlink(ROUTES);
    return rc;
}


int main(int argc, char **argv)
{
    WebsLogHandler prior;
    WebsRoute      *route;

    prior = logSetHandler(quiet);
    teqi(websOpen("../web", NULL), 0, "open the server");

    teqi(load("route uri=/admin/ auth=basic abilities=none\nroute uri=/\n"), 0, "abilities=none is no requirement");
    ttrue(load("route uri=/admin/ methods=none auth=basic abilities=manage\nroute uri=/\n") < 0,
          "methods=none never matches and must refuse the table");
    ttrue(load("route uri=/admin/ extensions=none auth=basic abilities=manage\nroute uri=/\n") < 0,
          "extensions=none never matches and must refuse the table");
    ttrue(load("route uri=/admin/ methods=, auth=basic abilities=manage\nroute uri=/\n") < 0,
          "an empty methods list never matches and must refuse the table");

    openTable();
    route = websAddRoute("/admin/", "file", -1);
    ttrue(route != 0, "add the route");
    ttrue(websSetRouteMatch(route, 0, 0, hashCreate(-1), -1, -1, -1) < 0,
          "an empty methods set must be refused when set programmatically");

    websCloseRoute();
    logSetHandler(prior);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH and ME_GOAHEAD_LOGGING");
    return 0;
}
#endif
