/*
    assert-guards.tst.c - Request validation works with assertions disabled
 */

#define ME_GOAHEAD_DEBUG 1
#define ME_GOAHEAD_ASSERT_ABORT 1

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
#include "../../src/route.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

#if ME_UNIX_LIKE && ME_GOAHEAD_LOGGING
#include <sys/wait.h>

static bool aborts(bool cond)
{
    pid_t pid;
    int   status;

    if ((pid = fork()) == 0) {
        assert(cond);
        _exit(0);
    }
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}
#endif


static Webs *unparsed(void)
{
    int wid;

    wid = websAlloc(-1);
    return webs[wid];
}


int main(int argc, char **argv)
{
    Webs *wp;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    tinfo("\n--- Suite A: assert aborts ---\n");
#if ME_UNIX_LIKE && ME_GOAHEAD_LOGGING
    ttrue(aborts(0), "a failed assert must abort with ME_GOAHEAD_ASSERT_ABORT");
    ttrue(!aborts(1), "a passing assert must not abort");
#elif ME_GOAHEAD_LOGGING
    tinfo("abort is checked on POSIX systems only");
#else
    tinfo("assert is compiled out without ME_GOAHEAD_LOGGING");
#endif

    tinfo("\n--- Suite B: incomplete requests ---\n");
    ttrue(websOpenRoute() == 0, "open routes");
    wp = unparsed();
    websRouteRequest(wp);
    teq(wp->code, HTTP_CODE_BAD_REQUEST, "routing a request with no request line must answer 400");
    websFree(wp);

    wp = unparsed();
    ttrue(websRunRequest(wp), "running a request with no request line must be handled");
    teq(wp->code, HTTP_CODE_BAD_REQUEST, "and answered 400");
    websFree(wp);
    websCloseRoute();
    return 0;
}
