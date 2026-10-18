/*
    auth-unknown-user-cost.tst.c - A login for an unknown user costs what a Blowfish user costs
 */

#include "testme.h"
#include "goahead.h"
#include <time.h>

#if ME_GOAHEAD_AUTH
#define BATCH   32
#define BATCHES 3

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


/*
    Measure Blowfish directly as the timing baseline.
 */
static double cipherCost(cchar *hash)
{
    clock_t start;
    double  best, elapsed;
    int     batch, i;

    best = -1;
    for (batch = 0; batch < BATCHES; batch++) {
        start = clock();
        for (i = 0; i < BATCH; i++) {
            websCheckPassword("nobody:" ME_GOAHEAD_REALM ":wrong", hash);
        }
        elapsed = (double) (clock() - start) / CLOCKS_PER_SEC;
        if (best < 0 || elapsed < best) {
            best = elapsed;
        }
    }
    return best;
}


static char *makeHash(cchar *username, cchar *password)
{
    char *preimage, *hash;

    preimage = sfmt("%s:%s:%s", username, ME_GOAHEAD_REALM, password);
    hash = websMakePassword(preimage, 0, 0);
    wfree(preimage);
    return hash;
}


static void suiteA_md5Only(Webs *wp, cchar *reference)
{
    char   *md5, *preimage;
    double miss, cipher;

    tinfo("\n--- Suite A: with only MD5 users, an unknown user costs no cipher ---\n");

    preimage = sfmt("md5user:%s:secret", ME_GOAHEAD_REALM);
    md5 = websMD5(preimage);
    websAddUser("md5user", md5, "user");
    wfree(preimage);
    wfree(md5);

    ttrue(verify(wp, "md5user", "secret"), "the MD5 user must authenticate");
    miss = cost(wp, "nosuchuser", "wrong");
    cipher = cipherCost(reference);
    tinfo("unknown user %.6fs, cipher %.6fs per %d", miss, cipher, BATCH);
    ttrue(miss < cipher / 2, "an unknown user must not run the cipher when no user has a Blowfish hash "
          "(%.6fs against %.6fs)", miss, cipher);
}


static void suiteB_blowfish(Webs *wp, cchar *hash)
{
    char   name[32];
    double miss, hit;
    int    i;

    tinfo("\n--- Suite B: with a Blowfish user, an unknown user costs a Blowfish login ---\n");

    // Skip malformed hashes because they fail before cipher work.
    for (i = 0; i < 8; i++) {
        fmt(name, sizeof(name), "malformed%d", i);
        websAddUser(name, "BF1:00128:badsalt", "user");
    }
    websAddUser("bfuser", hash, "user");

    hit = cost(wp, "bfuser", "wrong");
    miss = cost(wp, "nosuchuser", "wrong");
    tinfo("unknown user %.6fs, failed Blowfish login %.6fs per %d", miss, hit, BATCH);
    ttrue(miss >= hit / 2, "an unknown user must cost at least half of a failed Blowfish login "
          "(%.6fs against %.6fs)", miss, hit);
}


static void suiteC_outcome(Webs *wp)
{
    tinfo("\n--- Suite C: the unknown user is refused and the Blowfish user still authenticates ---\n");

    ttrue(!verify(wp, "nosuchuser", "secret"), "an unknown user with the Blowfish user's password must be refused");
    ttrue(!verify(wp, "nosuchuser", "wrong"), "an unknown user must be refused");
    ttrue(verify(wp, "bfuser", "secret"), "the Blowfish user must authenticate");
    ttrue(!verify(wp, "bfuser", "wrong"), "the Blowfish user with a wrong password must be refused");
}


int main(int argc, char **argv)
{
    Webs *wp;
    char *hash;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    if (websOpen("../web", NULL) < 0) {
        tfail("cannot open the server");
        return 1;
    }
    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    hash = makeHash("bfuser", "secret");
    ttrue(hash != 0, "a Blowfish hash must be made");

    suiteA_md5Only(wp, hash);
    suiteB_blowfish(wp, hash);
    suiteC_outcome(wp);

    wfree(hash);
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
