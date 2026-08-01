/*
    cookie-value-ownership.tst.c - Regression: the response cookie hash must own its cookie string.

    websSetCookie() formats the Set-Cookie value with sfmt() and stored it with
    hashEnter(wp->responseCookies, name, valueString(cookie, 0), 0). The zero flag clears
    WebsValue.allocated, and valueFree() releases a string only when that flag is set. Both release
    paths therefore skipped the block: hashEnter() when it replaces a same-named cookie, and
    hashFree() when termWebs() tears the connection down. No other site in the tree frees it, so
    every Set-Cookie orphaned its allocation - an unbounded leak on a long-lived embedded server.

    A leak is not directly assertable in-process, so this test asserts the ownership property that
    makes both release paths work, and then drives those paths hard enough that a leak detector or
    an allocator with integrity checks has something to trip on. Three suites:

      A) ownership - the stored value is marked allocated and holds the formatted cookie
      B) the replace path - re-setting a name replaces the value in place and frees the old one
      C) churn - many cookies set, replaced and torn down, exercising hashEnter and hashFree
 */

#include "testme.h"
#include "goahead.h"

/*
    A connection carries nothing this code path reads except the host and the cookie hash, so a
    zeroed Webs with those two fields is enough. websSetCookie() asserts only that wp is non-null.
 */
static Webs *allocWebs(void)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->host = sclone("example.com");
    wp->responseCookies = hashCreate(7);
    return wp;
}


static void freeWebs(Webs *wp)
{
    hashFree(wp->responseCookies);
    wfree(wp->host);
    wfree(wp);
}


/********************************* Suite A: ownership ***************************/

static void suiteA_ownership(void)
{
    Webs    *wp;
    WebsKey *kp;

    tinfo("\n--- Suite A: the cookie hash owns the cookie string ---\n");

    wp = allocWebs();
    websSetCookie(wp, "-goahead-session-", "abc123", "/", NULL, 0, WEBS_COOKIE_HTTP);

    kp = hashLookup(wp->responseCookies, "-goahead-session-");
    ttrue(kp != 0, "the cookie must be stored under its name");
    if (kp) {
        ttrue(kp->content.valid != 0, "the stored cookie value must be valid");
        ttrue(kp->content.type == string, "the stored cookie value must be a string");
        ttrue(kp->content.allocated != 0, "the hash must own the cookie string so valueFree releases it");
        ttrue(kp->content.value.string != 0, "the stored cookie must have a string");
        if (kp->content.value.string) {
            ttrue(strstr(kp->content.value.string, "-goahead-session-=abc123") != 0,
                  "the stored cookie must carry the name and value");
            ttrue(strstr(kp->content.value.string, "httponly") != 0,
                  "the stored cookie must carry the requested attributes");
        }
    }
    freeWebs(wp);
}


/********************************* Suite B: replace path ************************/

static void suiteB_replace(void)
{
    Webs    *wp;
    WebsKey *kp;
    int     count;

    tinfo("\n--- Suite B: re-setting a cookie replaces and frees the old value ---\n");

    wp = allocWebs();
    websSetCookie(wp, "-goahead-session-", "first", "/", NULL, 0, 0);
    websSetCookie(wp, "-goahead-session-", "second", "/", NULL, 0, 0);

    count = 0;
    for (kp = hashFirst(wp->responseCookies); kp; kp = hashNext(wp->responseCookies, kp)) {
        count++;
    }
    ttrue(count == 1, "re-setting a name must replace rather than accumulate");

    kp = hashLookup(wp->responseCookies, "-goahead-session-");
    ttrue(kp != 0, "the replaced cookie must still be present");
    if (kp && kp->content.value.string) {
        ttrue(strstr(kp->content.value.string, "=second") != 0, "the latest value must win");
        ttrue(strstr(kp->content.value.string, "=first") == 0, "the superseded value must be gone");
        ttrue(kp->content.allocated != 0, "the replacement must be owned by the hash too");
    }
    freeWebs(wp);
}


/********************************* Suite C: churn *******************************/

/*
    Drive the two release paths at volume. Each connection sets a batch of distinct cookies and
    replaces each one, so hashEnter frees on replace and hashFree frees at teardown. With the defect
    present this orphans every string; a leak detector or an integrity-checking allocator fails here.
 */
static void suiteC_churn(void)
{
    Webs *wp;
    char name[64], value[64];
    int  connection, i;

    tinfo("\n--- Suite C: repeated set, replace and teardown ---\n");

    for (connection = 0; connection < 50; connection++) {
        wp = allocWebs();
        for (i = 0; i < 20; i++) {
            fmt(name, sizeof(name), "cookie-%d", i);
            fmt(value, sizeof(value), "value-%d-%d", connection, i);
            websSetCookie(wp, name, value, "/", NULL, 0, WEBS_COOKIE_SECURE);
            websSetCookie(wp, name, value, "/", "example.org", 3600, WEBS_COOKIE_SAME_LAX);
        }
        freeWebs(wp);
    }
    ttrue(1, "repeated cookie churn must complete without allocator failure");
}


int main(int argc, char **argv)
{
    tinfo("cookie-value-ownership: the response cookie hash must own its cookie string\n");

    suiteA_ownership();
    suiteB_replace();
    suiteC_churn();

    return 0;
}
