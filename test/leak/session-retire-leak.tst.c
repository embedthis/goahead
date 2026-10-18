/* A session held by several requests is freed once, by its last holder */

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
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "../regress/heap-fixture.h"

#if HEAP_MEASURABLE

#define CYCLES      1000
#define RUNS        2

// Repeat enough cycles for a retained session to exceed the growth allowance.
#define MAX_GROWTH  (48 * 1024)

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

typedef struct Holders {
    Webs *a;
    Webs *b;
    int  order;
} Holders;

static void share(Holders *h, WebsSession *sp)
{
    h->a->session = sp;
    h->b->session = sp;
}



static void campaign(void *arg)
{
    Holders     *h;
    WebsSession *sp;
    int         i;

    h = arg;
    for (i = 0; i < CYCLES; i++) {
        sp = websAllocSession(h->a, NULL, 60);
        share(h, sp);
        if (h->order == 0) {
            websDestroySession(h->b);
            websDestroySession(h->a);
        } else if (h->order == 1) {
            websDestroySession(h->a);
            websDestroySession(h->b);
        } else {
            websDestroySession(h->b);
            releaseSession(h->a);
        }
        releaseSession(h->a);
        releaseSession(h->b);
    }
}


int main(int argc, char **argv)
{
    Holders h;
    ssize   growth;
    int     base, wid;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    if (websOpen("../web", NULL) < 0) {
        tfail("cannot open the server");
        return 1;
    }
    wid = websAlloc(-1);
    h.a = webs[wid];
    wid = websAlloc(-1);
    h.b = webs[wid];
    base = websGetSessionCount();

    for (h.order = 0; h.order < 3; h.order++) {
        growth = heapGrowth(campaign, &h, RUNS);
        teq(websGetSessionCount(), base, "order %d: the session count must return to its base", h.order);
        ttrue(growth < MAX_GROWTH, "order %d: %d shared sessions grew the heap by %zd bytes", h.order, CYCLES,
            growth);
    }
    websFree(h.a);
    websFree(h.b);
    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("The heap in use cannot be measured on this platform");
    return 0;
}
#endif
