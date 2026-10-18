/* In a legacy build each action is called with the signature it was defined with */

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
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#include "../../src/action.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int  legacyCalls;
static int  nativeCalls;
static char legacyPath[64];
static char legacyQuery[64];

static void legacyAction(Webs *wp, char *path, char *query)
{
    legacyCalls++;
    scopy(legacyPath, sizeof(legacyPath), path);
    scopy(legacyQuery, sizeof(legacyQuery), query);
}

static void nativeAction(Webs *wp)
{
    nativeCalls++;
}

static void request(Webs *wp, cchar *path)
{
    wfreeset(wp->path);
    wp->path = sclone(path);
    actionHandler(wp);
}

int main(int argc, char **argv)
{
    Webs *wp;
    int  wid;

    actionTable = hashCreate(WEBS_HASH_INIT);
    ttrue(websDefineAction("legacy", legacyAction) == 0, "a legacy action is defined");
    ttrue(websDefineNativeAction("native", nativeAction) == 0, "a native action is defined");

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("POST");
    wp->query = sclone("role=user");

    request(wp, "/action/legacy");
    ttrue(legacyCalls == 1, "the legacy action runs");
    tmatch(legacyPath, "legacy", "the legacy action receives the action name");
    tmatch(legacyQuery, "role=user", "the legacy action receives the query");

    request(wp, "/action/native");
    ttrue(nativeCalls == 1, "the native action runs");
    ttrue(legacyCalls == 1, "the native action is not dispatched as a legacy action");

    websFree(wp);
    closeAction();
    return 0;
}
