/*
    Release transfer buffers and close truncated responses without appending an error body.
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
static int live, errors, readResult;
static void *trackAlloc(ssize n) { void *p = walloc(n); if (p) live++; return p; }
static void trackFree(void *p) { if (p) live--; wfree(p); }
static ssize failRead(Webs *wp, char *buf, ssize n) { return readResult; }
static void reportError(Webs *wp, int code, cchar *format, ...) { errors++; }
#define walloc trackAlloc
#define wfree trackFree
#define websPageReadData failRead
#define websError reportError
#define websValid(wp) 1
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/file.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef walloc
#undef wfree
#undef websPageReadData
#undef websError
#undef websValid
int main(int argc, char **argv)
{
    Webs wp;
    for (readResult = 0; readResult >= -1; readResult--) {
        memset(&wp, 0, sizeof(wp));
        wp.txRemaining = 10;
        wp.flags = WEBS_KEEP_ALIVE;
        live = errors = 0;
        fileWriteEvent(&wp);
        teq(errors, 0, "no error page is written into a committed body");
        ttrue(!(wp.flags & WEBS_KEEP_ALIVE), "read failure closes the connection");
        teq(live, 0, "transfer buffer is released");
    }
    return 0;
}
