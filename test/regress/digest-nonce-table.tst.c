/*
    The Digest nonce-count table is bounded, an evicted or expired nonce is refused as stale, and the challenge says
    so
 */

#define ME_GOAHEAD_LIMIT_NONCES 4

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
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_DIGEST

static WebsRoute *route;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    A request authenticated by Digest with the nonce issued at the given time
 */
static Webs *request(WebsTime when, uint64 counter, cchar *nc)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->route = route;
    wp->flags |= WEBS_AUTHENTICATED;
    wp->nonce = formatDigestNonce(when, counter);
    wp->nonceTime = when;
    wp->nc = sclone(nc);
    return wp;
}

static int records(void)
{
    WebsKey *key;
    int     count;

    for (count = 0, key = hashFirst(nonceCounts); key; key = hashNext(nonceCounts, key)) {
        count++;
    }
    return count;
}

int main(int argc, char **argv)
{
    Webs     *wp;
    WebsTime now;
    int      i;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    route = websAddRoute("/d/", "continue", 0);
    teq(websSetRouteAuth(route, "digest"), 0, "make the route Digest");
    now = time(0);

    //  The table never holds more than the cap
    for (i = 0; i < 10; i++) {
        wp = request(now - 20 + i, (uint64) i, "00000001");
        ttrue(checkNonceCount(wp), "a fresh nonce count is accepted");
        websAcceptDigestCount(wp);
        websFree(wp);
        ttrue(records() <= ME_GOAHEAD_LIMIT_NONCES, "the table must not exceed its cap, holds %d", records());
    }

    //  An evicted nonce is refused as stale, so it can never be replayed
    wp = request(now - 20, 0, "00000002");
    ttrue(!checkNonceCount(wp), "an evicted nonce must be refused");
    ttrue(wp->flags & WEBS_NONCE_STALE, "an evicted nonce must be refused as stale");
    websFree(wp);

    //  A retained nonce refuses a replayed count and accepts a higher one
    wp = request(now - 11, 9, "00000001");
    ttrue(!checkNonceCount(wp), "a replayed count must be refused");
    ttrue(!(wp->flags & WEBS_NONCE_STALE), "a replay is not stale");
    websFree(wp);
    wp = request(now - 11, 9, "00000002");
    ttrue(checkNonceCount(wp), "a higher count must be accepted");
    websFree(wp);

    //  An expired nonce is refused as stale, and the challenge says so
    wp = request(now - ME_GOAHEAD_NONCE_DURATION - 10, 100, "00000001");
    ttrue(!checkNonceCount(wp), "an expired nonce must be refused");
    digestLogin(wp);
    ttrue(strstr(wp->authResponse, "stale=\"TRUE\"") != 0, "the challenge must report stale=TRUE, got %s",
          wp->authResponse);
    websFree(wp);

    wp = request(now, 101, "00000001");
    digestLogin(wp);
    ttrue(strstr(wp->authResponse, "stale=\"FALSE\"") != 0, "a fresh challenge must report stale=FALSE");
    websFree(wp);

    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Digest authentication is not enabled");
    return 0;
}
#endif
