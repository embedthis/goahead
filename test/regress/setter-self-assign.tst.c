/* setters must copy the new value before freeing the old one. */

#include "testme.h"
#include "goahead.h"

static Webs *allocWebs(void)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    return wp;
}


static void freeWebs(Webs *wp)
{
    wfree(wp->url);
    wfree(wp->path);
    wfree(wp->ext);
    wfree(wp->filename);
    wfree(wp->username);
    wfree(wp->password);
    wfree(wp);
}


/********************************* Suite A: websRewriteRequest *****************/

static void suiteA_rewrite(void)
{
    Webs *wp;

    tinfo("\n--- Suite A: websRewriteRequest with its own URL ---\n");

    wp = allocWebs();
    wp->url = sclone("/dir/page.html?a=1");
    ttrue(websRewriteRequest(wp, wp->url) == 0, "rewriting to the current URL must succeed");
    ttrue(wp->url && smatch(wp->url, "/dir/page.html?a=1"), "the URL must be unchanged");
    ttrue(wp->path && smatch(wp->path, "/dir/page.html"), "the path must be derived from the URL");
    freeWebs(wp);
}


/********************************* Suite B: websLoginUser **********************/

#if ME_GOAHEAD_AUTH
static char seenUser[32];
static char seenPassword[32];

static bool recordAndRefuse(Webs *wp)
{
    scopy(seenUser, sizeof(seenUser), wp->username ? wp->username : "");
    scopy(seenPassword, sizeof(seenPassword), wp->password ? wp->password : "");
    return 0;
}


static void suiteB_login(void)
{
    WebsRoute route;
    Webs      *wp;

    tinfo("\n--- Suite B: websLoginUser with the current username and password ---\n");

    memset(&route, 0, sizeof(route));
    route.verify = recordAndRefuse;

    wp = allocWebs();
    wp->route = &route;
    wp->username = sclone("ralph");
    wp->password = sclone("secret");
    ttrue(websLoginUser(wp, wp->username, wp->password) == 0, "the verifier refuses the login");
    ttrue(smatch(seenUser, "ralph"), "the verifier must see the username intact");
    ttrue(smatch(seenPassword, "secret"), "the verifier must see the password intact");
    wp->route = 0;
    freeWebs(wp);
}
#endif


int main(int argc, char **argv)
{
    tinfo("setter-self-assign: setters must copy the new value before freeing the old one\n");

    suiteA_rewrite();
#if ME_GOAHEAD_AUTH
    suiteB_login();
#endif
    return 0;
}
