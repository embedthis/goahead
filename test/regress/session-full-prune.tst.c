/*
    A full session table is pruned before a session is refused, and one account holds at most
    ME_GOAHEAD_LIMIT_SESSION_USER sessions
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static Webs *allocWebs(cchar *username)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->host = sclone("example.com");
    wp->responseCookies = hashCreate(7);
    wp->username = sclone(username);
    return wp;
}


static void freeWebs(Webs *wp)
{
    hashFree(wp->responseCookies);
    wfree(wp->username);
    wfree(wp->host);
    wfree(wp);
}


static void destroy(Webs *wp, WebsSession *sp)
{
    wp->session = sp;
    websDestroySession(wp);
    wp->session = 0;
}


static void suiteA_pruneWhenFull(Webs *wp)
{
    WebsSession *sp;
    int         base, count;

    tinfo("\n--- Suite A: expired sessions are pruned when the table is full ---\n");

    base = websGetSessionCount();
    for (count = base; count < ME_GOAHEAD_LIMIT_SESSION_COUNT; count++) {
        if (websAllocSession(wp, NULL, 0) == 0) {
            break;
        }
    }
    teq(websGetSessionCount(), ME_GOAHEAD_LIMIT_SESSION_COUNT, "the table must fill with expired sessions");
    sp = websAllocSession(wp, NULL, 60);
    ttrue(sp != 0, "a full table of expired sessions must still accept a new session");
    ttrue(websGetSessionCount() <= base + 1, "the expired sessions must have been removed");
    if (sp) {
        destroy(wp, sp);
    }
}



static bool live(cchar *id)
{
    Webs *probe;
    bool found;

    probe = allocWebs("probe");
    probe->cookie = sfmt("%s=%s", WEBS_SESSION, id);
    found = websGetSession(probe, 0) != 0;
    wfree(probe->cookie);
    freeWebs(probe);
    return found;
}


static void suiteB_userCap(void)
{
    WebsSession *sp;
    Webs        *mary, *joshua;
    char        *firstId, *lastId;
    int         base, i;

    tinfo("\n--- Suite B: one account holds at most ME_GOAHEAD_LIMIT_SESSION_USER sessions ---\n");

#if ME_GOAHEAD_LIMIT_SESSION_USER > 0
    mary = allocWebs("mary");
    joshua = allocWebs("joshua");
    base = websGetSessionCount();
    firstId = lastId = 0;
    for (i = 0; i < ME_GOAHEAD_LIMIT_SESSION_USER + 4; i++) {
        //  Each login is a new client holding no session
        mary->session = 0;
        sp = websLoginSession(mary, "form");
        ttrue(sp != 0, "every login must get a session");
        if (sp && i == 0) {
            firstId = sclone(sp->id);
        }
    }
    if (mary->session) {
        lastId = sclone(mary->session->id);
    }
    teq(websGetSessionCount(), base + ME_GOAHEAD_LIMIT_SESSION_USER, "one account must hold at most the limit");
    ttrue(firstId && !live(firstId), "the least recently used session must be retired");
    ttrue(lastId && live(lastId), "the newest session must be kept");
    wfree(firstId);
    wfree(lastId);

    joshua->session = 0;
    sp = websLoginSession(joshua, "basic");
    ttrue(sp != 0, "another account must still log in");
    tmatch(websGetSessionVar(joshua, WEBS_SESSION_USERNAME, ""), "joshua", "the session names its user");
    tmatch(websGetSessionVar(joshua, WEBS_SESSION_AUTHTYPE, ""), "basic", "the session names its protocol");
    teq(websGetSessionCount(), base + ME_GOAHEAD_LIMIT_SESSION_USER + 1, "the other account's session is added");

    freeWebs(mary);
    freeWebs(joshua);
#else
    tskip("No per-account session limit in this build");
#endif
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
    wp = allocWebs("nobody");
    suiteA_pruneWhenFull(wp);
    suiteB_userCap();
    freeWebs(wp);
    websClose();
    return 0;
}
