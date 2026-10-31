/*
    Multipart upload routes use a separate body limit; other bodies remain under the POST memory limit.
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

#define POST_LIMIT   1000
#define UPLOAD_LIMIT 100000

static Webs *request(int flags)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("POST");
    wp->flags |= flags;
    wp->finalized = 1;
    return wp;
}

static bool chunk(Webs *wp, ssize size)
{
    char header[32];

    fmt(header, sizeof(header), "\r\n%zx\r\n", size);
    wp->rxChunkState = WEBS_CHUNK_START;
    wp->rxBytes = 0;
    wp->error = 0;
    bufFlush(&wp->rxbuf);
    bufPutStr(&wp->rxbuf, header);
    bufAddNull(&wp->rxbuf);
    filterChunkData(wp);
    return wp->error != 0;
}

int main(int argc, char **argv)
{
    Webs *form, *upload;

    websSetPostLimit(POST_LIMIT);
    websSetUploadLimit(UPLOAD_LIMIT);

    form = request(WEBS_FORM);
    teq(bodyLimit(form), POST_LIMIT, "a form body is bounded by the POST limit");
    ttrue(!chunk(form, POST_LIMIT), "a form body at the POST limit is accepted");
    ttrue(chunk(form, POST_LIMIT + 1), "a form body past the POST limit is refused");
    websFree(form);

#if ME_GOAHEAD_UPLOAD
    upload = request(WEBS_UPLOAD);
    teq(bodyLimit(upload), UPLOAD_LIMIT, "an accepted upload is bounded by the upload limit");
    ttrue(!chunk(upload, UPLOAD_LIMIT / 2), "an upload past the POST limit and within the upload limit is accepted");
    ttrue(chunk(upload, UPLOAD_LIMIT + 1), "an upload past the upload limit is refused");
    websFree(upload);
#else
    upload = 0;
#endif

    websSetUploadLimit(0);
    teq(uploadLimit, (ssize) ME_GOAHEAD_LIMIT_UPLOAD_BODY, "zero restores the compile-time upload limit");
    websSetPostLimit(0);
    return 0;
}
