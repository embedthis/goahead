/*
    error-value.tst.c -- A script error names the length of the offending value, never its text, which may be request
    data a client chose.
 */
#include "testme.h"
#include "goahead.h"
#include "js.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    cchar *scripts[] = { "if (secret) { a = 1; }", "a = secret < 5;", NULL };
    char  *result, *error;
    int   jid, i;

    jid = jsOpenEngine(-1, -1);
    jsSetVar(jid, "secret", "PRIVATE-VALUE");
    for (i = 0; scripts[i]; i++) {
        error = NULL;
        result = jsEval(jid, scripts[i], &error);
        tnull(result, "a non-numeric operand must fail");
        tnotnull(error, "the failure must be reported");
        if (error) {
            ttrue(strstr(error, "PRIVATE-VALUE") == 0, "the error must not hold the value: %s", error);
            tcontains(error, "13 byte value", "the error must give the value's length");
        }
        wfree(error);
    }
    jsCloseEngine(jid);
#else
    tskip("JavaScript is not enabled");
#endif
    return 0;
}
