/*
    auth-md5-user-cost.tst.c - A refused password for an MD5 account costs what it costs for an unknown account
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


int main(int argc, char **argv)
{
    Webs   *wp;
    char   *preimage, *md5, *hash;
    double md5Miss, unknownMiss, blowfishMiss;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));

    preimage = sfmt("md5user:%s:secret", ME_GOAHEAD_REALM);
    md5 = websMD5(preimage);
    ttrue(websAddUser("md5user", md5, "user") != 0, "add an MD5 user");
    wfree(preimage);
    wfree(md5);
    preimage = sfmt("bfuser:%s:secret", ME_GOAHEAD_REALM);
    hash = websMakePassword(preimage, 0, 0);
    ttrue(websAddUser("bfuser", hash, "user") != 0, "add a Blowfish user");
    wfree(preimage);
    wfree(hash);
    websComputeAllUserAbilities();

    ttrue(verify(wp, "md5user", "secret"), "the MD5 user authenticates");
    ttrue(!verify(wp, "md5user", "wrong"), "a wrong password for the MD5 user is refused");
    ttrue(verify(wp, "bfuser", "secret"), "the Blowfish user authenticates");

    md5Miss = cost(wp, "md5user", "wrong");
    blowfishMiss = cost(wp, "bfuser", "wrong");
    unknownMiss = cost(wp, "nosuchuser", "wrong");
    tinfo("refused MD5 %.6fs, refused Blowfish %.6fs, unknown %.6fs per %d", md5Miss, blowfishMiss, unknownMiss, BATCH);
    ttrue(md5Miss >= unknownMiss / 2, "a refused MD5 login must cost at least half of an unknown-user login "
          "(%.6fs against %.6fs)", md5Miss, unknownMiss);
    ttrue(md5Miss >= blowfishMiss / 2, "a refused MD5 login must cost at least half of a refused Blowfish login "
          "(%.6fs against %.6fs)", md5Miss, blowfishMiss);

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
