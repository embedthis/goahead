/*
    action-legacy-name-leak.tst.c - The action handler frees the action name it clones, for both action signatures.

    getActionName returns an allocated copy of the second path segment. actionHandler frees it after a native action
    returns, but the ME_GOAHEAD_LEGACY branch returns before the free, so every request for a legacy-signature action
    leaks the name. Count the clones and frees around the handler for each signature; they must balance. The legacy
    configuration is compiled here, so the test does not need a separate server build.
 */
#define ME_GOAHEAD_LEGACY 1
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

static int clones, frees, calls;

static char *countClone(cchar *str, ssize len)
{
    clones++;
    return snclone(str, len);
}


static void countFree(void *blk)
{
    if (blk) {
        frees++;
    }
    wfree(blk);
}

#define snclone countClone
#define wfree countFree
#include "../../src/action.c"
#undef snclone
#undef wfree


static void legacyProc(Webs *wp, char *path, char *query)
{
    calls++;
}


static void nativeAction(Webs *wp)
{
    calls++;
}


static void run(cchar *path)
{
    Webs      wp;
    WebsRoute route;

    memset(&wp, 0, sizeof(wp));
    memset(&route, 0, sizeof(route));
    //  A route with an explicit method set, so the handler does not demand POST
    route.methods = 0;
    wp.route = &route;
    wp.method = "GET";
    wp.path = (char*) path;
    wp.query = "a=1";
    clones = frees = 0;
    actionHandler(&wp);
}


int main(int argc, char **argv)
{
    actionTable = hashCreate(WEBS_HASH_INIT);
    websDefineNativeAction("native", nativeAction);
    //  In a legacy build websDefineAction registers the WebsProc signature
    websDefineAction("legacy", (void*) legacyProc);

    run("/action/native");
    teq(calls, 1, "the native action ran");
    teq(clones, 1, "the native path clones the action name once");
    teq(frees, clones, "the native path frees the action name");

    run("/action/legacy");
    teq(calls, 2, "the legacy action ran");
    teq(clones, 1, "the legacy path clones the action name once");
    teq(frees, clones, "the legacy path must free the action name");

    hashFree(actionTable);
    return 0;
}
