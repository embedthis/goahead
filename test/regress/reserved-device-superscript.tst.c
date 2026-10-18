/* COM and LPT with a superscript digit are reserved devices */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_WIN_LIKE && !ME_ROM
#include "../../src/fs.c"

int main(int argc, char **argv)
{
    cchar *devices[] = {
        "web/COM\xc2\xb9", "web/com\xc2\xb2.txt", "web/Lpt\xc2\xb3:x", "web/sub/LPT\xc2\xb9 .log",
        "web/COM\xb9", "web/lpt\xb2.txt", "web/com\xb3::$DATA",
        0
    };
    cchar *files[] = {
        "web/com\xc2\xb9x", "web/com\xc3\xa9.txt", "web/lpt\xc2\xb4", "web/com\xb4", "web/compact\xc2\xb9",
        0
    };
    int i;

    for (i = 0; devices[i]; i++) {
        ttrue(websIsReservedDevice(devices[i]), "device %d names a reserved device", i);
    }
    for (i = 0; files[i]; i++) {
        ttrue(!websIsReservedDevice(files[i]), "file %d does not name a reserved device", i);
    }
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Windows reserved device names apply only to Windows builds");
    return 0;
}
#endif
