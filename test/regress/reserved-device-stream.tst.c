/*
    reserved-device-stream.tst.c - A reserved device name followed by a stream name is still a device
 */

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
        "web/CON", "web/con.txt", "web/COM1", "web/sub/aux",
        "web/CON::$DATA", "web/con:x", "web/prn:", "web/nul.txt:s", "web/sub/aux::$DATA", "web\\lpt1:s",
        "web/con :x",
        0
    };
    cchar *files[] = {
        "web/index.html", "web/console:x", "web/icon:x", "web/com10:x", "C:/web/index.html", "C:\\web\\a.txt",
        0
    };
    int i;

    for (i = 0; devices[i]; i++) {
        ttrue(websIsReservedDevice(devices[i]), "%s names a reserved device", devices[i]);
    }
    for (i = 0; files[i]; i++) {
        ttrue(!websIsReservedDevice(files[i]), "%s does not name a reserved device", files[i]);
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
