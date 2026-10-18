/* A match callback rewrite is routed from the first route in the table */

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
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH
static int declineCount;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static bool serveMatch(Webs *wp)
{
    return 1;
}

static bool rewriteMatch(Webs *wp)
{
    char *target;

    if (sstarts(wp->path, "/rw/")) {
        target = sclone(&wp->path[3]);
        websRewriteRequest(wp, target);
        wfree(target);
    }
    return 0;
}

static bool declineMatch(Webs *wp)
{
    declineCount++;
    return 0;
}

static WebsHash manageAbility(void)
{
    WebsHash abilities;

    abilities = hashCreate(-1);
    hashEnter(abilities, "manage", valueInteger(0), 0);
    return abilities;
}

static Webs *request(cchar *path, cchar *credentials)
{
    Webs *wp;
    char buf[64];
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->protocol = sclone("http");
    wp->url = sclone(path);
    wp->path = sclone(path);
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "127.0.0.1");
    if (credentials) {
        scopy(buf, sizeof(buf), credentials);
        wp->authType = sclone("basic");
        wp->authDetails = websEncode64(buf);
    }
    websRouteRequest(wp);
    return wp;
}

int main(int argc, char **argv)
{
    WebsRoute *admin, *catchAll;
    Webs      *wp;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    teq(websDefineHandler("serve", serveMatch, 0, 0, 0), 0, "define the serving handler");
    teq(websDefineHandler("rewrite", rewriteMatch, 0, 0, 0), 0, "define the rewriting handler");
    teq(websDefineHandler("decline", declineMatch, 0, 0, 0), 0, "define the declining handler");

    admin = websAddRoute("/admin/", "serve", 0);
    ttrue(admin != 0, "add the protected route");
    websSetRouteMatch(admin, 0, 0, -1, -1, manageAbility(), -1);
    teq(websSetRouteAuth(admin, "basic"), 0, "protect the route with Basic authentication");
    ttrue(websAddRoute("/rw/", "rewrite", -1) != 0, "add the rewriting route");
    ttrue(websAddRoute("/decline/", "decline", -1) != 0, "add the declining route");
    catchAll = websAddRoute("/", "serve", -1);
    ttrue(catchAll != 0, "add the catch-all route");

    ttrue(websAddRole("administrator", manageAbility()) != 0, "add the role");
    ttrue(websAddUser("joshua", "2fd6e47ff9bb70c0465fd2f5c8e5305e", "administrator") != 0, "add the user");
    websComputeAllUserAbilities();

    wp = request("/rw/admin/secret.html", NULL);
    ttrue(wp->route == 0, "a rewrite into the first route must not be served by a later route");
    teq(wp->code, HTTP_CODE_UNAUTHORIZED, "a rewrite into the first route must be challenged");
    websFree(wp);

    wp = request("/rw/admin/", NULL);
    ttrue(wp->route == 0, "a rewrite to exactly the protected prefix must not be served by a later route");
    teq(wp->code, HTTP_CODE_UNAUTHORIZED, "a rewrite to exactly the protected prefix must be challenged");
    websFree(wp);

    wp = request("/rw/admin", NULL);
    teqp(wp->route, catchAll, "a rewrite short of the protected prefix reaches the catch-all");
    websFree(wp);

    wp = request("/rw/admin/secret.html", "joshua:pass1");
    teqp(wp->route, admin, "valid credentials after a rewrite select the protected route");
    websFree(wp);

    wp = request("/rw/pub/page.html", NULL);
    teqp(wp->route, catchAll, "a rewrite to an unprotected path is served by the catch-all");
    websFree(wp);

    declineCount = 0;
    wp = request("/decline/page.html", NULL);
    teqp(wp->route, catchAll, "a decline without a rewrite continues to the catch-all");
    teq(declineCount, 1, "a decline without a rewrite does not restart the scan");
    websFree(wp);

    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication is not enabled");
    return 0;
}
#endif
