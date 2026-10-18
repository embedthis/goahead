/*
    buf-flat.tst.c - A wrapped buffer is read as a flat string only after it is made contiguous
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
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    Wrap a fixed-size ring by consuming filler before adding data.
 */
static void wrap(WebsBuf *bp, ssize size, cchar *data)
{
    char filler[64];
    ssize skip;

    bufFree(bp);
    bufCreate(bp, size, size);
    //  The allocation may be rounded up, so measure from its actual length
    skip = bp->buflen - slen(data) / 2;
    memset(filler, 'x', sizeof(filler));
    bufPutBlk(bp, filler, skip);
    bufAdjustStart(bp, skip);
    bufPutBlk(bp, data, slen(data));
}


static void suiteA_bufStart(void)
{
    WebsBuf buf;
    cchar   *data = "0123456789ABCDEF";
    char    *start;

    tinfo("\n--- Suite A: bufStart makes wrapped data contiguous ---\n");

    memset(&buf, 0, sizeof(buf));
    wrap(&buf, 24, data);
    ttrue(buf.servp > buf.endp, "the fixture wraps the data round the end of the ring");
    teq(bufLen(&buf), slen(data), "the ring holds the data");

    start = bufStart(&buf);
    ttrue(smatch(start, data), "bufStart must return the data whole and in order, got [%s]", start);
    ttrue(start == buf.buf && buf.endp == buf.buf + slen(data), "the data is moved to the front");
    teq(bufLen(&buf), slen(data), "no data is lost");

    bufFlush(&buf);
    bufPutStr(&buf, "flat");
    ttrue(smatch(bufStart(&buf), "flat"), "a flat buffer is returned as is");
    bufFree(&buf);
}


static void suiteB_chunkFilter(void)
{
    Webs  *wp;
    cchar *body = "\r\nA\r\n0123456789\r\n0\r\n\r\n";
    int   wid;

    tinfo("\n--- Suite B: the chunk filter reads a wrapped receive buffer ---\n");

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("POST");
    wp->finalized = 1;
    wp->rxChunkState = WEBS_CHUNK_START;
    wrap(&wp->rxbuf, 32, body);
    ttrue(wp->rxbuf.servp > wp->rxbuf.endp, "the fixture wraps the chunked body round the end of the ring");

    filterChunkData(wp);
    ttrue(!wp->error, "a wrapped chunked body must parse");
    ttrue(smatch(bufStart(&wp->input), "0123456789"), "the chunk data must arrive whole, got [%s]",
          bufStart(&wp->input));
    ttrue(wp->eof, "the last chunk must end the body");
    websFree(wp);
}


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    suiteA_bufStart();
    suiteB_chunkFilter();
    return 0;
}
