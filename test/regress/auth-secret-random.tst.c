/*
    auth-secret-random.tst.c - Authentication does not start without a random secret
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE
#include "fd-exhaust-fixture.h"
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

int main(int argc, char **argv)
{
#if ME_UNIX_LIKE && ME_GOAHEAD_AUTH
    Exhaust ex;
    int     rc;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "initialize the server");
    websCloseAuth();

    exhaustDescriptors(&ex);
    rc = websOpenAuth(0);
    releaseDescriptors(&ex);
    ttrue(rc < 0, "authentication must not start when the random source cannot be read");

    websCloseAuth();
    teq(websOpenAuth(0), 0, "authentication must start once the random source is available");
    websClose();
#else
    tskip("POSIX build with authentication only");
#endif
    return 0;
}
