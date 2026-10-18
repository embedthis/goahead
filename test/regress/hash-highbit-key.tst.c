/*
    hash-highbit-key.tst.c - Regression: a key byte with the high bit set must not break hashing.

    hashIndex sums each key byte shifted left by a rotating amount. It promoted the byte through a
    signed char, so on the usual signed-char platforms a byte in 0x80-0xFF sign-extended to a
    negative int and the shift was undefined (C11 6.5.7p4): 0xE2 became -30, and -30 << 21 has no
    defined result. The key is client-supplied - a percent-decoded query parameter name, or a
    multipart field name - so any request could reach it unauthenticated.

    Two properties are asserted. First, every byte value must round-trip: what hashEnter stored,
    hashLookup must find and hashDelete must remove, for all 128 high-bit values and across keys
    long enough to exercise every shift amount in the cycle. Second, the fix must be inert for
    ASCII: the iteration order of a fixed ASCII key set is a direct function of bucket placement,
    and it is pinned here against the order recorded before the fix was applied, so a change that
    perturbed existing tables would fail rather than pass quietly.

    A UBSan build (-fsanitize=undefined) reports the defect on the first high-bit insert; a plain
    build does not, which is why the round-trip assertions carry the test on their own.
 */

#include "testme.h"
#include "goahead.h"

/*
    Bucket placement recorded from the unfixed code, in hashFirst/hashNext order at table size 67.
    Every key is pure ASCII, so the unsigned promotion must not move any of them. 0x7F is the last
    byte the old code handled correctly and is included as the boundary next to 0x80.
 */
static cchar *asciiKeys[] = {
    "HTTP_HOST", "QUERY_STRING", "REMOTE_ADDR", "REQUEST_METHOD", "SERVER_PORT",
    "CONTENT_TYPE", "a", "zz", "index.html", "AUTH_TYPE",
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcd",
    "\x7f", "~", "!", NULL
};

static cchar *asciiOrder[] = {
    "REQUEST_METHOD", "QUERY_STRING", "SERVER_PORT", "a", "CONTENT_TYPE",
    "!", "index.html", "REMOTE_ADDR",
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcd",
    "AUTH_TYPE", "~", "HTTP_HOST", "zz", "\x7f", NULL
};

/*
    Multi-byte keys spanning the shift cycle. hashIndex advances the shift by 7 each byte and wraps
    at 24, so a single-byte key only ever exercises shift 0. These reach 7, 14 and 21, where a
    sign-extended value did the most damage.
 */
static cchar *multiByte[] = {
    "\xe2\x82\xac",                     /* EURO SIGN, UTF-8 */
    "caf\xc3\xa9",                      /* high bytes behind ASCII, shifts 21 and 0 */
    "\xff\xff\xff\xff\xff",             /* every byte maximal, wraps the cycle */
    "\x80\x01\x80\x01\x80\x01",         /* alternating high and low */
    "a\xe2",                            /* high byte at shift 7 */
    "ab\xe2",                           /* high byte at shift 14 */
    "abc\xe2",                          /* high byte at shift 21 */
    "abcd\xe2",                         /* high byte back at shift 0 */
    NULL
};

/*
    Enter, look up, and delete one key, asserting the table agrees with itself at each step. This is
    the property the defect threatened: hashEnter, hashLookup and hashDelete all reach the same
    hashIndex, so a key must resolve to the bucket it was stored in.
 */
static void roundTrip(WebsHash sd, cchar *key, cchar *label)
{
    WebsKey *kp;

    ttrue(hashEnter(sd, key, valueString("v", 0), 0) != 0, "%s: hashEnter must succeed", label);
    kp = hashLookup(sd, key);
    ttrue(kp != 0, "%s: hashLookup must find the entry just stored", label);
    ttrue(hashDelete(sd, key) == 0, "%s: hashDelete must remove the entry", label);
    ttrue(hashLookup(sd, key) == 0, "%s: entry must be gone after delete", label);
}

int main(int argc, char **argv)
{
    WebsHash sd;
    WebsKey  *kp;
    char     key[3];
    int      i, n;

    tinfo("hash-highbit-key: a high-bit key byte must hash without undefined behavior\n");

    websRuntimeOpen();

    /*
        Every high-bit byte, alone. 0x80-0xFF are exactly the values that sign-extended to negative.
     */
    sd = hashCreate(67);
    ttrue(sd >= 0, "hashCreate must succeed");
    key[1] = '\0';
    for (i = 0x80; i <= 0xff; i++) {
        key[0] = (char) i;
        roundTrip(sd, key, "single high byte");
    }

    /*
        The boundary. 0x7F was always safe and 0x80 was the first value that was not; both must work
        and must remain distinct keys.
     */
    key[0] = (char) 0x7f;
    ttrue(hashEnter(sd, key, valueString("low", 0), 0) != 0, "0x7F must enter");
    key[0] = (char) 0x80;
    ttrue(hashEnter(sd, key, valueString("high", 0), 0) != 0, "0x80 must enter");
    key[0] = (char) 0x7f;
    kp = hashLookup(sd, key);
    ttrue(kp != 0 && smatch(kp->content.value.string, "low"), "0x7F must not collide with 0x80");
    key[0] = (char) 0x80;
    kp = hashLookup(sd, key);
    ttrue(kp != 0 && smatch(kp->content.value.string, "high"), "0x80 must keep its own value");
    hashFree(sd);

    /*
        Multi-byte keys, so the high byte lands at each shift amount in the cycle.
     */
    sd = hashCreate(67);
    for (i = 0; multiByte[i]; i++) {
        roundTrip(sd, multiByte[i], "multi-byte key");
    }
    hashFree(sd);

    /*
        A lookup for a high-bit key that was never stored must miss rather than match something else.
     */
    sd = hashCreate(67);
    hashEnter(sd, "\xe2", valueString("v", 0), 0);
    ttrue(hashLookup(sd, "\xe3") == 0, "an absent high-bit key must not match a stored one");
    ttrue(hashLookup(sd, "\xe2\xe2") == 0, "a longer high-bit key must not match a shorter one");
    hashFree(sd);

    /*
        ASCII bucket placement must be untouched. Iteration walks the bucket array in order, so this
        order is the placement; it is the sequence recorded from the unfixed code.
     */
    sd = hashCreate(67);
    for (i = 0; asciiKeys[i]; i++) {
        hashEnter(sd, asciiKeys[i], valueString("v", 0), 0);
    }
    n = 0;
    for (kp = hashFirst(sd); kp; kp = hashNext(sd, kp), n++) {
        ttrue(asciiOrder[n] != 0, "iteration must not yield more keys than were entered");
        if (asciiOrder[n] == 0) {
            break;
        }
        ttrue(smatch(kp->name.value.string, asciiOrder[n]),
              "ASCII bucket order changed at %d: got [%s], expected [%s]",
              n, kp->name.value.string, asciiOrder[n]);
    }
    ttrue(asciiOrder[n] == 0, "iteration must yield every key that was entered");
    hashFree(sd);

    return 0;
}
