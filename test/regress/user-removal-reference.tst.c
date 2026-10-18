/*
    user-removal-reference.tst.c - Removing a user leaves no request holding its record
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
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH
#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static Webs *authenticated(cchar *username)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->username = sclone(username);
    wp->user = websLookupUser(username);
    return wp;
}


int main(int argc, char **argv)
{
    Webs     *removed, *other;
    WebsHash ability;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    ability = hashCreate(-1);
    hashEnter(ability, "view", valueInteger(0), 0);
    ttrue(websAddUser("leaving", "unused", "view") != 0, "add the user to remove");
    ttrue(websAddUser("staying", "unused", "view") != 0, "add a user to keep");
    websComputeAllUserAbilities();

    removed = authenticated("leaving");
    other = authenticated("staying");
    ttrue(removed->user != 0 && websCan(removed, ability), "the user can view before removal");

    teq(websRemoveUser("leaving"), 0, "remove the user");
    ttrue(removed->user == 0, "a request holding the removed user must drop the record");
    ttrue(!websCan(removed, ability), "the removed user must no longer be granted anything");
    ttrue(other->user != 0 && websCan(other, ability), "a request for another user is untouched");

    websFree(removed);
    websFree(other);
    hashFree(ability);
    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication is not enabled");
    return 0;
}
#endif
