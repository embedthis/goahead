/*
    document-root.tst.c - An empty or missing document root refuses to start
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int loadRoutes(cchar *contents)
{
    cchar *path = "document-root.tmp";
    FILE  *fp;
    int   rc;

    if ((fp = fopen(path, "w")) == 0) {
        tfail("cannot write %s", path);
        return 0;
    }
    fputs(contents, fp);
    fclose(fp);
    rc = websLoad(path);
    unlink(path);
    return rc;
}


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif

    /*
        Rejected startup allocates no server resources and needs no websClose.
     */
    ttrue(websOpen("", NULL) < 0, "an empty document root must refuse to start");

    ttrue(websOpen("no-such-directory-for-this-test", NULL) < 0, "a missing document root must refuse to start");
    ttrue(websOpen("document-root.tst.c", NULL) < 0, "a file is not a document root");

    /*
        Keep document root separate from the working directory used for PUT staging.
     */
    ttrue(websOpen("../web", NULL) == 0, "a directory is a valid document root");
    tmatch(websGetDocuments(), "../web", "the document root is the one supplied");
    websSetDocuments("");
    tmatch(websGetDocuments(), "../web", "an empty root must not replace a valid one");

    ttrue(loadRoutes("route uri=/empty/ dir=\n") < 0, "an empty route dir= must refuse the configuration");
    ttrue(loadRoutes("route uri=/named/ dir=.\n") == 0, "a route with a named dir= must still load");
    websClose();

    return 0;
}
