/* Detailed interpreter diagnostics remain available to native callers, not HTTP clients. */
#include "testme.h"
#include "goahead.h"
#include "js.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    char deep[512], large[4100], *cp, *error, *result;
    cchar *scripts[6];
    cchar *messages[] = {"Script recursion too deep", "Token too big", "Undefined procedure while",
        "Syntax error", "Variable already declared"};
    int jid, i;

    strcpy(deep, "var a=");
    cp = deep + strlen(deep);
    for (i=0; i<150; i++) *cp++ = '(';
    *cp++ = '1';
    for (i=0; i<150; i++) *cp++ = ')';
    strcpy(cp, ";");
    strcpy(large, "var a=\"");
    cp = large + strlen(large);
    memset(cp, 'a', 4000);
    strcpy(cp + 4000, "\";");
    scripts[0] = deep;
    scripts[1] = large;
    scripts[2] = "while (1) {}";
    scripts[3] = "var i=5; ++i;";
    scripts[4] = "var z; var z;";
    scripts[5] = NULL;
    for (i=0; scripts[i]; i++) {
        jid = jsOpenEngine(-1, -1);
        jsOpenBlock(jid);
        error = NULL;
        result = jsEval(jid, scripts[i], &error);
        tnull(result, "Invalid script fails");
        tnotnull(error, "Native caller receives diagnostic");
        if (error) {
            tcontains(error, messages[i], "Detailed diagnostic is preserved");
        }
        wfree(error);
        jsCloseEngine(jid);
    }
#endif
    return 0;
}
