/*
    role-removal-abilities.tst.c - Removing a role withdraws its abilities from the users that held it

    websSetUserRoles recomputes the user's abilities at once. websRemoveRole must do the same for every user, or
    the users keep the removed role's abilities until something else recomputes them.

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

static WebsHash abilitySet(cchar *ability)
{
    WebsHash abilities;

    abilities = hashCreate(-1);
    hashEnter(abilities, ability, valueInteger(0), 0);
    return abilities;
}


static bool holds(cchar *username, cchar *ability)
{
    WebsUser *user;

    user = websLookupUser(username);
    return user && user->abilities >= 0 && hashLookup(user->abilities, ability) != 0;
}


int main(int argc, char **argv)
{
    tinfo("role-removal-abilities: websRemoveRole withdraws the role's abilities from its users\n");

    /* The role table takes handle zero: users-handle-zero.tst.c covers a users hash on that handle */
    roleTable = hashCreate(-1);
    users = hashCreate(-1);
    ttrue(websAddRole("ops", abilitySet("deploy")) != 0, "define a role granting deploy");
    ttrue(websAddRole("audit", abilitySet("read")) != 0, "define a role granting read");
    ttrue(websAddUser("operator", "unused", "ops,audit") != 0, "add a user holding both roles");
    websComputeAllUserAbilities();
    ttrue(holds("operator", "deploy"), "the user holds deploy through the ops role");
    ttrue(holds("operator", "read"), "the user holds read through the audit role");

    tinfo("\n--- Suite A: removing a role withdraws its abilities ---\n");
    teq(websRemoveRole("ops"), 0, "A1 remove the ops role");
    ttrue(!holds("operator", "deploy"), "A2 the user no longer holds deploy once the role granting it is gone");
    ttrue(holds("operator", "read"), "A3 the user keeps read, which the remaining role grants");

    tinfo("\n--- Suite B: the user record is unchanged, only its abilities ---\n");
    ttrue(smatch(websLookupUser("operator")->roles, "ops,audit"), "B1 the roles string still names the removed role");
    teq(websRemoveRole("ops"), -1, "B2 removing an unknown role fails");

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
