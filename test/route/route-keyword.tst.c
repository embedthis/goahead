/*
    route-keyword.tst.c - Regression: a mistyped route keyword must refuse the configuration.
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
static void captureLog(int level, cchar *msg)
{
}
#endif

static int loadConfig(cchar *contents)
{
    cchar *path = "route-keyword.tmp";
    FILE  *fp;
    int   rc;
#if ME_GOAHEAD_LOGGING
    WebsLogHandler priorHandler;
#endif

    if ((fp = fopen(path, "w")) == 0) {
        ttrue(0, "cannot write %s", path);
        return 0;
    }
    fputs(contents, fp);
    fclose(fp);

    ttrue(websOpenRoute() == 0, "websOpenRoute failed");
#if ME_GOAHEAD_AUTH
    ttrue(websOpenAuth(1) == 0, "websOpenAuth failed");
#endif
#if ME_GOAHEAD_LOGGING
    priorHandler = logSetHandler(captureLog);
#endif
    rc = websLoad(path);
#if ME_GOAHEAD_LOGGING
    logSetHandler(priorHandler);
#endif
#if ME_GOAHEAD_AUTH
    websCloseAuth();
#endif
    websCloseRoute();
    unlink(path);
    return rc;
}


int main(int argc, char **argv)
{
    tinfo("route-keyword: a mistyped keyword must refuse the configuration\n");

    ttrue(loadConfig("route uri=/ handler=continue\n") == 0, "a valid route must load");

    ttrue(loadConfig("route uri=/admin/ abilties=manage handler=continue\n") < 0,
          "a mistyped abilities keyword must refuse the load");

    ttrue(loadConfig("route uri=/admin/ athu=basic handler=continue\n") < 0,
          "a mistyped auth keyword must refuse the load");

    ttrue(loadConfig("route uri=/ handler=continue nonsense=1\n") < 0,
          "an unknown route keyword must refuse the load");

#if ME_GOAHEAD_AUTH
    ttrue(loadConfig("user name=joe password=x roles=manage\n") == 0, "a valid user must load");
    ttrue(loadConfig("user name=joe password=x role=manage\n") < 0,
          "a mistyped user keyword must refuse the load");

    ttrue(loadConfig("role name=manager abilities=manage\n") == 0, "a valid role must load");
    ttrue(loadConfig("role name=manager ability=manage\n") < 0,
          "a mistyped role keyword must refuse the load");
#endif

    ttrue(loadConfig("frobnicate uri=/\n") < 0, "an unknown directive must still refuse the load");


    ttrue(loadConfig("route uri=/a/ methods=GET|POST protocol=https extensions=jst,asp handler=continue\n") == 0,
          "canonical selector values must load");
    ttrue(loadConfig("route uri=/a/ methods=get handler=continue\n") < 0, "methods=get must refuse the load");
    ttrue(loadConfig("route uri=/a/ methods=GET|Post handler=continue\n") < 0, "methods=Post must refuse the load");
    ttrue(loadConfig("route uri=/a/ protocol=HTTP handler=continue\n") < 0, "protocol=HTTP must refuse the load");
    ttrue(loadConfig("route uri=/a/ protocol=htps handler=continue\n") < 0, "protocol=htps must refuse the load");
    ttrue(loadConfig("route uri=/ extensions=JST handler=continue\n") < 0, "extensions=JST must refuse the load");
    ttrue(loadConfig("route uri=/ extensions=.jst handler=continue\n") < 0, "extensions=.jst must refuse the load");

    return 0;
}
