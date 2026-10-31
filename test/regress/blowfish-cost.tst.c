/*
    BF2 hash cost scales with its rounds count; BF1 credentials still verify.
 */

#include "testme.h"
#include "goahead.h"
#include <time.h>

static double timeHash(int rounds)
{
    clock_t start;
    char    *hash;
    int     i;

    start = clock();
    for (i = 0; i < 4; i++) {
        hash = websMakePassword("user:example.com:secret", 16, rounds);
        ttrue(hash != 0, "a hash is made at %d rounds", rounds);
        ttrue(websCheckPassword("user:example.com:secret", hash), "the hash at %d rounds verifies", rounds);
        ttrue(!websCheckPassword("user:example.com:wrong", hash), "the hash at %d rounds refuses a wrong password",
              rounds);
        wfree(hash);
    }
    return (double) (clock() - start);
}

int main(int argc, char **argv)
{
    double low, high;
    char   *hash;

    low = timeHash(32);
    high = timeHash(1024);
    tinfo("32 rounds: %.0f ticks, 1024 rounds: %.0f ticks", low, high);
    ttrue(high > low * 8, "32 times the rounds must cost far more, got %.0f and %.0f ticks", low, high);

    hash = websMakePassword("user:example.com:secret", 16, 0);
    ttrue(hash && sstarts(hash, WEBS_CRYPT_BLOWFISH2 ":00128:"), "the default is 128 rounds in the BF2 format");
    wfree(hash);

    //  The configured BF1 account remains verifiable.
    ttrue(websCheckPassword("testbf1:example.com:pass1",
                            "BF1:00128:k0715dAbGKAUd0DC:ADXY6J8XaIHQ5RkXvW5ok5yUgohWUo3u"),
          "a legacy BF1 hash still verifies");
    return 0;
}
