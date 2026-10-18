/* The server's own working directory is unchanged while a CGI request is being set up */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#include "cgi-handler-fixture.h"

#if ME_GOAHEAD_CGI && !VXWORKS
static char duringSetup[ME_GOAHEAD_LIMIT_FILENAME];


static void recordCwd(char **env)
{
    if (getcwd(duringSetup, sizeof(duringSetup)) == NULL) {
        duringSetup[0] = '\0';
    }
}
#endif

int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS
    Webs      wp;
    WebsRoute route;
    char      before[ME_GOAHEAD_LIMIT_FILENAME], after[ME_GOAHEAD_LIMIT_FILENAME];

    tnotnull(getcwd(before, sizeof(before)), "save the server directory");

    memset(&wp, 0, sizeof(wp));
    memset(&route, 0, sizeof(route));
    route.extensions = -1;
    route.dir = ME_WIN_LIKE ? "C:/" : "/";
    route.prefix = "/cgi-bin";
    route.prefixLen = slen(route.prefix);
    wp.route = &route;
    wp.path = "/cgi-bin/test.exe";
    wp.query = "key=value";
    wp.vars = wp.cgifd = -1;
    primaryMode = S_IFREG;
    inspectEnv = recordCwd;
    reportedCode = statCalls = tempCalls = 0;
    cgiHandler(&wp);
    inspectEnv = NULL;

    teq(tempCalls, 2, "the handler reached process setup");
    tmatch(duringSetup, before, "the server directory is not the CGI directory while the request is set up");
    tnotnull(getcwd(after, sizeof(after)), "read the server directory after the request");
    tmatch(after, before, "the server directory is unchanged after the request");
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
