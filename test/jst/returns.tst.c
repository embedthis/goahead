/* Returning from a for body must release its saved lexer state. */

#include "testme.h"
#include "goahead.h"
#include "js.h"

#include "../regress/heap-fixture.h"

#if ME_GOAHEAD_JAVASCRIPT
#define WARMUP      100
#define EVALUATIONS 10000
#define RUNS        2
#define ALLOWANCE   (64 * 1024)

typedef struct Script {
    cchar *script;
    cchar *expected;
    bool  valid;
} Script;

static bool evaluate(cchar *script, cchar *expected)
{
    char *result, *error;
    int  jid;
    bool valid;

    error = NULL;
    jid = jsOpenEngine(-1, -1);
    result = jsEval(jid, script, &error);
    valid = expected ? result && !error && smatch(result, expected) : !result && error;
    wfree(error);
    jsCloseEngine(jid);
    return valid;
}


#if HEAP_MEASURABLE
static void campaign(void *arg)
{
    Script *sp;
    int    i;

    sp = arg;
    for (i = 0; i < EVALUATIONS; i++) {
        sp->valid = evaluate(sp->script, sp->expected) && sp->valid;
    }
}
#endif


static void checkScript(cchar *name, cchar *script, cchar *expected)
{
    Script s;
    ssize  growth;
    int    i;

    s.script = script;
    s.expected = expected;
    s.valid = evaluate(script, expected);
    ttrue(s.valid, "%s: return value or error is preserved", name);
    if (!s.valid) {
        return;
    }
    if (!HEAP_MEASURABLE || ME_GOAHEAD_REPLACE_MALLOC || smatch(tget("ME_SANITIZE", ""), "1") ||
        getenv("ASAN_OPTIONS") || getenv("UBSAN_OPTIONS")) {
        tinfo("%s: allocator growth check unavailable; result checked\n", name);
        return;
    }
#if HEAP_MEASURABLE
    for (i = 0; i < WARMUP; i++) {
        s.valid = evaluate(script, expected) && s.valid;
    }
    growth = heapGrowth(campaign, &s, RUNS);
    ttrue(s.valid, "%s: every evaluation has the expected result", name);
    ttrue(growth < ALLOWANCE, "%s: allocated bytes grew by at least %lld over %d evaluations (limit %d)",
          name, (long long) growth, EVALUATIONS, ALLOWANCE);
#endif
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    checkScript("direct return", "for (i=0;i<3;i++) { return i; }", "0");
    checkScript("nested block", "for (i=0;i<3;i++) { { return i; } }", "0");
    checkScript("then return", "for (i=0;i<3;i++) { if (i==1) { return i; } }", "1");
    checkScript("else return", "for (i=0;i<3;i++) { if (i==0) { x=1; } else { return i; } }", "1");
    checkScript("nested loops", "for (i=0;i<3;i++) { for (j=0;j<3;j++) { return j; } }", "0");
    checkScript("skipped loop", "for (i=0;i<0;i++) { return i; } return 7;", "7");
    checkScript("normal loop", "for (i=0;i<3;i++) { x=i; } return 9;", "9");
    checkScript("body error", "for (i=0;i<3;i++) { missing; }", NULL);
#endif
    return 0;
}
