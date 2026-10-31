/*
    password-rounds-field.tst.c - A stored password record whose work-factor field is not a positive number
    verifies nothing; the verifier applies the same rule websMakePassword does when it writes the record.
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
#include "../../src/crypt.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH
/*
    Rewrite the rounds field of a record and make its digest what the cipher yields for that count, which for a
    non-positive count no longer depends on the password. Such a record must verify nothing.
 */
static char *withRounds(cchar *record, cchar *rounds, cchar *password)
{
    char *copy, *algorithm, *salt, *digest, *tok, *result;

    copy = sclone(record);
    algorithm = stok(copy, ":", &tok);
    stok(NULL, ":", &tok);
    salt = stok(NULL, ":", &tok);
    digest = cryptPassword2(password, salt, atoi(rounds));
    result = sfmt("%s:%s:%s:%s", algorithm, rounds, salt, digest ? digest : "");
    wfree(digest);
    wfree(copy);
    return result;
}


int main(int argc, char **argv)
{
    char  *record, *damaged;
    cchar *bad[] = { "00000", "0", "-1", "xy", "12a", "", NULL };
    int   i;

    record = websMakePassword("user:example.com:alpha", 0, 0);
    ttrue(record != 0, "make a password record");
    ttrue(websCheckPassword("user:example.com:alpha", record), "the right password verifies");
    ttrue(!websCheckPassword("user:example.com:bravo", record), "a wrong password is refused");

    for (i = 0; bad[i]; i++) {
        damaged = withRounds(record, bad[i], "user:example.com:alpha");
        ttrue(!websCheckPassword("user:example.com:alpha", damaged),
              "a record with rounds \"%s\" refuses the original password", bad[i]);
        ttrue(!websCheckPassword("user:example.com:bravo", damaged),
              "a record with rounds \"%s\" refuses another password", bad[i]);
        wfree(damaged);
    }
    wfree(record);
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("Authentication is not enabled");
    return 0;
}
#endif
