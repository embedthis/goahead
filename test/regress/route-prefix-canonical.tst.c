/*
    The API and configuration loader must refuse noncanonical route prefixes that cannot match requests.
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

#define ROUTES "route-prefix-canonical.tmp"

static void quiet(int level, cchar *msg)
{
}


static bool serve(Webs *wp)
{
    return 1;
}


static void openTable(void)
{
    ttrue(websOpenRoute() == 0, "open the route table");
    websDefineHandler("file", 0, serve, 0, 0);
}


static int load(cchar *table)
{
    FILE *fp;
    int  rc;

    websCloseRoute();
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

    prior = logSetHandler(quiet);
    teqi(websOpen("../web", NULL), 0, "open the server");
    websCloseRoute();
    openTable();

    ttrue(websAddRoute("/", "file", -1) != 0, "the root prefix installs");
    ttrue(websAddRoute("/admin/", "file", -1) != 0, "a directory prefix installs");
    ttrue(websAddRoute("/action/login", "file", -1) != 0, "a file prefix installs");
    ttrue(websAddRoute("/caf\xc3\xa9/", "file", -1) != 0, "a composed non-ASCII prefix installs");

    ttrue(websAddRoute("admin/", "file", -1) == 0, "a prefix without a leading slash must be refused");
    ttrue(websAddRoute("/admin//sub/", "file", -1) == 0, "a prefix with an empty segment must be refused");
    ttrue(websAddRoute("/admin/./", "file", -1) == 0, "a prefix with a dot segment must be refused");
    ttrue(websAddRoute("/a/../b/", "file", -1) == 0, "a prefix with a parent segment must be refused");
    ttrue(websAddRoute("/%61dmin/", "file", -1) == 0, "a percent-encoded prefix must be refused");
    ttrue(websAddRoute("/caf" "e\xcc\x81/", "file", -1) == 0, "a decomposed non-ASCII prefix must be refused");

    //  The loader refuses the table rather than installing a dead authenticated route
    ttrue(load("route uri=admin/ auth=basic abilities=manage\nroute uri=/\n") < 0,
          "a table whose authenticated prefix lacks the leading slash must be refused");
    ttrue(load("route uri=/admin//sub/ auth=basic abilities=manage dir=private\n") < 0,
          "a table whose authenticated prefix has an empty segment must be refused");
    ttrue(load("route uri=/admin/ auth=basic abilities=manage\nroute uri=/\n") == 0,
          "a table with canonical prefixes loads");

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
