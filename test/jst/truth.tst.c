/* Template conditions follow one truth rule in if, for, && and || */
#include "testme.h"
#include "goahead.h"
#include "js.h"

#if ME_GOAHEAD_JAVASCRIPT

static cchar *scripts[] = {
    "var r = 0; if (v) { r = 1; } r;",
    "var n = 0; for (i = 0; v; i++) { n++; v = 0; } n;",
    "var r = v || 0; r;",
    "var r = v && 1; r;",
    "var r = 1; if (0) { if (v) { r = 2; } } r;",
    NULL
};

static void check(int jid, cchar *value, cchar *script, cchar *expect)
{
    char *result, *error;

    error = NULL;
    jsSetVar(jid, "v", value);
    result = jsEval(jid, script, &error);
    if (expect) {
        tnull(error, "v=\"%s\": %s must evaluate", value, script);
        tmatch(result, expect, "v=\"%s\": %s", value, script);
    } else {
        tnull(result, "v=\"%s\": %s must fail", value, script);
        tnotnull(error, "v=\"%s\": %s must report an error", value, script);
    }
    wfree(error);
}

//  A numeric value (1, 2, 10, +1, " 1", 01, -1) is true
static cchar *trueValues[] = { "1", "2", "10", "+1", " 1", "01", "-1", "1 ", NULL };

//  Zero and an empty value are false
static cchar *falseValues[] = { "0", "-0", "", "00", " 0", NULL };

//  Anything else is an error
static cchar *badValues[] = { "abc", "1x", " ", "x1", "1.5", NULL };
#endif

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    int jid, i, s;

    jid = jsOpenEngine(-1, -1);
    for (i = 0; trueValues[i]; i++) {
        check(jid, trueValues[i], scripts[0], "1");
        check(jid, trueValues[i], scripts[1], "1");
        check(jid, trueValues[i], scripts[2], "1");
        check(jid, trueValues[i], scripts[3], "1");
    }
    for (i = 0; falseValues[i]; i++) {
        check(jid, falseValues[i], scripts[0], "0");
        check(jid, falseValues[i], scripts[1], "0");
        check(jid, falseValues[i], scripts[2], "0");
        check(jid, falseValues[i], scripts[3], "0");
    }
    for (i = 0; badValues[i]; i++) {
        for (s = 0; s < 4; s++) {
            check(jid, badValues[i], scripts[s], NULL);
        }
        //  The untaken branch is not evaluated
        check(jid, badValues[i], scripts[4], "1");
    }
    //  The right operand is tested too
    jsSetVar(jid, "w", "abc");
    check(jid, "1", "var r = v || w; r;", NULL);
    check(jid, "1", "var r = v && w; r;", NULL);
    check(jid, "1", "var r = w || v; r;", NULL);
    jsCloseEngine(jid);
#else
    tskip("JavaScript templates are not in this build");
#endif
    return 0;
}
