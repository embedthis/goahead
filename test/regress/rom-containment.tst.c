/*
    ROM containment must allow table-backed documents when the document root is absent on disk.
 */

#define ME_ROM 1

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

#if ME_UNIX_LIKE

#define DOCUMENTS "zrom-documents-not-on-disk"

/*
    Only filename and route are needed by the containment check.
 */
static Webs *romRequest(cchar *filename)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->filename = sclone(filename);
    wp->path = sclone(&filename[sizeof(DOCUMENTS) - 1]);
    wp->docfd = -1;
    return wp;
}


static void freeRequest(Webs *wp)
{
    wfree(wp->filename);
    wfree(wp->path);
    wfree(wp);
}


static void testRomDocumentAllowed(void)
{
    struct stat sbuf;
    Webs        *wp;

    ttrue(stat(DOCUMENTS, &sbuf) < 0, "the ROM documents directory must not exist on disk for this test");
    websSetDocuments(DOCUMENTS);

    wp = romRequest(DOCUMENTS "/index.html");
    ttrue(websPageContained(wp), "R1 a ROM document must be inside its root when the documents directory is "
          "not on disk");
    ttrue(websPageAllowed(wp), "R2 a ROM document must be allowed when the documents directory is not on disk");
    freeRequest(wp);

    wp = romRequest(DOCUMENTS "/");
    ttrue(websPageAllowed(wp), "R3 the ROM documents directory itself must be allowed");
    freeRequest(wp);

    /*
        The hidden-file policy still applies to the request path on a ROM build.
     */
    wp = romRequest(DOCUMENTS "/.env");
    ttrue(!websPageAllowed(wp), "R4 a hidden ROM document must still be refused");
    freeRequest(wp);
}


int main(int argc, char **argv)
{
    testRomDocumentAllowed();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("containment is checked on Unix-like systems only");
    return 0;
}
#endif
