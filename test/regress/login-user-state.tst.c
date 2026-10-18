/* websLoginUser must verify each call from a clean state. */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH

static int sawEncoded;
static int sawDigest;

static bool recordState(Webs *wp)
{
    sawEncoded = wp->encoded;
    sawDigest = wp->digest != 0;
    return 0;
}


static Webs *allocWebs(WebsRoute *route)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->route = route;
    return wp;
}


static void freeWebs(Webs *wp)
{
    wfree(wp->username);
    wfree(wp->password);
    wfree(wp->digest);
    wfree(wp);
}


static void suiteA_cleanState(void)
{
    WebsRoute route;
    Webs      *wp;

    tinfo("\n--- Suite A: the verifier sees no cached state ---\n");

    memset(&route, 0, sizeof(route));
    route.verify = recordState;
    wp = allocWebs(&route);
    wp->encoded = 1;
    wp->digest = sclone("cached-digest");

    websLoginUser(wp, "ralph", "secret");
    ttrue(sawEncoded == 0, "the password must not be treated as already encoded");
    ttrue(sawDigest == 0, "a cached Digest must not be compared against a plaintext password");

    wp->route = 0;
    freeWebs(wp);
}


static bool refuseAfterLookup(Webs *wp)
{
    //  A real verifier resolves the user record before comparing passwords
    wp->user = (WebsUser*) wp;
    return 0;
}


static void suiteB_failedLogin(void)
{
    WebsRoute route;
    Webs      *wp;

    tinfo("\n--- Suite B: a failed login leaves no claimed user ---\n");

    memset(&route, 0, sizeof(route));
    route.verify = refuseAfterLookup;
    wp = allocWebs(&route);

    ttrue(websLoginUser(wp, "admin", "wrong") == 0, "the verifier refuses the login");
    ttrue(websGetUsername(wp) == 0, "a failed login must not leave the claimed username");
    ttrue(wp->user == 0, "a failed login must not leave a user record");
    ttrue(wp->password == 0, "a failed login must not keep the password");

    wp->route = 0;
    freeWebs(wp);
}
#endif


int main(int argc, char **argv)
{
    tinfo("login-user-state: websLoginUser must verify each call from a clean state\n");
#if ME_GOAHEAD_AUTH
    suiteA_cleanState();
    suiteB_failedLogin();
#endif
    return 0;
}
