/*
    blowfish-key-window.tst.c - Regression: password material must never fall outside the Blowfish
    key schedule window.

    The Blowfish schedule consumes exactly WEBS_BLOWFISH_KEY_WINDOW key bytes and ignores the rest.
    Password hashes are keyed with "salt:username:realm:password", so once the three known fields
    and their separators fill the window the password contributes nothing: the stored hash does not
    depend on the password it claims to check, and every candidate matches.

    New hashes are written in the BF2 format, which keys the schedule with a fixed-width digest of
    the composite, so no length of username or realm can crowd the password out. Legacy BF1 hashes
    keep verifying below the window and are refused at it, because a BF1 hash at the window is an
    open door rather than a credential.
 */

#include "testme.h"
#include "goahead.h"

/*
    The realm the server binds into the hashed value. Matches ME_GOAHEAD_REALM's default; the test
    composes credentials itself rather than depending on the build's realm length.
 */
#define TEST_REALM  "example.com"

/*
    A 42-character username. With the 11-character realm the two sum to 53, which is exactly the
    point at which the salt, the username, the realm and their separators fill the 72-byte window.
 */
#define LONG_USER   "bf1cliffuser000000000000000000000000000000"

#define SHORT_USER  "testbf1"

/*
    A fixed 16-character salt, the length websMakePassword issues by default.
 */
#define TEST_SALT   "1E1QncXLrNwc2SO1"

#define TEST_ROUNDS 128

/*
    Compose the credential string that auth.c and gopass hash: "username:realm:password".
 */
static char *credential(cchar *username, cchar *password)
{
    return sfmt("%s:%s:%s", username, TEST_REALM, password);
}


/*
    Build a legacy BF1 stored hash for a credential, the way websMakePassword wrote them before the
    BF2 format. websCryptPassword still implements the BF1 construction, so this produces a hash
    byte-identical to one already deployed on a device.
 */
static char *makeLegacyPassword(cchar *username, cchar *password)
{
    char *credentials, *hash, *result;

    credentials = credential(username, password);
    hash = websCryptPassword(credentials, TEST_SALT, TEST_ROUNDS);
    result = sfmt("%s:%05d:%s:%s", WEBS_CRYPT_BLOWFISH, TEST_ROUNDS, TEST_SALT, hash);
    wfree(hash);
    wfree(credentials);
    return result;
}


/*
    Check a password against a stored hash, composing the credential the way the server does.
 */
static bool checkCredential(cchar *username, cchar *password, cchar *stored)
{
    char *credentials;
    bool success;

    credentials = credential(username, password);
    success = websCheckPassword(credentials, stored);
    wfree(credentials);
    return success;
}


int main(int argc, char **argv)
{
    char *stored, *legacy, *credentials, *right, *wrong;

    tinfo("blowfish-key-window: password material must stay inside the key schedule window\n");

    /*
        The truncation itself. Two credentials for the same long username differ only in the
        password, and the password begins beyond the window, so the raw BF1 primitive cannot tell
        them apart. This is the defect BF2 removes and the reason a BF1 hash here cannot be trusted.
     */
    credentials = credential(LONG_USER, "pass1");
    right = websCryptPassword(credentials, TEST_SALT, 8);
    wfree(credentials);
    credentials = credential(LONG_USER, "totallydifferent");
    wrong = websCryptPassword(credentials, TEST_SALT, 8);
    wfree(credentials);
    ttrue(smatch(right, wrong), "BF1 must be shown to ignore password bytes past the key window");
    wfree(right);
    wfree(wrong);

    /*
        New hashes are written in the BF2 format.
     */
    credentials = credential(SHORT_USER, "pass1");
    stored = websMakePassword(credentials, 16, TEST_ROUNDS);
    wfree(credentials);
    ttrue(stored != 0, "websMakePassword must produce a hash");
    ttrue(sstarts(stored, WEBS_CRYPT_BLOWFISH2 ":"), "new hashes must be written in the BF2 format");

    /*
        BF2 below the window: unchanged behaviour, the correct password authenticates.
     */
    ttrue(checkCredential(SHORT_USER, "pass1", stored), "BF2 must accept the correct password");
    ttrue(!checkCredential(SHORT_USER, "wrongpass", stored), "BF2 must reject a wrong password");
    ttrue(!checkCredential(SHORT_USER, "", stored), "BF2 must reject an empty password");
    wfree(stored);

    /*
        BF2 with a username and realm that would have filled the window. The digest is fixed width,
        so there is no cliff: the correct password still authenticates and nothing else does.
     */
    credentials = credential(LONG_USER, "pass1");
    stored = websMakePassword(credentials, 16, TEST_ROUNDS);
    wfree(credentials);
    ttrue(stored != 0, "websMakePassword must produce a hash for a long username");
    ttrue(checkCredential(LONG_USER, "pass1", stored), "BF2 must accept the correct password at the BF1 cliff");
    ttrue(!checkCredential(LONG_USER, "wrongpass", stored), "BF2 must reject a wrong password at the BF1 cliff");
    ttrue(!checkCredential(LONG_USER, "", stored), "BF2 must reject an empty password at the BF1 cliff");
    ttrue(!checkCredential(LONG_USER, "totallydifferent", stored),
          "BF2 must reject a wrong password of a different length at the BF1 cliff");
    wfree(stored);

    /*
        A legacy BF1 hash at the window must fail closed. It never depended on the password, so
        refusing it removes an open door rather than a working login - the nominally correct
        password must be refused along with everything else.
     */
    legacy = makeLegacyPassword(LONG_USER, "pass1");
    ttrue(!checkCredential(LONG_USER, "pass1", legacy), "BF1 at the key window must refuse the nominal password");
    ttrue(!checkCredential(LONG_USER, "wrongpass", legacy), "BF1 at the key window must refuse a wrong password");
    ttrue(!checkCredential(LONG_USER, "", legacy), "BF1 at the key window must refuse an empty password");
    wfree(legacy);

    /*
        A legacy BF1 hash below the window keeps working. Deployed credentials that do depend on
        their password must not be broken by the fix.
     */
    legacy = makeLegacyPassword(SHORT_USER, "pass1");
    ttrue(checkCredential(SHORT_USER, "pass1", legacy), "BF1 below the key window must accept the correct password");
    ttrue(!checkCredential(SHORT_USER, "wrongpass", legacy), "BF1 below the key window must reject a wrong password");
    ttrue(!checkCredential(SHORT_USER, "", legacy), "BF1 below the key window must reject an empty password");
    wfree(legacy);

    /*
        A stored hash naming neither construction must be refused rather than verified as BF1.
     */
    ttrue(!checkCredential(SHORT_USER, "pass1", "XX9:00128:" TEST_SALT ":AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"),
          "an unknown hash format must be refused");
    ttrue(!checkCredential(SHORT_USER, "pass1", "BF1:00128:missingfields"), "a malformed hash must be refused");

    /*
        A salt long enough to push the digest out of the window must be refused rather than
        silently truncated.
     */
    credentials = credential(SHORT_USER, "pass1");
    ttrue(websMakePassword(credentials, WEBS_BLOWFISH_KEY_WINDOW, TEST_ROUNDS) == 0,
          "an over-long salt must be refused rather than truncated");
    wfree(credentials);

    return 0;
}
