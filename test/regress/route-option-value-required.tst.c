/*
    Route, user and role options without a nonempty value must refuse the configuration.
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

#define ROUTES "route-option-value-required.tmp"

static void quiet(int level, cchar *msg)
{
}


static bool serve(Webs *wp)
{
    return 1;
}


static int load(cchar *table)
{
    FILE *fp;
    int  rc;

    websCloseRoute();
    ttrue(websOpenRoute() == 0, "open the route table");
    websDefineHandler("file", 0, serve, 0, 0);
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

    teqi(load("route uri=/admin/ auth=basic abilities=manage\nroute uri=/\n"), 0, "a complete table loads");

    ttrue(load("route uri=/admin/ auth abilities=manage\nroute uri=/\n") < 0,
          "auth without a value must refuse the table");
    ttrue(load("route uri=/admin/ auth=basic abilities=manage handler\nroute uri=/\n") < 0,
          "handler without a value must refuse the table");
    ttrue(load("route uri=/admin/ auth=basic abilities=\nroute uri=/\n") < 0,
          "an empty abilities value must refuse the table");
    ttrue(load("route uri=/admin/ auth=basic protocol\nroute uri=/\n") < 0,
          "protocol without a value must refuse the table");
    ttrue(load("user name=u password roles=user\n") < 0, "password without a value must refuse the table");
    ttrue(load("role name=r abilities=\n") < 0, "an empty role ability set must refuse the table");

    ttrue(load("route uri=/admin/ auth=basic extensions\nroute uri=/\n") < 0,
          "extensions without a value must refuse the table");
    ttrue(load("route uri=/admin/ auth=basic methods\nroute uri=/\n") < 0,
          "methods without a value must refuse the table");
    ttrue(load("route uri=/admin/ auth=basic abilities\nroute uri=/\n") < 0,
          "abilities without a value must refuse the table");
    ttrue(load("role name=r abilities\n") < 0, "role abilities without a value must refuse the table");

    ttrue(load("route uri=/admin/ auth=basic abilities=manage redirect\nroute uri=/\n") < 0,
          "redirect without a value must refuse the table");

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
