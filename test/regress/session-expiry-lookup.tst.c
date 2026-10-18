/*
    session-expiry-lookup.tst.c - An expired session is dead when it is presented
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int fired;

static void markEvent(void *arg, int id)
{
    fired = 1;
    websStopEvent(id);
}


static Webs *allocWebs(cchar *cookie)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->host = sclone("example.com");
    wp->responseCookies = hashCreate(7);
    wp->cookie = cookie ? sclone(cookie) : 0;
    return wp;
}


static void freeWebs(Webs *wp)
{
    hashFree(wp->responseCookies);
    wfree(wp->cookie);
    wfree(wp->host);
    wfree(wp);
}


static Webs *presenting(WebsSession *sp)
{
    Webs *wp;
    char *cookie;

    cookie = sfmt("%s=%s", WEBS_SESSION, sp->id);
    wp = allocWebs(cookie);
    wfree(cookie);
    return wp;
}


static void suiteA_expired(void)
{
    WebsSession *sp, *found;
    Webs        *owner, *wp;
    char        *id;
    int         base;

    tinfo("\n--- Suite A: an expired session is refused when presented ---\n");

    owner = allocWebs(0);
    base = websGetSessionCount();
    sp = websAllocSession(owner, NULL, 60);
    ttrue(sp != 0, "allocate a session");
    id = sclone(sp->id);
    sp->expires = time(0) - 1;

    wp = presenting(sp);
    found = websGetSession(wp, 0);
    ttrue(found == 0, "an expired session must not be returned");
    teq(websGetSessionCount(), base, "the expired session must be removed from the store");

    found = websGetSession(wp, 1);
    ttrue(found != 0, "a new session must be created in its place when requested");
    if (found) {
        ttrue(!smatch(found->id, id), "the new session must not reuse the expired id");
        websDestroySession(wp);
    }
    wfree(id);
    freeWebs(wp);
    freeWebs(owner);
}


static void suiteB_live(void)
{
    WebsSession *sp, *found;
    Webs        *owner, *wp;

    tinfo("\n--- Suite B: a live session is still found ---\n");

    owner = allocWebs(0);
    sp = websAllocSession(owner, NULL, 60);
    ttrue(sp != 0, "allocate a session");

    wp = presenting(sp);
    found = websGetSession(wp, 0);
    ttrue(found == sp, "a live session presented by cookie must be found");
    websDestroySession(wp);
    freeWebs(wp);
    freeWebs(owner);
}


int main(int argc, char **argv)
{
    int id;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    /*
        Register first to reserve event ID zero.
     */
    id = websStartEvent(0, markEvent, 0);
    teq(id, 0, "the probe event must hold id zero");

    websSetDebug(1);
    if (websOpen("../web", NULL) < 0) {
        tfail("cannot open the server");
        return 1;
    }
    suiteA_expired();
    suiteB_live();

    tinfo("\n--- Suite C: websClose leaves other events alone ---\n");
    websClose();
    websRunEvents();
    ttrue(fired, "an event websClose does not own must survive it");
    return 0;
}
