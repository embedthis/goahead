/* In a legacy build a chunked request body does not replace the URL query */

#define ME_GOAHEAD_LEGACY 1

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

int main(int argc, char **argv)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("POST");
    wp->query = sclone("role=user");
    wp->rxChunkState = WEBS_CHUNK_START;
    bufPutStr(&wp->rxbuf, "\r\n9\r\nrole=root\r\n0\r\n\r\n");
    bufAddNull(&wp->rxbuf);
    ttrue(filterChunkData(wp), "the chunked body is decoded");
    ttrue(wp->eof, "the body is complete");
    tmatch(wp->query, "role=user", "the URL query must not be replaced by the body");
    websFree(wp);
    return 0;
}
