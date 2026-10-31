/*
    lexer-eof-putback.tst.c - A script whose last token is an identifier or a number evaluates, and a diagnostic
    holds only bytes of the script: the lexer must not put the end-of-input marker back as a byte (js.c inputPutback)
 */
#include "testme.h"
#include "goahead.h"
#include "js.h"

#if ME_GOAHEAD_JAVASCRIPT
static int hasHighByte(cchar *s)
{
    for (; s && *s; s++) {
        if ((uchar) *s >= 0x80) {
            return 1;
        }
    }
    return 0;
}

static void check(int jid, cchar *script, cchar *expect)
{
    char *result, *error;

    error = NULL;
    result = jsEval(jid, script, &error);
    tnull(error, "[%s] must evaluate (got: %s)", script, error ? error : "");
    tnotnull(result, "[%s] must yield a result", script);
    if (result) {
        tmatch(result, expect, "[%s]", script);
    }
    wfree(error);
}
#endif

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    char *result, *error;
    int  jid;

    jid = jsOpenEngine(-1, -1);

    //  A trailing number or identifier with no terminator is the end of the statement
    check(jid, "x = 5", "5");
    check(jid, "var y = 7; y", "7");
    check(jid, "5", "5");
    check(jid, "x = 5 ", "5");
    check(jid, "x = 5;", "5");

    //  A trailing undefined identifier reports itself, not a byte the lexer invented
    error = NULL;
    result = jsEval(jid, "abc", &error);
    tnull(result, "an undefined trailing identifier must fail");
    tnotnull(error, "an undefined trailing identifier must report an error");
    if (error) {
        tcontains(error, "Undefined variable abc", "the diagnostic names the identifier");
        ttrue(!hasHighByte(error), "the diagnostic holds only bytes of the script");
    }
    wfree(error);
    jsCloseEngine(jid);
#else
    tskip("JavaScript templates are not in this build");
#endif
    return 0;
}
