/*
    blocks.tst.c -- Bare-block syntax errors must terminate and preserve engine reuse.
 */

#include "testme.h"
#include "goahead.h"
#include "js.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    cchar *scripts[] = {
        "{+1", "{*", "{,", "{!a", "{=1", "{++a", "{else", "{{+1",
        "{+}", "{{+1}}", "if (0) {+1}", "for (i = 0; i < 0; i++) {+1}", NULL
    };
    char  *result, *error;
    int   jid, i;

    jid = jsOpenEngine(-1, -1);
    for (i = 0; scripts[i]; i++) {
        error = NULL;
        result = jsEval(jid, scripts[i], &error);
        tnull(result, "Malformed block must fail: %s", scripts[i]);
        tnotnull(error, "Malformed block must report an error: %s", scripts[i]);
        if (error) {
            tcontains(error, "Syntax error", "Malformed block diagnostic");
        }
        wfree(error);
        error = NULL;
        result = jsEval(jid, "{var a = 1 + 1; a;}", &error);
        tnull(error, "Engine must accept a valid block after an error");
        tnotnull(result, "Valid block must produce a result");
        if (result) {
            tmatch(result, "2", "Valid block result");
        }
        wfree(error);
    }
    jsCloseEngine(jid);
#endif
    return 0;
}
