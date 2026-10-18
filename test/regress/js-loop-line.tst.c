/* Rewinding a template loop rewinds the line kept for error reports */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#include "js.h"

#if ME_GOAHEAD_JAVASCRIPT

static void testOneLine(int jid)
{
    char *result, *error;

    error = NULL;
    result = jsEval(jid, "for (i = 0; i < 5000; i++) { x = i; } nosuch();", &error);
    ttrue(result == NULL && error, "the error after the loop is reported");
    ttrue(error && slen(error) < 512, "the reported line is the source line, got %d bytes", (int) slen(error));
    wfree(error);
}


static void testLineNumber(int jid)
{
    char *result, *error;

    error = NULL;
    result = jsEval(jid, "var x = 0;\nfor (i = 0; i < 100; i++) {\n    x = i;\n}\nnosuch();", &error);
    ttrue(result == NULL && error, "the error after the loop is reported");
    ttrue(error && strstr(error, "At line 5,"), "the error names line 5, got: %s", error ? error : "");
    wfree(error);
}


static void testLoopsStillRun(int jid)
{
    char *result, *error;

    error = NULL;
    result = jsEval(jid, "var n = 0;\nfor (i = 0; i < 4; i++) {\n    for (j = 0; j < 3; j++) {\n"
                    "        n = n + 1;\n    }\n}\nn;", &error);
    ttrue(result && smatch(result, "12"), "nested multi-line loops still run, got %s", result ? result : error);
    wfree(error);
}


int main(int argc, char **argv)
{
    int jid;

    jid = jsOpenEngine(-1, -1);
    ttrue(jid >= 0, "open the engine");
    testOneLine(jid);
    testLineNumber(jid);
    testLoopsStillRun(jid);
    jsCloseEngine(jid);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("JavaScript templates are not enabled");
    return 0;
}
#endif
