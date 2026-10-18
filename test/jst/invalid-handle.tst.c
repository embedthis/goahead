/* Invalid engine handles and engines with no active script must be safe. */
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
int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    int jid, rc;
    char *result, *message = NULL;
    rc = jsGetLineNumber(-1);
    teq(rc, -1, "invalid handle returns an error");
    jid = jsOpenEngine(-1, -1);
    ttrue(jid >= 0, "create engine");
    rc = jsGetLineNumber(jid);
    teq(rc, -1, "no active script has no line number");
    result = jsEval(jid, "var x=1;", &message);
    tnotnull(result, "valid script still evaluates");
    tnull(message, "valid script has no error");
    jsCloseEngine(jid);
    rc = jsGetLineNumber(jid);
    teq(rc, -1, "closed handle returns an error");
    jsCloseEngine(jid);
    ttrue(1, "repeated close is safe");
#else
    tskip("Javascript disabled");
#endif
    return 0;
}
