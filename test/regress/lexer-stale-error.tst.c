/*
    lexer-stale-error.tst.c - An evaluation reports its own failure: an oversized identifier is refused where it
    occurs, and a message left by an earlier evaluation never describes a later one (js.c jsEval, getLexicalToken)
 */
#include "testme.h"
#include "goahead.h"
#include "js.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    char big[2200], script[2300], *result, *error;
    int  jid;

    jid = jsOpenEngine(-1, -1);
    memset(big, 'a', 2100);
    big[2100] = '\0';

    //  An oversized string literal is refused with the token diagnostic (as test/jst/diagnostics.tst.c pins)
    snprintf(script, sizeof(script), "var s = \"%s\";", big);
    error = NULL;
    result = jsEval(jid, script, &error);
    tnull(result, "an oversized literal must fail");
    tnotnull(error, "an oversized literal must report an error");
    if (error) {
        tcontains(error, "Token too big", "the literal is refused as too big");
    }
    wfree(error);

    //  A later syntax error on the same engine describes itself, not the earlier failure
    error = NULL;
    result = jsEval(jid, "f(", &error);
    tnull(result, "an unterminated call must fail");
    tnotnull(error, "an unterminated call must report an error");
    if (error) {
        ttrue(strstr(error, "Token too big") == NULL, "a later failure must not carry an earlier message, got: %s",
              error);
        tcontains(error, "Syntax error", "the unterminated call is a syntax error");
    }
    wfree(error);

    //  An oversized identifier is refused as such rather than split and run as two identifiers
    snprintf(script, sizeof(script), "%s = 1;", big);
    error = NULL;
    result = jsEval(jid, script, &error);
    tnull(result, "an oversized identifier must fail");
    tnotnull(error, "an oversized identifier must report an error");
    if (error) {
        tcontains(error, "Token too big", "the identifier is refused as too big");
    }
    wfree(error);

    //  The engine stays usable and a clean evaluation carries no message
    error = NULL;
    result = jsEval(jid, "var ok = 1; ok;", &error);
    tnull(error, "a valid script after errors must evaluate");
    if (result) {
        tmatch(result, "1", "the engine is still usable");
    } else {
        tnotnull(result, "a valid script has a result");
    }
    wfree(error);
    jsCloseEngine(jid);
#else
    tskip("JavaScript templates are not in this build");
#endif
    return 0;
}
