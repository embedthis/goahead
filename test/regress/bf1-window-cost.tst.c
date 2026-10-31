/*
    Refused BF1 logins at the key-window limit must still pay the Blowfish verification cost.
    Unknown-user decoys must cost the same as known-user failures for either hash format.
 */

#include "testme.h"
#include "goahead.h"
#include <time.h>

#if ME_GOAHEAD_AUTH
#define BATCH   32
#define BATCHES 3
#define SALT    "0123456789abcdef"
#define LONG_USER "longnameuser0000000000000000000000000000"    /* 40 characters */
#define OTHER_USER "othernameuser000000000000000000000000000"   /* 40 characters, not an account */

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static bool verify(Webs *wp, cchar *username, cchar *password)
{
    wfreeset(wp->username);
    wfreeset(wp->password);
    wp->username = sclone(username);
    wp->password = sclone(password);
    wp->encoded = 0;
    wp->user = 0;
    return websVerifyPasswordFromFile(wp);
}


/*
    Use the least CPU time across batches to reduce interruption noise.
 */
static double cost(Webs *wp, cchar *username, cchar *password)
{
    clock_t start;
    double  best, elapsed;
    int     batch, i;

    best = -1;
    for (batch = 0; batch < BATCHES; batch++) {
        start = clock();
        for (i = 0; i < BATCH; i++) {
            verify(wp, username, password);
        }
        elapsed = (double) (clock() - start) / CLOCKS_PER_SEC;
        if (best < 0 || elapsed < best) {
            best = elapsed;
        }
    }
    return best;
}


static double checkCost(cchar *key, cchar *hash)
{
    clock_t start;
    double  best, elapsed;
    int     batch, i;

    best = -1;
    for (batch = 0; batch < BATCHES; batch++) {
        start = clock();
        for (i = 0; i < BATCH; i++) {
            websCheckPassword(key, hash);
        }
        elapsed = (double) (clock() - start) / CLOCKS_PER_SEC;
        if (best < 0 || elapsed < best) {
            best = elapsed;
        }
    }
    return best;
}


static char *preimage(cchar *username, cchar *password)
{
    return sfmt("%s:%s:%s", username, ME_GOAHEAD_REALM, password);
}


static char *makeBf1Hash(cchar *username, cchar *password)
{
    char *key, *hash, *record;

    key = preimage(username, password);
    hash = websCryptPassword(key, SALT, 128);
    record = sfmt("%s:00128:%s:%s", WEBS_CRYPT_BLOWFISH, SALT, hash);
    wfree(hash);
    wfree(key);
    return record;
}


static bool isCompleteBlowfish(cchar *hash)
{
    int fields;

    if (!sstarts(hash, WEBS_CRYPT_BLOWFISH ":") && !sstarts(hash, WEBS_CRYPT_BLOWFISH2 ":")) {
        return 0;
    }
    for (fields = 1; *hash; hash++) {
        if (*hash == ':') {
            fields++;
        }
    }
    return fields == 4;
}


/*
    The name of the first user with a complete Blowfish hash in table order: the decoy account.
 */
static cchar *firstBlowfishUser(void)
{
    WebsUser *user;
    WebsKey  *key;

    for (key = hashFirst(websGetUsers()); key; key = hashNext(websGetUsers(), key)) {
        user = key->content.value.symbol;
        if (isCompleteBlowfish(user->password)) {
            return user->name;
        }
    }
    return 0;
}


static void suiteA_checkPassword(void)
{
    char   *hash, *below, *window;
    double belowCost, windowCost;

    tinfo("\n--- Suite A: a BF1 verification refused at the key window costs what one below it costs ---\n");

    hash = makeBf1Hash("bf1user", "secret");
    below = preimage("bf1user", "wrong");
    window = preimage(LONG_USER, "wrong");
    ttrue(slen(SALT) + 1 + slen(below) < WEBS_BLOWFISH_KEY_WINDOW, "the short credential is below the key window");
    ttrue(slen(SALT) + 1 + slen(window) >= WEBS_BLOWFISH_KEY_WINDOW, "the long credential reaches the key window");

    ttrue(!websCheckPassword(below, hash), "a wrong password below the window is refused");
    ttrue(!websCheckPassword(window, hash), "a credential at the window is refused");
    wfree(below);
    below = preimage("bf1user", "secret");
    ttrue(websCheckPassword(below, hash), "the right password below the window verifies");
    wfree(below);
    below = preimage("bf1user", "wrong");

    belowCost = checkCost(below, hash);
    windowCost = checkCost(window, hash);
    tinfo("refused below the window %.6fs, refused at the window %.6fs per %d", belowCost, windowCost, BATCH);
    ttrue(windowCost >= belowCost / 2, "a BF1 refusal at the key window must cost at least half of a refusal "
          "below it (%.6fs against %.6fs)", windowCost, belowCost);
    wfree(window);
    wfree(below);
    wfree(hash);
}


static void suiteB_unknownUser(Webs *wp)
{
    char   name[32], *bf1, *bf2, *key;
    cchar  *decoy;
    double unknownLong, knownBf2, knownBf1;
    int    i;

    tinfo("\n--- Suite B: an unknown user costs a Blowfish login at every credential length ---\n");

    key = preimage(LONG_USER, "secret");
    bf2 = websMakePassword(key, 0, 0);
    ttrue(bf2 != 0, "a BF2 hash is made for the long account name");
    ttrue(websAddUser(LONG_USER, bf2, "user") != 0, "add the BF2 account with a 40 character name");
    wfree(key);
    wfree(bf2);

    /*
        Put a cheaper BF1 hash first in table order; the decoy must still select the costlier BF2 hash.
     */
    decoy = 0;
    for (i = 0; i < 64; i++) {
        fmt(name, sizeof(name), "bf1user%d", i);
        bf1 = makeBf1Hash(name, "secret");
        ttrue(websAddUser(name, bf1, "user") != 0, "add the BF1 account");
        wfree(bf1);
        if (smatch((decoy = firstBlowfishUser()), name)) {
            break;
        }
        websRemoveUser(name);
    }
    websComputeAllUserAbilities();
    tinfo("decoy account is \"%s\"", decoy ? decoy : "-");
    ttrue(decoy && sstarts(decoy, "bf1user"), "the decoy account carries a legacy BF1 hash");

    ttrue(verify(wp, LONG_USER, "secret"), "the long BF2 account authenticates");
    ttrue(verify(wp, name, "secret"), "the BF1 account authenticates");
    ttrue(!verify(wp, LONG_USER, "wrong"), "the long BF2 account refuses a wrong password");
    ttrue(!verify(wp, OTHER_USER, "wrong"), "an unknown long name is refused");

    knownBf1 = cost(wp, name, "wrong");
    knownBf2 = cost(wp, LONG_USER, "wrong");
    unknownLong = cost(wp, OTHER_USER, "wrong");
    tinfo("refused BF1 short %.6fs, refused BF2 long %.6fs, unknown long %.6fs per %d",
          knownBf1, knownBf2, unknownLong, BATCH);
    ttrue(unknownLong >= knownBf2 / 2, "an unknown long name must cost at least half of a refused BF2 login of "
          "the same length (%.6fs against %.6fs)", unknownLong, knownBf2);
    ttrue(unknownLong >= knownBf1 / 2, "an unknown long name must cost at least half of a refused short Blowfish "
          "login (%.6fs against %.6fs)", unknownLong, knownBf1);
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
    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));

    suiteA_checkPassword();
    suiteB_unknownUser(wp);

    wfree(wp->username);
    wfree(wp->password);
    wfree(wp);
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
