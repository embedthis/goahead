/*
    unchunked-body-truncation.tst.c - A body the input buffer cannot hold is refused, not truncated
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

static Webs *receive(cchar *body, ssize max)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("POST");
    wp->finalized = 1;
    bufFree(&wp->input);
    bufCreate(&wp->input, 16, max);
    wp->rxChunkState = WEBS_CHUNK_UNCHUNKED;
    wp->rxRemaining = slen(body);
    bufPutStr(&wp->rxbuf, body);
    bufAddNull(&wp->rxbuf);
    filterChunkData(wp);
    return wp;
}


int main(int argc, char **argv)
{
    Webs *wp;
    char body[65];

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    memset(body, 'b', sizeof(body) - 1);
    body[sizeof(body) - 1] = '\0';

    wp = receive(body, 256);
    ttrue(!wp->error, "a body the input buffer can hold is accepted");
    teq(bufLen(&wp->input), 64, "and received whole");
    teq(wp->rxRemaining, 0, "with nothing left to read");
    websFree(wp);

    wp = receive(body, 32);
    ttrue(wp->error, "a body the input buffer cannot hold must be refused, not truncated");
    websFree(wp);
    return 0;
}
