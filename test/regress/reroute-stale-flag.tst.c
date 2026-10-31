/*
    reroute-stale-flag.tst.c - A rewrite from a service callback is routed once, after which routing resumes
    normally. websRewriteRequest sets WEBS_REROUTE and the handler declines; websRouteRequest then rescans from the
    first route but leaves the flag set. When a later handler on the rewritten path declines from its service
    callback, as the upload filter does by design, the stale flag makes the rescan start from the first route again
    instead of resuming after the declining route. The declining route is re-offered until the route limit is
    exhausted, and the request fails with 404 before reaching the route behind the filter, denied or served.
 */

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
static int declines, served;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

//  Rewrite /svc/X to /X from the service callback and decline, as test.c's serviceRewrite probe does
static bool rewriteService(Webs *wp)
{
    char *target;

    if (sstarts(wp->path, "/svc/")) {
        target = sclone(&wp->path[4]);
        websRewriteRequest(wp, target);
        wfree(target);
    }
    return 0;
}


//  Decline every request from the service callback, as the upload filter does
static bool declineService(Webs *wp)
{
    declines++;
    return 0;
}


static bool serveService(Webs *wp)
{
    served++;
    return 1;
}


static WebsHash manageAbility(void)
{
    WebsHash abilities;

    abilities = hashCreate(-1);
    hashEnter(abilities, "manage", valueInteger(0), 0);
    return abilities;
}


//  Route and dispatch as websPump does, until a handler serves the request or routing refuses it
static Webs *request(cchar *path, cchar *credentials)
{
    Webs *wp;
    char buf[64];
    int  wid, i;

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
    declines = served = 0;
    websRouteRequest(wp);
    for (i = 0; i < 18 && wp->route && !wp->finalized; i++) {
        if (websRunRequest(wp)) {
            break;
        }
        websRouteRequest(wp);
    }
    return wp;
}


int main(int argc, char **argv)
{
    WebsRoute *protected;
    Webs      *wp;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    teq(websDefineHandler("rewrite", 0, rewriteService, 0, 0), 0, "define the rewriting handler");
    teq(websDefineHandler("decline", 0, declineService, 0, 0), 0, "define the declining handler");
    teq(websDefineHandler("serve", 0, serveService, 0, 0), 0, "define the serving handler");

    ttrue(websAddRoute("/svc/", "rewrite", -1) != 0, "add the rewriting route");
    ttrue(websAddRoute("/x/", "decline", -1) != 0, "add the declining filter route");
    protected = websAddRoute("/x/", "serve", -1);
    ttrue(protected != 0, "add the protected route behind the filter");
    websSetRouteMatch(protected, 0, 0, -1, -1, manageAbility(), -1);
    teq(websSetRouteAuth(protected, "basic"), 0, "protect the route with Basic authentication");

    ttrue(websAddRole("administrator", manageAbility()) != 0, "add the role");
    ttrue(websAddUser("joshua", "2fd6e47ff9bb70c0465fd2f5c8e5305e", "administrator") != 0, "add the user");
    websComputeAllUserAbilities();

    //  Direct requests: the filter declines once and the protected route decides
    wp = request("/x/secret.html", NULL);
    teq(declines, 1, "the filter declines a direct anonymous request once");
    teq(wp->code, HTTP_CODE_UNAUTHORIZED, "a direct anonymous request is challenged");
    websFree(wp);

    wp = request("/x/secret.html", "joshua:pass1");
    teq(served, 1, "a direct authorized request is served");
    websFree(wp);

    //  Rewritten requests must reach the same decision
    wp = request("/svc/x/secret.html", NULL);
    teq(declines, 1, "the filter declines a rewritten anonymous request once, not until the route limit");
    teq(wp->code, HTTP_CODE_UNAUTHORIZED, "a rewritten anonymous request is challenged, not refused with 404");
    websFree(wp);

    wp = request("/svc/x/secret.html", "joshua:pass1");
    teq(served, 1, "a rewritten authorized request is served");
    ttrue(wp->route == protected, "the rewritten authorized request is held by the protected route");
    websFree(wp);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Requires ME_GOAHEAD_AUTH");
    return 0;
}
#endif
