/*
    budget.tst.c -- Script work and concatenation limits, including engine reuse.
 */
#include "testme.h"
#include "goahead.h"
#include "js.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    cchar *scripts[] = {
        "for (i=0;i<200000;i++) {}",
        "var s='x'; for (i=0;i<18;i++) {s=s+s;}", NULL
    };
    cchar *messages[] = {"Script step limit exceeded", "Script result limit exceeded"};
    char *result, *error;
    int jid, i;

    jid = jsOpenEngine(-1, -1);
    for (i = 0; scripts[i]; i++) {
        error = NULL;
        result = jsEval(jid, scripts[i], &error);
        tnull(result, "Excessive script must fail");
        tnotnull(error, "Excessive script must report an error");
        if (error) {
            tcontains(error, messages[i], "Limit diagnostic");
        }
        wfree(error);
        error = NULL;
        result = jsEval(jid, "var a=0; for (i=0;i<10;i++) {a=a+1;} a;", &error);
        tnull(error, "Budget resets for the next evaluation");
        if (result) {
            tmatch(result, "10", "Normal loop remains usable");
        } else {
            tnotnull(result, "Normal loop has a result");
        }
        wfree(error);
    }
    error = NULL;
    result = jsEval(jid, "var s='x'; for (i=0;i<16;i++) {s=s+s;} s;", &error);
    tnull(error, "A concatenated result at the default limit succeeds");
    tnotnull(result, "Boundary result exists");
    if (result) {
        teqi(slen(result), 65536, "Exact result limit is accepted");
    }
    wfree(error);
    jsCloseEngine(jid);
#endif
    return 0;
}
