/*
    session-id-ownership.tst.c - websGetSessionID returns caller-owned memory
 */

#include "testme.h"
#include "goahead.h"

static Webs *allocWebs(void)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    return wp;
}


int main(int argc, char **argv)
{
    Webs        *wp;
    WebsSession session;
    char        *id;

    tinfo("session-id-ownership: websGetSessionID must always return an owned copy\n");

    wp = allocWebs();
    memset(&session, 0, sizeof(session));
    session.id = sclone("::webs.session::abcdef");
    wp->session = &session;

    id = websGetSessionID(wp);
    ttrue(id != 0, "a resolved session must yield an id");
    ttrue(id != session.id, "the result must not be the session's own id string");
    ttrue(smatch(id, session.id), "the result must equal the session id");
    if (id != session.id) {
        wfree(id);
    }
    ttrue(smatch(session.id, "::webs.session::abcdef"), "freeing the result must leave the session id intact");
    wfree(session.id);
    wp->session = 0;

    wp->cookie = sclone("-goahead-session-=::webs.session::fromcookie; other=1");
    id = websGetSessionID(wp);
    ttrue(id != 0, "a session cookie must yield an id");
    ttrue(smatch(id, "::webs.session::fromcookie"), "the id must be parsed from the cookie");
    wfree(id);

    wfree(wp->cookie);
    wp->cookie = 0;
    ttrue(websGetSessionID(wp) == 0, "no session and no cookie must yield no id");

    wfree(wp);
    return 0;
}
