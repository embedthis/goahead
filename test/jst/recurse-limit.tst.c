/*
    recurse-limit.tst.c - Template nesting is limited by ME_GOAHEAD_LIMIT_SCRIPT_RECURSE
 */

#define ME_GOAHEAD_LIMIT_SCRIPT_RECURSE 12

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/js.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_JAVASCRIPT
static char *nested(int jid, int depth, char **error)
{
    char script[256], *cp;
    int  i;

    cp = script;
    cp += scopy(cp, 8, "a = ");
    for (i = 0; i < depth; i++) {
        *cp++ = '(';
    }
    *cp++ = '1';
    for (i = 0; i < depth; i++) {
        *cp++ = ')';
    }
    scopy(cp, 4, ";");
    *error = NULL;
    return jsEval(jid, script, error);
}


static void suiteA_limit(int jid)
{
    char *result, *error;

    tinfo("\n--- Suite A: the limit ---\n");
    result = nested(jid, 2, &error);
    ttrue(result && smatch(result, "1"), "nesting within the limit must evaluate");
    wfree(error);

    result = nested(jid, 40, &error);
    ttrue(result == NULL, "nesting past a limit of 12 must be refused");
    ttrue(error && strstr(error, "Script recursion too deep"), "the refusal must name the recursion limit");
    wfree(error);
}


static void suiteB_loops(int jid)
{
    char *result, *error;

    tinfo("\n--- Suite B: for statements ---\n");
    error = NULL;
    result = jsEval(jid, "var n = 0; for (i = 0; i < 3; i++) { n = n + 1; } n;", &error);
    ttrue(result && smatch(result, "3"), "a for statement must loop, got %s", result ? result : error);
    wfree(error);

    error = NULL;
    result = jsEval(jid, "var m = 0; for (i = 0; i < 2; i++) { for (j = 0; j < 3; j++) { m = m + 1; } } m;", &error);
    ttrue(result && smatch(result, "6"), "nested for statements must loop, got %s", result ? result : error);
    wfree(error);

    error = NULL;
    result = jsEval(jid, "for (i = 0; i < 2; i++) { +", &error);
    ttrue(result == NULL, "a for statement with a malformed body must fail cleanly");
    wfree(error);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    int jid;

    jid = jsOpenEngine(-1, -1);
    suiteA_limit(jid);
    suiteB_loops(jid);
    jsCloseEngine(jid);
#else
    tskip("JavaScript templates are not enabled");
#endif
    return 0;
}
