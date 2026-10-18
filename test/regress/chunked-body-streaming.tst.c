/* Decoded chunk data is handed to the body consumers as it arrives */

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

static void put(Webs *wp, cchar *data, ssize len)
{
    bufPutBlk(&wp->rxbuf, data, len);
    bufAddNull(&wp->rxbuf);
}

int main(int argc, char **argv)
{
    Webs *wp;
    char block[1024];
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("PUT");
    wp->rxChunkState = WEBS_CHUNK_START;

    //  Two whole chunks and no last chunk: the data is reported at once
    memset(block, 'a', sizeof(block));
    put(wp, "\r\n400\r\n", 7);
    put(wp, block, sizeof(block));
    put(wp, "\r\n400\r\n", 7);
    put(wp, block, sizeof(block));
    ttrue(filterChunkData(wp), "decoded chunk data must be reported before the last chunk");
    teqz(bufLen(&wp->input), 2048, "both chunks must be decoded in one call");
    ttrue(!wp->eof, "the body is not complete");


    bufFlush(&wp->input);
    ttrue(!filterChunkData(wp), "no new data must report nothing");

    //  Part of a chunk: the part is reported, the rest follows
    put(wp, "\r\n10\r\n12345678", 14);
    ttrue(filterChunkData(wp), "part of a chunk must be reported");
    teqz(bufLen(&wp->input), 8, "the part is decoded");
    put(wp, "9abcdef0", 8);
    ttrue(filterChunkData(wp), "the rest of the chunk must be reported");
    teqz(bufLen(&wp->input), 16, "the whole chunk is decoded");

    //  An incomplete chunk header with no data reports nothing
    bufFlush(&wp->input);
    put(wp, "\r\n4", 3);
    ttrue(!filterChunkData(wp), "an incomplete chunk header must report nothing");

    //  The last chunk completes the body
    bufFlush(&wp->rxbuf);
    put(wp, "\r\n0\r\n\r\n", 7);
    ttrue(filterChunkData(wp), "the last chunk must be reported");
    ttrue(wp->eof, "the body is complete");
    websFree(wp);
    return 0;
}
