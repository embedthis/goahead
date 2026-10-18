/*
    cookie-scope-validation.tst.c - Cookie scope and attribute validation
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static Webs *allocWebs(cchar *host)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->host = sclone(host);
    wp->responseCookies = hashCreate(7);
    return wp;
}


static void freeWebs(Webs *wp)
{
    hashFree(wp->responseCookies);
    wfree(wp->host);
    wfree(wp);
}


static cchar *cookieFor(Webs *wp, cchar *name)
{
    WebsKey *kp;

    if ((kp = hashLookup(wp->responseCookies, name)) == 0) {
        return 0;
    }
    return kp->content.value.string;
}


static void suiteA_hostOnly(void)
{
    Webs  *wp;
    cchar *cookie;

    tinfo("\n--- Suite A: no domain argument makes a host-only cookie ---\n");

    wp = allocWebs("example.com:8080");
    websSetCookie(wp, "-goahead-session-", "::webs.session::abc+/=", "/", NULL, 0, WEBS_COOKIE_HTTP);
    cookie = cookieFor(wp, "-goahead-session-");
    ttrue(cookie != 0, "the cookie must be set");
    if (cookie) {
        ttrue(strstr(cookie, "domain") == 0, "a cookie set with no domain must carry no domain attribute");
        ttrue(sstarts(cookie, "-goahead-session-=::webs.session::abc+/=; path=/"),
              "the name, value and path must be written as given");
    }
    freeWebs(wp);

    wp = allocWebs("evil.example;samesite=none");
    websSetCookie(wp, "id", "1", "/", NULL, 0, 0);
    cookie = cookieFor(wp, "id");
    ttrue(cookie != 0, "the cookie must be set");
    if (cookie) {
        ttrue(strstr(cookie, "evil") == 0, "nothing from the Host header may appear in the cookie");
    }
    freeWebs(wp);
}


static void suiteB_explicitDomain(void)
{
    Webs  *wp;
    cchar *cookie;

    tinfo("\n--- Suite B: an explicit domain is written as given ---\n");

    wp = allocWebs("www.example.com");
    websSetCookie(wp, "id", "1", "/app", "example.com", 0, 0);
    cookie = cookieFor(wp, "id");
    ttrue(cookie != 0, "the cookie must be set");
    if (cookie) {
        ttrue(smatch(cookie, "id=1; path=/app; domain=example.com"), "the explicit domain must be written");
    }
    freeWebs(wp);
}


static void suiteC_refused(void)
{
    Webs *wp;
    int  i;
    struct {
        cchar *name, *value, *path, *domain, *why;
    } cases[] = {
        { "id", "a;domain=evil.example", "/", NULL, "a ';' in the value" },
        { "id", "a b", "/", NULL, "white space in the value" },
        { "id", "a,b", "/", NULL, "a ',' in the value" },
        { "id", "\"a\"", "/", NULL, "a '\"' in the value" },
        { "id", "a\r\nX-Injected: 1", "/", NULL, "CR LF in the value" },
        { "id", "a", "/;domain=evil.example", NULL, "a ';' in the path" },
        { "id", "a", "/", "example.com;secure", "a ';' in the domain" },
        { "id", "a", "/", "example .com", "white space in the domain" },
        { "i;d", "a", "/", NULL, "a ';' in the name" },
        { "i=d", "a", "/", NULL, "a '=' in the name" },
        { 0 }
    };

    tinfo("\n--- Suite C: components that would end the cookie early are refused ---\n");

    for (i = 0; cases[i].name; i++) {
        wp = allocWebs("example.com");
        websSetCookie(wp, cases[i].name, cases[i].value, cases[i].path, cases[i].domain, 0, 0);
        ttrue(cookieFor(wp, cases[i].name) == 0, "a cookie with %s must not be set", cases[i].why);
        freeWebs(wp);
    }
}


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    suiteA_hostOnly();
    suiteB_explicitDomain();
    suiteC_refused();
    return 0;
}
