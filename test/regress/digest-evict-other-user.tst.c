/*
    Nonce-table eviction touches only the account that filled the table: another account's fresh nonce stays valid,
    the evicted account's nonces stay refused, and the cap still holds
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
    A request authenticated by Digest as the given account with the nonce issued at the given time
 */
static Webs *request(cchar *username, WebsTime when, uint64 counter, cchar *nc)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->route = route;
    wp->flags |= WEBS_AUTHENTICATED;
    wp->username = sclone(username);
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

    //  Fill the table with mary's nonces while joshua holds an equally fresh, unused nonce.
    for (i = 0; i <= ME_GOAHEAD_LIMIT_NONCES; i++) {
        wp = request("mary", now, (uint64) i, "00000001");
        ttrue(checkNonceCount(wp), "mary's fresh nonce %d must be accepted", i);
        websAcceptDigestCount(wp);
        websFree(wp);
        ttrue(records() <= ME_GOAHEAD_LIMIT_NONCES, "the table must not exceed its cap, holds %d", records());
    }

    //  Another account's unused nonce must survive mary's eviction
    wp = request("joshua", now, 1000, "00000001");
    ttrue(checkNonceCount(wp), "another account's fresh nonce must still be accepted after eviction, refused: %s",
          wp->authReason ? wp->authReason : "-");
    ttrue(!(wp->flags & WEBS_NONCE_STALE), "another account's fresh nonce must not be reported stale");
    websAcceptDigestCount(wp);
    websFree(wp);

    //  The evicted account's nonce stays refused as stale, so eviction never permits a replay
    wp = request("mary", now, 0, "00000002");
    ttrue(!checkNonceCount(wp), "mary's evicted nonce must be refused");
    ttrue(wp->flags & WEBS_NONCE_STALE, "mary's evicted nonce must be refused as stale");
    websFree(wp);

    //  The surviving account's recorded count still refuses a replay and accepts a higher count
    wp = request("joshua", now, 1000, "00000001");
    ttrue(!checkNonceCount(wp), "a replayed count must be refused");
    websFree(wp);
    wp = request("joshua", now, 1000, "00000002");
    ttrue(checkNonceCount(wp), "a higher count must be accepted");
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
