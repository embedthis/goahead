/* Template comparisons with a number compare numerically */
#include "testme.h"
#include "goahead.h"
#include "js.h"

#if ME_GOAHEAD_JAVASCRIPT
typedef struct Case {
    cchar *value;
    cchar *expr;
    cchar *result;
} Case;

static void check(int jid, Case *c)
{
    char *result, *error, *script;

    error = NULL;
    jsSetVar(jid, "v", c->value);
    script = sfmt("var r = %s; r;", c->expr);
    result = jsEval(jid, script, &error);
    wfree(script);
    if (c->result) {
        tnull(error, "v=\"%s\": %s must evaluate", c->value, c->expr);
        tmatch(result, c->result, "v=\"%s\": %s", c->value, c->expr);
    } else {
        tnull(result, "v=\"%s\": %s must fail", c->value, c->expr);
        tnotnull(error, "v=\"%s\": %s must report an error", c->value, c->expr);
    }
    wfree(error);
}
#endif

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    Case cases[] = {
        //  A bound check must hold for a signed or space-padded number
        { " 5000", "v <= 100", "0" },
        { "+5000", "v <= 100", "0" },
        { "5000 ", "v <= 100", "0" },
        { "-5", "v < 1", "1" },
        { "-5", "v > \"-10\"", "1" },
        { " 7", "v == 7", "1" },
        { "9", "v < 10", "1" },


        { "5000x", "v <= 100", 0 },
        { "abc", "v > 5", 0 },

        //  An empty value is zero beside a number, and a string beside a string
        { "", "v < 1", "1" },
        { "", "v == 0", "1" },
        { "bob", "v == \"\"", "0" },
        { "", "v == \"\"", "1" },

        //  Two strings compare as strings
        { "apple", "v < \"banana\"", "1" },
        { "apple", "v == \"apple\"", "1" },

        //  + adds numbers and joins anything else
        { " 12", "v + 3", "15" },
        { "a", "v + 3", "a3" },
        { 0, 0, 0 }
    };
    int jid, i;

    jid = jsOpenEngine(-1, -1);
    for (i = 0; cases[i].value; i++) {
        check(jid, &cases[i]);
    }
    jsCloseEngine(jid);
#else
    tskip("JavaScript templates are not in this build");
#endif
    return 0;
}
