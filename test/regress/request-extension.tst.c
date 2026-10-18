/* Request and URL parsers agree on the final path component extension. */
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
static int testListenPort(int sid) { return 80; }
#define socketGetPort testListenPort
#include "../../src/http.c"
#undef socketGetPort
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
int main(int argc, char **argv)
{
    cchar *paths[] = { "/dir.v2/README", "/dir.v2/file.txt", "/file.", "/dir.v2/", "/plain", "/.hidden" };
    cchar *expected[] = { NULL, ".txt", NULL, NULL, NULL, ".hidden" };
    Webs *wp;
    char *buf, *ext, *request;
    int i, wid, rc;
    socketOpen();
    for (i = 0; i < 6; i++) {
        wid = websAlloc(-1);
        ttrue(wid >= 0, "allocate request");
        wp = webs[wid];
        wp->listenSid = -1;
        request = sfmt("GET %s HTTP/1.0\r\n", paths[i]);
        bufPutStr(&wp->rxbuf, request);
        wfree(request);
        parseFirstLine(wp);
        if (expected[i]) tmatch(wp->ext, expected[i], "request extension");
        else tnull(wp->ext, "directory dots and trailing dots are not extensions");
        buf = ext = NULL;
        rc = websUrlParse(paths[i], &buf, NULL, NULL, NULL, NULL, &ext, NULL, NULL);
        teq(rc, 0, "URL parses");
        if (expected[i]) tmatch(ext, expected[i], "URL extension");
        else tnull(ext, "URL has no extension");
        wfree(buf);
        websFree(wp);
    }
    socketClose();
    return 0;
}
