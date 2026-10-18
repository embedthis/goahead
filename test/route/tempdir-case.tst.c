/*
    tempdir-case.tst.c - Temp-directory containment follows filesystem case rules
 */

#include "testme.h"
#include "goahead.h"

#if ME_WIN_LIKE
int main(int argc, char **argv)
{
    tinfo("tempdir-case: the Windows containment test was already case-insensitive - nothing to check\n");
    return 0;
}
#else

#if ME_GOAHEAD_LOGGING
/*
    Count errors without printing expected diagnostics.
 */
static void captureLog(int level, cchar *msg)
{
}
#endif

static int checkWithDir(cchar *dir)
{
    WebsRoute *route;
    int       problems;
#if ME_GOAHEAD_LOGGING
    WebsLogHandler priorHandler;
#endif

    ttrue(websOpenRoute() == 0, "websOpenRoute failed");
    route = websAddRoute("/staged/", "continue", -1);
    ttrue(route != 0, "Cannot add route");
    websSetRouteMatch(route, dir, 0, -1, -1, -1, -1);
#if ME_GOAHEAD_LOGGING
    priorHandler = logSetHandler(captureLog);
#endif
    problems = websCheckTempDirs();
#if ME_GOAHEAD_LOGGING
    logSetHandler(priorHandler);
#endif
    websCloseRoute();
    return problems;
}


int main(int argc, char **argv)
{
    char *variant;

    tinfo("tempdir-case: the temp-dir containment guard must fold case where the file system does\n");

    ttrue(checkWithDir(ME_GOAHEAD_TEMP_DIR) == 1, "serving the staging directory itself must be refused");

    /*
        Apply the filesystem's case rules to containment.
     */
    variant = supper(sclone(ME_GOAHEAD_TEMP_DIR));
    if (smatch(variant, ME_GOAHEAD_TEMP_DIR)) {
        tinfo("tempdir-case: %s has no case variant - skipping the variant check\n", ME_GOAHEAD_TEMP_DIR);
    } else {
#if ME_GOAHEAD_CASE_INSENSITIVE_FS
        ttrue(checkWithDir(variant) == 1, "a case variant of the staging directory must be refused on a "
              "case-insensitive file system");
#else
        ttrue(checkWithDir(variant) == 0, "a case variant is a different directory on a case-sensitive "
              "file system");
#endif
    }
    wfree(variant);

    ttrue(checkWithDir("/var/private") == 0, "a disjoint served directory must be allowed");

    return 0;
}
#endif
