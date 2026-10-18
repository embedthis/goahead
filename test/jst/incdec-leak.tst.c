/* A failed ++ or -- in a template frees the variable name */
#include "testme.h"
#include "goahead.h"
#include "js.h"

#include "../regress/heap-fixture.h"

#if ME_GOAHEAD_JAVASCRIPT && HEAP_MEASURABLE

#define WARMUP      200
#define ITERATIONS  5000
#define RUNS        2

/*
    Allow allocator growth while detecting the larger per-operation leak.
 */
#define MAX_GROWTH  (160 * 1024)

static void run(int jid, int count)
{
    cchar *scripts[] = {
        "missingVariableName++;",
        "var hitsCounter=\"abc\"; hitsCounter++;",
        "var downCounter=\"2147483647\"; downCounter++;",
        "var lowCounter=\"xyz\"; lowCounter--;",
        NULL
    };
    char *result, *error;
    int  i, j;

    for (i = 0; i < count; i++) {
        for (j = 0; scripts[j]; j++) {
            error = NULL;
            result = jsEval(jid, scripts[j], &error);
            wfree(result);
            wfree(error);
        }
    }
}

static void campaign(void *arg)
{
    run(*(int*) arg, ITERATIONS);
}

int main(int argc, char **argv)
{
    char  *error;
    ssize growth;
    int   jid;

    jid = jsOpenEngine(-1, -1);
    error = NULL;
    tnull(jsEval(jid, "missingVariableName++;", &error), "an increment of an undefined variable fails");
    tnotnull(error, "and reports an error");
    wfree(error);

    run(jid, WARMUP);
    growth = heapGrowth(campaign, &jid, RUNS);
    ttrue(growth < MAX_GROWTH, "failed increments must not grow the heap: grew at least %zd bytes per %d", growth,
          ITERATIONS * 4);
    jsCloseEngine(jid);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("The heap in use cannot be measured here");
    return 0;
}
#endif
