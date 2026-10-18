/*
    session-count-accounting.tst.c - The session count is the number of sessions in the store
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE
#include "fd-exhaust-fixture.h"
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

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


static void destroy(Webs *wp, WebsSession *sp)
{
    wp->session = sp;
    websDestroySession(wp);
}


static void suiteA_directAllocation(Webs *wp)
{
    WebsSession *sp;
    int         base;

    tinfo("\n--- Suite A: a directly allocated session is counted ---\n");

    base = websGetSessionCount();
    sp = websAllocSession(wp, NULL, 60);
    ttrue(sp != 0, "websAllocSession must allocate a session");
    teq(websGetSessionCount(), base + 1, "a directly allocated session must be counted");
    destroy(wp, sp);
    teq(websGetSessionCount(), base, "destroying it must give the count back, not take it below the base");
}


#if ME_UNIX_LIKE
static void suiteB_failedCreation(Webs *wp)
{
    Exhaust     ex;
    WebsSession *sp;
    int         base, i;

    tinfo("\n--- Suite B: a failed creation does not consume a slot ---\n");

    base = websGetSessionCount();
    exhaustDescriptors(&ex);
    for (i = 0; i < 10; i++) {
        wp->session = 0;
        sp = websGetSession(wp, 1);
        if (sp) {
            break;
        }
    }
    releaseDescriptors(&ex);

    ttrue(sp == 0, "session creation must fail while the random source cannot be opened");
    teq(websGetSessionCount(), base, "failed creations must leave the session count unchanged");

    wp->session = 0;
    sp = websGetSession(wp, 1);
    ttrue(sp != 0, "session creation must work again once descriptors are available");
    teq(websGetSessionCount(), base + 1, "the recovered session must be counted once");
    destroy(wp, sp);
}
#endif


static void suiteC_cap(Webs *wp)
{
    WebsSession **list;
    int         base, count, i;

    tinfo("\n--- Suite C: the cap applies to direct allocation ---\n");

    base = websGetSessionCount();
    list = walloc(sizeof(WebsSession*) * (ME_GOAHEAD_LIMIT_SESSION_COUNT + 1));
    for (count = 0; count <= ME_GOAHEAD_LIMIT_SESSION_COUNT; count++) {
        if ((list[count] = websAllocSession(wp, NULL, 60)) == 0) {
            break;
        }
    }
    teq(count, ME_GOAHEAD_LIMIT_SESSION_COUNT - base, "allocation must stop at the session cap");
    teq(websGetSessionCount(), ME_GOAHEAD_LIMIT_SESSION_COUNT, "the count must stop exactly at the cap");

    for (i = 0; i < count; i++) {
        destroy(wp, list[i]);
    }
    wfree(list);
    teq(websGetSessionCount(), base, "destroying every session must return the count to its base");
}


/*
    Require cache-handle reuse after failed session insertion to detect leaks.
 */
static void suiteD_refused(Webs *wp)
{
    WebsSession *sp;
    WebsHash    probe, after;
    int         base;

    tinfo("\n--- Suite D: a refused session frees its cache ---\n");
    base = websGetSessionCount();
    probe = hashCreate(7);
    hashFree(probe);
    sp = websAllocSession(wp, "", 60);
    ttrue(sp == 0, "a session with an empty id must be refused");
    teq(websGetSessionCount(), base, "a refused session must not be counted");
    after = hashCreate(7);
    teq(after, probe, "a refused session must free its cache table");
    hashFree(after);
}


int main(int argc, char **argv)
{
    Webs *wp;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    if (websOpen("../web", NULL) < 0) {
        tfail("cannot open the server");
        return 1;
    }
    wp = allocWebs();

    suiteA_directAllocation(wp);
#if ME_UNIX_LIKE
    suiteB_failedCreation(wp);
#endif
    suiteC_cap(wp);
    suiteD_refused(wp);

    freeWebs(wp);
    websClose();
    return 0;
}
