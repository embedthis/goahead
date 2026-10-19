/*
    WebsTime is 64 bits on every target, all expiry arithmetic uses the one websTime() clock, and a failed or
    unrepresentable clock refuses sessions and Digest nonces instead of keeping them forever.

    The clock is replaced by a controllable one so the test can cross 2038 and fail the clock on a 64-bit host. A
    32-bit build runs the same code with the post-2038 steps skipped, because its time_t cannot hold them.
 */

#include "testme.h"
#include "goahead.h"

static int64 mockNow;                   /* Seconds the clock reports. Zero for the real clock */
static int   mockFail;                  /* Nonzero when the clock fails */

static time_t testTime(time_t *out)
{
    time_t now;

    now = mockFail ? (time_t) -1 : (mockNow ? (time_t) mockNow : time(0));
    if (out) {
        *out = now;
    }
    return now;
}

#define time(t) testTime(t)

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/time.c"
#include "../../src/http.c"
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define BASE_2038   ((int64) 0x7fffff00)    /* A few seconds before the 32-bit time_t limit */
#define WIDE_TIME   (sizeof(time_t) >= 8)

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif


static Webs *newWebs(cchar *cookie)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->cookie = cookie ? sclone(cookie) : 0;
    return wp;
}


static Webs *presenting(WebsSession *sp)
{
    Webs *wp;
    char *cookie;

    cookie = sfmt("%s=%s", WEBS_SESSION, sp->id);
    wp = newWebs(cookie);
    wfree(cookie);
    return wp;
}


static void testWidth(void)
{
    WebsTime now;
    char     *date;

    tinfo("\n--- WebsTime is 64 bits and websTime() agrees with the host clock ---\n");
    teqi((int) sizeof(WebsTime), 8, "WebsTime must be 64 bits regardless of time_t");
    ttrue((WebsTime) -1 < 0, "WebsTime must be signed");

    mockNow = 0;
    mockFail = 0;
    now = websTime();
    ttrue(now > 0, "websTime must return the current time");
    ttrue(now >= (int64) time(0) - 1 && now <= (int64) time(0) + 1, "websTime must agree with time(0)");

    mockFail = 1;
    teqll((int64) websTime(), -1, "a failed clock must report -1");
    mockFail = 0;
    teqll((int64) websTime(), (int64) now, "the clock must recover when the failure clears");


    teqll((int64) toTimeT(1700000000), 1700000000, "an in-range time converts unchanged");
    if (WIDE_TIME) {
        teqll((int64) toTimeT(INT64(5000000000)), INT64(5000000000), "a wide time_t keeps a post-2038 time");
        date = formatHttpDate(INT64(4102444800));
        tcontains(date, "Fri, 01 Jan 2100 00:00:00 GMT", "a post-2038 date formats");
        wfree(date);
    } else {
        teqll((int64) toTimeT(INT64(5000000000)), (int64) INT_MAX, "a 32-bit time_t saturates");
    }
}


static void testSessions(void)
{
    WebsSession *sp, *found;
    Webs        *owner, *wp;
    int         base;

    tinfo("\n--- Sessions expire exactly one lifespan after use, across the 32-bit limit ---\n");
    owner = newWebs(0);
    base = websGetSessionCount();

    mockNow = BASE_2038;
    sp = websAllocSession(owner, NULL, 600);
    ttrue(sp != 0, "allocate a session");
    teqll((int64) sp->expires, BASE_2038 + 600, "the expiry is the clock plus the lifespan");
    ttrue(sp->expires > INT64(0x7fffffff), "the expiry must not wrap at the 32-bit limit");

    if (WIDE_TIME) {
        mockNow = BASE_2038 + 599;
        wp = presenting(sp);
        found = websGetSession(wp, 0);
        ttrue(found == sp, "a session inside its lifespan is returned");
        teqll((int64) sp->expires, BASE_2038 + 599 + 600, "use refreshes the expiry from the same clock");
        websFree(wp);

        mockNow = BASE_2038 + 599 + 600;
        wp = presenting(sp);
        ttrue(websGetSession(wp, 0) == 0, "a session past 2038 must expire after its lifespan");
        teq(websGetSessionCount(), base, "the expired session must be removed");
        websFree(wp);
    } else {
        mockNow = BASE_2038;
        wp = presenting(sp);
        websGetSession(wp, 0);
        websFree(wp);
    }

    tinfo("\n--- A failed clock refuses sessions ---\n");
    mockNow = 0;
    sp = websAllocSession(owner, NULL, 600);
    ttrue(sp != 0, "allocate a session on a working clock");
    mockFail = 1;
    base = websGetSessionCount();
    ttrue(websAllocSession(owner, NULL, 600) == 0, "no session is created without a clock");
    teq(websGetSessionCount(), base, "a refused creation leaves the store unchanged");
    wp = presenting(sp);
    ttrue(websGetSession(wp, 0) == 0, "an existing session is treated as expired without a clock");
    teq(websGetSessionCount(), base - 1, "the session is retired");
    websFree(wp);
    mockFail = 0;
    websFree(owner);
}


#if ME_GOAHEAD_AUTH && ME_GOAHEAD_DIGEST
static WebsRoute *route;

static Webs *digestRequest(WebsTime when, uint64 counter, cchar *nc)
{
    Webs *wp;

    wp = newWebs(0);
    wp->route = route;
    wp->flags |= WEBS_AUTHENTICATED;
    wp->nonce = formatDigestNonce(when, counter);
    wp->nonceTime = when;
    wp->nc = sclone(nc);
    return wp;
}


/*
    A request that presents a full Digest header with the given nonce
 */
static Webs *digestHeader(cchar *nonce)
{
    Webs *wp;

    wp = newWebs(0);
    wp->route = route;
    wp->url = sclone("/d/x");
    wp->method = sclone("GET");
    wp->authDetails = sfmt("username=\"tu\", realm=\"%s\", nonce=\"%s\", uri=\"/d/x\", qop=auth, nc=00000001, "
                           "cnonce=\"c\", response=\"abc\"", ME_GOAHEAD_REALM, nonce);
    return wp;
}


static void testDigest(void)
{
    Webs     *wp;
    char     *nonce;
    WebsTime now;
    int      stale;

    tinfo("\n--- Digest nonces expire on a 64-bit clock ---\n");
    teq(websSetRouteAuth(route, "digest") , 0, "make the route Digest");
    ttrue(websAddUser("tu", "tp", "") != 0, "add a user");

    mockNow = BASE_2038 + 1000;
    now = websTime();
    wp = digestRequest(now - 5, 1, "00000001");
    ttrue(checkNonceCount(wp), "a fresh nonce is accepted");
    websFree(wp);

    wp = digestRequest(now - ME_GOAHEAD_NONCE_DURATION - 10, 2, "00000001");
    ttrue(!checkNonceCount(wp), "an expired nonce is refused");
    ttrue(wp->flags & WEBS_NONCE_STALE, "an expired nonce is reported stale");
    websFree(wp);

    if (WIDE_TIME) {
        mockNow = INT64(0x80000100);
        now = websTime();
        wp = digestRequest(now - 5, 3, "00000001");
        ttrue(checkNonceCount(wp), "a nonce issued after 2038 is accepted while fresh");
        websFree(wp);
        wp = digestRequest(now - ME_GOAHEAD_NONCE_DURATION - 10, 4, "00000001");
        ttrue(!checkNonceCount(wp), "a nonce issued after 2038 expires");
        websFree(wp);

        mockNow = INT64(0x100000000);
        wp = newWebs(0);
        wp->route = route;
        ttrue(createDigestNonce(wp) == 0, "no nonce is issued once the time overflows the 32-bit field");
        websFree(wp);
    }

    tinfo("\n--- A nonce is checked against the clock when presented ---\n");
    mockNow = 0;
    wp = newWebs(0);
    wp->route = route;
    nonce = createDigestNonce(wp);
    ttrue(nonce != 0, "a nonce is issued on a working clock");
    websFree(wp);
    wp = digestHeader(nonce);
    ttrue(parseDigestDetails(wp), "a freshly issued nonce is accepted");
    websFree(wp);

    mockNow = websTime() - 100;
    wp = digestHeader(nonce);
    ttrue(!parseDigestDetails(wp), "a nonce stamped in the future is refused");
    websFree(wp);

    mockNow = 0;
    mockFail = 1;
    wp = digestHeader(nonce);
    ttrue(!parseDigestDetails(wp), "a nonce is refused without a clock");
    websFree(wp);

    tinfo("\n--- A failed clock refuses Digest ---\n");
    wp = digestRequest(1, 5, "00000001");
    ttrue(nonceStale(wp), "every nonce is stale without a clock");
    ttrue(!checkNonceCount(wp), "a nonce is refused without a clock");
    stale = (wp->flags & WEBS_NONCE_STALE) != 0;
    ttrue(stale, "the refusal is reported stale");
    websFree(wp);
    wp = newWebs(0);
    wp->route = route;
    ttrue(createDigestNonce(wp) == 0, "no nonce is issued without a clock");
    digestLogin(wp);
    ttrue(wp->authResponse == 0, "no Digest challenge is offered without a clock");
    websFree(wp);
    mockFail = 0;
    wfree(nonce);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    testWidth();

    tinfo("\n--- Startup refuses to run on a failed clock ---\n");
    mockFail = 1;
    teq(websOpen("../web", NULL), -1, "websOpen must refuse to start without a clock");
    mockFail = 0;
    websTimeClose();
    websRuntimeClose();
    websOsClose();

    teq(websOpen("../web", NULL), 0, "open the server");
    testSessions();
#if ME_GOAHEAD_AUTH && ME_GOAHEAD_DIGEST
    route = websAddRoute("/d/", "continue", 0);
    testDigest();
#endif
    websClose();
    return 0;
}
