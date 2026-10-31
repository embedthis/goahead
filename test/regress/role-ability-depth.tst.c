/*
    role-ability-depth.tst.c - Every ability a role lists is granted, however many it lists and however roles nest

    computeAbilities bounds role recursion with a depth counter. It must count nesting depth, not the number of
    abilities visited, or a wide role silently loses abilities past the bound and logs a misleading recursion error.
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

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

#define WIDE  30                                /* Abilities on one role, past the recursion bound of 20 */
#define OUTER 12                                /* Direct abilities on a role that also nests another */
#define INNER 12                                /* Abilities on the nested role */

static int  recursionErrors;

static void countLog(int level, cchar *msg)
{
    if (strstr(msg, "Recursive ability definition")) {
        recursionErrors++;
    }
}


static WebsHash abilitySet(cchar *prefix, int count, cchar *extra)
{
    WebsHash abilities;
    char     name[32];
    int      i;

    abilities = hashCreate(-1);
    for (i = 1; i <= count; i++) {
        fmt(name, sizeof(name), "%s%02d", prefix, i);
        hashEnter(abilities, name, valueInteger(0), 0);
    }
    if (extra) {
        hashEnter(abilities, extra, valueInteger(0), 0);
    }
    return abilities;
}


static int countGranted(WebsHash abilities, cchar *prefix, int count)
{
    char name[32];
    int  i, granted;

    for (i = 1, granted = 0; i <= count; i++) {
        fmt(name, sizeof(name), "%s%02d", prefix, i);
        if (hashLookup(abilities, name)) {
            granted++;
        }
    }
    return granted;
}


static void suiteA_wideRole(void)
{
    WebsUser user;

    tinfo("\n--- Suite A: a role listing %d abilities grants all of them ---\n", WIDE);

    memset(&user, 0, sizeof(user));
    user.abilities = -1;
    user.roles = (char*) "wide";
    recursionErrors = 0;
    computeUserAbilities(&user);
    teq(countGranted(user.abilities, "w", WIDE), WIDE, "A1 every ability of the wide role is granted");
    teq(recursionErrors, 0, "A2 a flat role is not reported as recursive");
    hashFree(user.abilities);
}


static void suiteB_nestedRoles(void)
{
    WebsUser user;

    tinfo("\n--- Suite B: a role nesting another grants both sets ---\n");

    memset(&user, 0, sizeof(user));
    user.abilities = -1;
    user.roles = (char*) "outer";
    recursionErrors = 0;
    computeUserAbilities(&user);
    teq(countGranted(user.abilities, "o", OUTER), OUTER, "B1 every direct ability of the outer role is granted");
    teq(countGranted(user.abilities, "i", INNER), INNER, "B2 every ability of the nested role is granted");
    teq(recursionErrors, 0, "B3 a two-level nesting is not reported as recursive");
    hashFree(user.abilities);
}


static void suiteC_cycleStillReported(void)
{
    WebsUser user;

    tinfo("\n--- Suite C: a role that names itself is still reported ---\n");

    memset(&user, 0, sizeof(user));
    user.abilities = -1;
    user.roles = (char*) "loop";
    recursionErrors = 0;
    computeUserAbilities(&user);
    ttrue(recursionErrors > 0, "C1 a recursive role definition is reported");
    ttrue(hashLookup(user.abilities, "x01") != 0, "C2 the abilities reached before the bound are granted");
    hashFree(user.abilities);
}


int main(int argc, char **argv)
{
    tinfo("role-ability-depth: the recursion bound counts role depth, not abilities visited\n");

    logSetHandler(countLog);
    roleTable = hashCreate(-1);
    ttrue(websAddRole("wide", abilitySet("w", WIDE, 0)) != 0, "define the wide role");
    ttrue(websAddRole("inner", abilitySet("i", INNER, 0)) != 0, "define the nested role");
    ttrue(websAddRole("outer", abilitySet("o", OUTER, "inner")) != 0, "define the outer role");
    ttrue(websAddRole("loop", abilitySet("x", 1, "loop")) != 0, "define the recursive role");

    suiteA_wideRole();
    suiteB_nestedRoles();
    suiteC_cycleStillReported();

    hashFree(roleTable);
    roleTable = -1;
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication or logging is not enabled");
    return 0;
}
#endif
