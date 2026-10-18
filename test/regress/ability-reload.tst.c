/* Recomputed ability tables must populate handle zero and release their predecessors. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
static int liveTables;
static WebsHash trackedCreate(int size) { WebsHash h = hashCreate(size); if (h >= 0) liveTables++; return h; }
static void trackedFree(WebsHash h) { if (h >= 0) liveTables--; hashFree(h); }
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#define hashCreate trackedCreate
#define hashFree trackedFree
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef hashCreate
#undef hashFree
int main(int argc, char **argv)
{
#if ME_GOAHEAD_AUTH
    WebsUser user;
    WebsHash spare;
    WebsKey *key;
    int i;
    memset(&user, 0, sizeof(user));
    user.abilities = -1;
    user.roles = "read";
    spare = hashCreate(-1);
    teq(spare, 0, "reserve zero handle");
    roleTable = hashCreate(-1);
    hashFree(spare);
    computeUserAbilities(&user);
    teq(user.abilities, 0, "ability table uses recycled zero handle");
    key = hashLookup(user.abilities, "read");
    tnotnull(key, "zero-handle table is populated");
    for (i = 0; i < 20; i++) {
        user.roles = i % 2 ? "read" : "write";
        computeUserAbilities(&user);
        teq(liveTables, 1, "reload retains exactly one ability table");
        key = hashLookup(user.abilities, user.roles);
        tnotnull(key, "updated ability is present");
        key = hashLookup(user.abilities, i % 2 ? "write" : "read");
        tnull(key, "previous ability is removed");
    }
    trackedFree(user.abilities);
    hashFree(roleTable);
    roleTable = -1;
    teq(liveTables, 0, "all user ability tables released");
#else
    tskip("Auth disabled");
#endif
    return 0;
}
