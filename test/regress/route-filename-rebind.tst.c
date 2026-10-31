/* A route serves its own directory, not the directory of an earlier route whose handler declined */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define PRIVATE_DIR "/private-root"

static char *servedFilename;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static bool declineService(Webs *wp)
{
    return 0;
}

static bool probeService(Webs *wp)
{
    wfreeset(servedFilename);
    servedFilename = sclone(wp->filename);
    return 1;
}

int main(int argc, char **argv)
{
    WebsRoute *declining, *catchAll;
    Webs      *wp;
    char      *expected;
    int       wid;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    teq(websDefineHandler("declineSvc", 0, declineService, 0, 0), 0, "define the declining service");
    teq(websDefineHandler("probeSvc", 0, probeService, 0, 0), 0, "define the probing service");

    declining = websAddRoute("/d/", "declineSvc", -1);
    ttrue(declining != 0, "add the declining route");
    teq(websSetRouteMatch(declining, PRIVATE_DIR, 0, -1, -1, -1, -1), 0, "give the declining route its own dir=");
    catchAll = websAddRoute("/", "probeSvc", -1);
    ttrue(catchAll != 0, "add the catch-all route");

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->protocol = sclone("http");
    wp->url = sclone("/d/f.html");
    wp->path = sclone("/d/f.html");
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "127.0.0.1");

    websRouteRequest(wp);
    teqp(wp->route, declining, "the dir= route is selected first");
    ttrue(!websRunRequest(wp), "its service declines");
    ttrue(sstarts(wp->filename, PRIVATE_DIR), "the declining route bound the file name to its directory");
    websRouteRequest(wp);
    teqp(wp->route, catchAll, "routing resumes at the catch-all");
    ttrue(websRunRequest(wp), "the catch-all serves");
    expected = sfmt("%s%s", websGetDocuments(), "/d/f.html");
    tmatch(servedFilename, expected, "the catch-all serves from its own directory, not the declining route's");
    wfree(expected);

    websFree(wp);
    websClose();
    return 0;
}
