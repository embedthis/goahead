/*
    upload-limit-overflow.tst.c - The per-file upload limit is tested against the room left, never by a sum

    writeToFile (src/upload.c) guards ME_GOAHEAD_LIMIT_UPLOAD with "file->size + len > limit". The sum can overflow
    ssize, after which the comparison passes and the write proceeds.
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

#if ME_GOAHEAD_UPLOAD
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"

/*
    Capture the status writeToFile reports instead of writing a response.
 */
static int lastCode = 0;

static void recordError(Webs *wp, int code, cchar *fmt, ...)
{
    lastCode = code & WEBS_CODE_MASK;
}
#define websError recordError
#include "../../src/upload.c"
#undef websError
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int limitWrite(Webs *wp, WebsUpload *file, ssize size, ssize len)
{
    char data[1];

    memset(file, 0, sizeof(*file));
    file->size = size;
    wp->currentFile = file;
    lastCode = 0;
    return writeToFile(wp, data, len);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_UPLOAD
    Webs       *wp;
    WebsUpload file;
    int        wid;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    wid = websAlloc(-1);
    ttrue(wid >= 0, "allocate request");
    wp = webs[wid];
    wp->finalized = 1;
    wp->upfd = -1;

    tinfo("\n--- A write that would pass the per-file limit is refused with 413 ---\n");
    ttrue(limitWrite(wp, &file, ME_GOAHEAD_LIMIT_UPLOAD, 1) < 0, "one byte past the limit is refused");
    teq(lastCode, HTTP_CODE_REQUEST_TOO_LARGE, "and reported as too large");
    teq(file.size, ME_GOAHEAD_LIMIT_UPLOAD, "the file size is unchanged");

    tinfo("\n--- The limit holds when size plus length does not fit in ssize ---\n");
    ttrue(limitWrite(wp, &file, ME_GOAHEAD_LIMIT_UPLOAD, MAXSSIZE - ME_GOAHEAD_LIMIT_UPLOAD + 1) < 0,
          "a length that overflows the sum is refused");
    teq(lastCode, HTTP_CODE_REQUEST_TOO_LARGE, "as too large, not as a failed write");
    teq(file.size, ME_GOAHEAD_LIMIT_UPLOAD, "the file size is unchanged");

    ttrue(limitWrite(wp, &file, 1, MAXSSIZE) < 0, "a length of MAXSSIZE is refused");
    teq(lastCode, HTTP_CODE_REQUEST_TOO_LARGE, "as too large, not as a failed write");

    websFree(wp);
#else
    tskip("upload support is not compiled in");
#endif
    return 0;
}
