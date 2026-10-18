/* A build without TLS warns for each route that authenticates, as its credentials cross the network in clear */

#define ME_COM_SSL 0

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

static int  warnings;
static char lastMessage[512];

static void captureLog(int level, cchar *msg)
{
    if (msg && strstr(msg, "Warning:")) {
        warnings++;
        scopy(lastMessage, sizeof(lastMessage), msg);
    }
}


static void checkAuth(cchar *uri, cchar *type)
{
    char expected[256];
    int  rc;

    warnings = 0;
    lastMessage[0] = '\0';
    rc = websSetRouteAuth(websAddRoute(uri, "continue", -1), type);
    teq(rc, 0, "set %s authentication", type);
    teq(warnings, 1, "a %s route warns once", type);
    fmt(expected, sizeof(expected), "Route \"%s\" uses %s authentication but this build has no TLS", uri, type);
    ttrue(strstr(lastMessage, expected) != 0, "the warning names the route and type: %s", lastMessage);
    ttrue(strstr(lastMessage, "unencrypted") != 0, "and says the credentials are unencrypted");
}


int main(int argc, char **argv)
{
    ttrue(websOpenRoute() == 0, "open routes");
    logSetHandler(captureLog);

    checkAuth("/basic/", "basic");
#if ME_GOAHEAD_DIGEST
    checkAuth("/digest/", "digest");
#endif
    checkAuth("/form/", "form");

    warnings = 0;
    ttrue(websSetRouteAuth(websAddRoute("/unknown/", "continue", -1), "bogus") < 0, "an unknown type is refused");
    teq(warnings, 0, "without a cleartext warning");

    websCloseRoute();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication and logging are required");
    return 0;
}
#endif
