/*
    url-decode.tst.c - Query decoding and request path validation, byte for byte
 */

#include "testme.h"
#include "goahead.h"

static void decodes(cchar *input, cchar *expected)
{
    char buf[128];

    scopy(buf, sizeof(buf), input);
    websDecodeUrl(buf, buf, -1);
    ttrue(smatch(buf, expected), "websDecodeUrl(\"%s\") must give \"%s\", got \"%s\"", input, expected, buf);
}


static void validates(cchar *path, cchar *expected)
{
    char *result;

    result = websValidateUriPath(path);
    if (expected) {
        ttrue(result && smatch(result, expected), "\"%s\" must validate to \"%s\", got \"%s\"", path, expected,
              result ? result : "refused");
    } else {
        ttrue(result == 0, "\"%s\" must be refused, got \"%s\"", path, result ? result : "");
    }
    wfree(result);
}


static void suiteA_decode(void)
{
    tinfo("\n--- Suite A: websDecodeUrl ---\n");

    decodes("a+b", "a b");
    decodes("a%20b", "a b");
    decodes("a%2Bb", "a+b");
    decodes("a%2bb", "a+b");
    decodes("%41%62%63", "Abc");
    decodes("caf%C3%A9", "caf\xc3\xa9");
    decodes("%E2%82%AC", "\xe2\x82\xac");
    decodes("a%26b%3Dc", "a&b=c");
    decodes("%25", "%");
    decodes("%2541", "%41");

    decodes("a%", "a%");
    decodes("a%2", "a%2");
    decodes("%zz", "%zz");
    decodes("%g1", "%g1");
    decodes("%1g", "%1g");

    decodes("a%00b", "a%00b");
    decodes("%00", "%00");

    //  Other control characters are data here; validation of their use is the caller's.
    decodes("a%0Ab", "a\nb");

    /*
        The destination size includes the terminator.
     */
    {
        char input[] = "a%20b%20c", out[8];

        memset(out, 'Z', sizeof(out));
        websDecodeUrl(out, input, 4);
        ttrue(smatch(out, "a b"), "a 4-byte buffer must hold \"a b\", got \"%s\"", out);
        ttrue(out[4] == 'Z', "nothing may be written past a 4-byte buffer");

        memset(out, 'Z', sizeof(out));
        websDecodeUrl(out, input, 1);
        ttrue(out[0] == '\0' && out[1] == 'Z', "a 1-byte buffer holds only the terminator");

        memset(out, 'Z', sizeof(out));
        websDecodeUrl(out, input, 0);
        ttrue(out[0] == 'Z', "a 0-byte buffer is not written");

        websDecodeUrl(input, input, -1);
        ttrue(smatch(input, "a b c"), "-1 must decode in place, got \"%s\"", input);
    }
}


static void suiteB_validate(void)
{
    tinfo("\n--- Suite B: websValidateUriPath ---\n");

    validates("/a+b", "/a+b");
    validates("/a%20b", "/a b");
    validates("/a%2Bb", "/a+b");
    validates("/caf%C3%A9", "/caf\xc3\xa9");
    validates("/a%25b", "/a%b");
    validates("/a%2541", "/a%41");

    validates("/a%2Fb", "/a/b");

    validates("/a/%2e%2e/b", "/b");

    validates("/a%01b", 0);
    validates("/a%0Ab", 0);
    validates("/a%7Fb", 0);

    validates("/a%00b", 0);
    validates("/a%2500b", 0);
    validates("/index.html%00.txt", 0);

    validates("/a%252e%252e/b", 0);
    validates("/a%252Fb", 0);
    validates("/a%252fb", 0);
    validates("/a%255Cb", 0);

    validates("/a%5Cb", 0);
}


int main(int argc, char **argv)
{
    suiteA_decode();
    suiteB_validate();
    return 0;
}
