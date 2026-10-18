/* The CGI handler never changes the server's working directory, on success or on any failure path. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
static int changes;
static int recordChdir(cchar *path) { changes++; return 0; }
#undef ME_GOAHEAD_LIMIT_CGI_ARGS
#define ME_GOAHEAD_LIMIT_CGI_ARGS 20
#define chdir recordChdir
#include "cgi-handler-fixture.h"
#undef chdir
int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS
    Webs wp;
    WebsRoute route;
    cchar *queries[] = { "x=y", "x=y", "x=y", "%3D", "-bad",
        "a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a+a", "x=y" };
    int i;
    for (i = 0; i < 7; i++) {
        memset(&wp, 0, sizeof(wp));
        memset(&route, 0, sizeof(route));
        route.extensions = -1;
        route.dir = "/configured";
        route.prefix = "/cgi-bin";
        route.prefixLen = slen(route.prefix);
        wp.route = &route;
        //  Not executable: access fails on POSIX, and Windows runs only .exe programs
        wp.path = i == 1 ? "/cgi-bin/tool.bat" : "/cgi-bin/tool.exe";
        wp.query = (char*) queries[i];
        wp.vars = wp.cgifd = -1;
        primaryMode = i == 0 ? 0 : S_IFREG;
        fallbackMode = 0;
        fixtureAccessError = i == 1;
        fixtureArgAllocError = i == 2;
        changes = reportedCode = statCalls = tempCalls = 0;
        cgiHandler(&wp);
        teq(changes, 0, "the handler never changes the server directory");
        if (i < 6) teq(tempCalls, 0, "early rejection never reaches spawn setup");
        else teq(tempCalls, 2, "normal setup reaches final cleanup");
    }
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
