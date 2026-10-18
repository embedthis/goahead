/*
    format-alternate-string.tst - "%#s" prints the string, it does not read past it
 */
#include    "testme.h"
#include    "goahead.h"

int main(int argc, char **argv)
{
    char  *result, *one;
    ssize i;

    /*
        Allocate a one-byte empty string so ASan detects over-read.
     */
    one = walloc(1);
    one[0] = '\0';
    result = sfmt("%#s", one);
    tnotnull(result, "the formatter must return a result");
    if (result) {
        tmatch(result, "", "a one-byte empty string must print as nothing");
        wfree(result);
    }
    wfree(one);

    result = sfmt("%#s", "hello");
    tmatch(result, "hello", "an alternate-flagged string must print in full");
    wfree(result);

    result = sfmt("[%s][%#s]", "abc", "abc");
    tmatch(result, "[abc][abc]", "the alternate flag must not change what %%s prints");
    wfree(result);

    /*
        Check that high-bit bytes do not produce negative lengths.
     */
    result = sfmt("%#s", "\xFE\xFFtail");
    tmatch(result, "\xFE\xFFtail", "a high leading byte must not be read as a length");
    wfree(result);

    result = sfmt("[%#8s]", "ab");
    tmatch(result, "[      ab]", "width must still pad an alternate-flagged string");
    wfree(result);

    result = sfmt("[%-#8s]", "ab");
    tmatch(result, "[ab      ]", "left alignment must still apply");
    wfree(result);

    /*
        Repeat the string conversion to check each argument's bounds.
     */
    result = sfmt("Content-Type: %#s%#s%#s", "a", "b", "c");
    tmatch(result, "Content-Type: abc", "repeated alternate strings must print only themselves");
    wfree(result);

    result = sfmt("%#x", 255);
    tmatch(result, "0xff", "the alternate flag must still prefix hex");
    wfree(result);

    result = sfmt("%#o", 8);
    tmatch(result, "010", "the alternate flag must still prefix octal");
    wfree(result);

    result = sfmt("%.2s", "abcdef");
    tmatch(result, "ab", "precision must still truncate");
    wfree(result);

    result = sfmt("%#s", NULL);
    tmatch(result, "null", "a null argument must still print null");
    wfree(result);

    /*
        Vary lengths to detect mismatches independently of allocator placement.
     */
    for (i = 1; i < 32; i++) {
        char *probe = walloc(i + 1);
        memset(probe, 'x', i);
        probe[i] = '\0';
        result = sfmt("%#s", probe);
        teqz(slen(result), i, "an alternate-flagged string prints its own length");
        wfree(result);
        wfree(probe);
    }
    return 0;
}
