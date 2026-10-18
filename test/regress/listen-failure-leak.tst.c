/* A listen that cannot bind its port frees the parsed address */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#include "heap-fixture.h"

#if HEAP_MEASURABLE

#define TEST_URL    "http://127.0.0.1:18393"
#define WARMUP      100
#define ITERATIONS  5000
#define RUNS        2

/*
    Unfixed, a run leaks one address string per listen: about 80 KB.
 */
#define MAX_GROWTH  (32 * 1024)

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int failures;

static void repeat(int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (websListen(TEST_URL) < 0) {
            failures++;
        }
    }
}

static void campaign(void *arg)
{
    repeat(ITERATIONS);
}

int main(int argc, char **argv)
{
    ssize growth;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    ttrue(websListen(TEST_URL) >= 0, "listen on a loopback port");

    repeat(WARMUP);
    teq(failures, WARMUP, "a second listen on the same port fails");
    growth = heapGrowth(campaign, NULL, RUNS);
    teq(failures, WARMUP + ITERATIONS * RUNS, "every repeated listen fails");
    ttrue(growth < MAX_GROWTH, "failed listens must not grow the heap: grew at least %zd bytes per %d",
          growth, ITERATIONS);

    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("The heap in use cannot be measured here");
    return 0;
}
#endif
