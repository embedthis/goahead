/* websPageAllowed refuses a Windows reserved device name */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

static bool allowed(cchar *path)
{
    Webs  wp;
    char  *filename;
    bool  rc;

    memset(&wp, 0, sizeof(wp));
    filename = sfmt("../web%s", path);
    wp.path = (char*) path;
    wp.filename = filename;
    rc = websPageAllowed(&wp);
    wfree(filename);
    return rc;
}


int main(int argc, char **argv)
{
    cchar *devices[] = { "/COM1", "/nul.txt", "/dir/aux", "/tmp/CON", "/tmp/lpt1.html", 0 };
    int   i;

    //  Containment needs a documents root to resolve
    websSetDocuments("../web");
    ttrue(allowed("/index.html"), "a regular file must be allowed");
    for (i = 0; devices[i]; i++) {
#if ME_WIN_LIKE && !ME_ROM
        ttrue(!allowed(devices[i]), "%s names a reserved device and must be refused", devices[i]);
#else
        ttrue(allowed(devices[i]), "%s is an ordinary file off Windows and must be allowed", devices[i]);
#endif
    }
    return 0;
}
