/* a Digest route is refused when its password store is PAM. */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_DIGEST && ME_COMPILER_HAS_PAM

static WebsRoute *allocRoute(WebsVerify verify)
{
    WebsRoute *route;

    route = walloc(sizeof(WebsRoute));
    memset(route, 0, sizeof(WebsRoute));
    route->prefix = sclone("/protected/");
    route->verify = verify;
    return route;
}


static void freeRoute(WebsRoute *route)
{
    wfree(route->prefix);
    wfree(route->authType);
    wfree(route);
}


static void suiteA_refused(void)
{
    WebsRoute *route;

    tinfo("\n--- Suite A: digest on the PAM store is refused ---\n");
    route = allocRoute(websVerifyPasswordFromPam);
    ttrue(websSetRouteAuth(route, "digest") < 0, "a Digest route on the PAM store must be refused");
    ttrue(route->authType == 0, "a refused route must not be marked as authenticated");
    freeRoute(route);
}


static void suiteB_accepted(void)
{
    WebsRoute *route;

    tinfo("\n--- Suite B: other combinations are accepted ---\n");
    route = allocRoute(websVerifyPasswordFromFile);
    ttrue(websSetRouteAuth(route, "digest") == 0, "a Digest route on the file store must be accepted");
    freeRoute(route);

    route = allocRoute(websVerifyPasswordFromPam);
    ttrue(websSetRouteAuth(route, "basic") == 0, "a Basic route on the PAM store must be accepted");
    freeRoute(route);
}
#endif


int main(int argc, char **argv)
{
    tinfo("digest-pam-refused: a Digest route is refused when its password store is PAM\n");
#if ME_GOAHEAD_AUTH && ME_GOAHEAD_DIGEST && ME_COMPILER_HAS_PAM
    suiteA_refused();
    suiteB_accepted();
#endif
    return 0;
}
