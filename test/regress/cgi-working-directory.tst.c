/* Inject a failed cwd lookup without changing the test runner directory. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
static int failCwd, directoryChanges;
static char *checkedCwd(char *buf, size_t size)
{
    if (failCwd) { memset(buf, 'x', size); errno = ERANGE; return NULL; }
    scopy(buf, (ssize) size, "/saved");
    return buf;
}
static int checkedChdir(cchar *path) { directoryChanges++; return 0; }
#define getcwd checkedCwd
#define chdir checkedChdir
#include "cgi-handler-fixture.h"
#undef getcwd
#undef chdir
int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS
    Webs wp;
    WebsRoute route;
    int configured;
    for (configured = 0; configured < 2; configured++) {
        memset(&wp, 0, sizeof(wp));
        memset(&route, 0, sizeof(route));
        route.extensions = -1;
        wp.route = &route;
        route.prefix = "/cgi-bin";
        route.prefixLen = slen(route.prefix);
        route.dir = configured ? "/configured" : NULL;
        wp.path = "/cgi-bin/test.exe";
        wp.query = "key=value";
        wp.vars = wp.cgifd = -1;
        primaryMode = S_IFREG;
        failCwd = 1;
        directoryChanges = reportedCode = statCalls = tempCalls = 0;
        cgiHandler(&wp);
        teq(reportedCode, HTTP_CODE_INTERNAL_SERVER_ERROR, "failed cwd lookup rejects the request");
        teq(directoryChanges, 0, "failed cwd lookup cannot change directory");
        teq(statCalls, 0, "failed cwd lookup cannot resolve an executable");
        teq(tempCalls, 0, "failed cwd lookup cannot create staging files");
    }
    failCwd = 0;
    directoryChanges = 0;
    runHandler(-1);
    teq(tempCalls, 2, "successful lookup reaches normal CGI setup");
    teq(directoryChanges, 0, "successful setup never changes the working directory");
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
