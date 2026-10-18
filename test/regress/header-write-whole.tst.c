/* websWriteHeader writes a whole header line or nothing */

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

static char output[4096];

static Webs *openWebs(void)
{
    int wid;

    wid = websAlloc(-1);
    return webs[wid];
}

static cchar *closeWebs(Webs *wp)
{
    bufAddNull(&wp->output);
    scopy(output, sizeof(output), bufStart(&wp->output));
    websFree(wp);
    return output;
}

#if ME_GOAHEAD_LOGGING
static void quiet(int level, cchar *msg)
{
}
#endif

int main(int argc, char **argv)
{
    Webs  *wp;
    cchar *out;
    int   rc;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quiet);
#endif

    wp = openWebs();
    rc = websWriteHeader(wp, "X-Reflect", "a%cb", 1);
    websWriteHeader(wp, "X-Next", "ok");
    ttrue(rc < 0, "a value holding 0x01 must be refused");
    out = closeWebs(wp);
    tmatch(out, "X-Next: ok\r\n", "nothing of a refused value may be written");

    wp = openWebs();
    rc = websWriteHeader(wp, "bad key", "value");
    websWriteHeader(wp, "X-Next", "ok");
    ttrue(rc < 0, "a name that is not a token must be refused");
    out = closeWebs(wp);
    tmatch(out, "X-Next: ok\r\n", "nothing of a refused name may be written");

    wp = openWebs();
    rc = websWriteHeader(wp, "X-Empty", NULL);
    websWriteHeader(wp, "X-Next", "ok");
    ttrue(rc == 0, "a header with no value is written");
    out = closeWebs(wp);
    tmatch(out, "X-Empty: \r\nX-Next: ok\r\n", "a header with no value must be a whole line");

    wp = openWebs();
    rc = websWriteHeader(wp, "Content-Type", "text/html%c", 11);
    ttrue(rc < 0, "a Content-Type holding 0x0B must be refused");
    ttrue(!(wp->flags & WEBS_TYPE_WRITTEN), "a refused Content-Type must not count as written");
    closeWebs(wp);

    wp = openWebs();
    websWriteHeader(wp, "Content-Type", "text/plain");
    ttrue(wp->flags & WEBS_TYPE_WRITTEN, "a written Content-Type must be recorded");
    out = closeWebs(wp);
    tmatch(out, "Content-Type: text/plain\r\n", "a valid header is written whole");

    ttrue(websValidHeaderValue("a\tb"), "a tab is permitted in a value");
    ttrue(!websValidHeaderValue("a\x7f"), "DEL is refused in a value");
    ttrue(!websValidHeaderName("X%41"), "a percent sign is refused in a name");
    return 0;
}
