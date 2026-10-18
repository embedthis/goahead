/* A websGetSessionVar value stays valid until the request ends */

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
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static bool acceptAll(Webs *wp)
{
    return 1;
}


static Webs *allocRequest(WebsRoute *route)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    ttrue(wid >= 0, "allocate a request in the connection list");
    wp = webs[wid];
    wp->route = route;
    return wp;
}


static void readThenRotate(WebsRoute *route, cchar *how)
{
    Webs        *wp;
    WebsSession *first;
    cchar       *value;
    int         base;

    base = websGetSessionCount();
    wp = allocRequest(route);
    first = websGetSession(wp, 1);
    ttrue(first != 0, "create a session");
    websSetSessionVar(wp, "next", "/target");
    value = websGetSessionVar(wp, "next", 0);
    ttrue(smatch(value, "/target"), "read the session variable");

    if (smatch(how, "login")) {
        ttrue(websLoginUser(wp, "user", "pass"), "log in");
        teq(websGetSessionCount(), base + 1, "login must replace the session, not add one");
    } else if (smatch(how, "rotate")) {
        ttrue(websRotateSession(wp) != 0, "rotate the session");
        teq(websGetSessionCount(), base + 1, "rotation must replace the session, not add one");
    } else {
        websDestroySession(wp);
        teq(websGetSessionCount(), base, "destroying must remove the session from the store");
    }
    ttrue(wp->session != first, "after %s the request must not use the old session", how);
    ttrue(smatch(value, "/target"), "a value read before %s must still be valid", how);

    //  A second rotation in the same request releases the first retired session
    if (smatch(how, "rotate")) {
        ttrue(websRotateSession(wp) != 0, "rotate again");
        teq(websGetSessionCount(), base + 1, "a second rotation must not add a session");
    }
    websFree(wp);
    teq(websGetSessionCount(), smatch(how, "destroy") ? base : base + 1,
        "ending the request must keep only the live session");
}


int main(int argc, char **argv)
{
    WebsRoute *route;
    int       i;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    if (websOpen("../web", NULL) < 0) {
        tfail("cannot open the server");
        return 1;
    }
    route = walloc(sizeof(WebsRoute));
    memset(route, 0, sizeof(WebsRoute));
    route->verify = acceptAll;

    for (i = 0; i < 3; i++) {
        readThenRotate(route, "login");
        readThenRotate(route, "rotate");
        readThenRotate(route, "destroy");
    }
    wfree(route);
    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication is not in this build");
    return 0;
}
#endif
