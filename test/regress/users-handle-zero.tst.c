/*
    users-handle-zero.tst.c - Abilities are computed for every user whatever handle the users hash holds

    WebsHash handles start at zero and freed handles are reused, so the users table can hold handle zero: it is
    the first hash of a process that opens authentication first, and a recycled handle after a close and reopen.
    websComputeAllUserAbilities must test the handle against -1, not for truth, or such a table is skipped and no
    user holds any ability.
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
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH

static bool holds(cchar *username, cchar *ability)
{
    WebsUser *user;

    user = websLookupUser(username);
    return user && user->abilities >= 0 && hashLookup(user->abilities, ability) != 0;
}


int main(int argc, char **argv)
{
    tinfo("users-handle-zero: a users table on hash handle zero still gets its abilities computed\n");

    /* The first hash of the process: websOpenAuth called before anything else, or a recycled handle */
    users = hashCreate(-1);
    teq(users, 0, "the users table holds handle zero");
    roleTable = hashCreate(-1);

    ttrue(websAddUser("viewer", "unused", "view") != 0, "add a user whose roles name an ability");
    websComputeAllUserAbilities();
    ttrue(holds("viewer", "view"), "A1 the user holds the ability after websComputeAllUserAbilities");

    ttrue(websAddUser("second", "unused", "view") != 0, "add a second user");
    teq(websSetUserRoles("second", "view,manage"), 0, "A2 websSetUserRoles computes abilities for one user");
    ttrue(holds("second", "manage"), "A3 that user holds the new ability");
    websComputeAllUserAbilities();
    ttrue(holds("second", "manage") && holds("viewer", "view"), "A4 recomputing all users keeps both");

    websCloseAuth();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication is not enabled");
    return 0;
}
#endif
