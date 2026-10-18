/*
    body-limit-overflow.tst.c - The request body limits cannot be defeated by overflow
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

static Webs *request(cchar *method)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone(method);
    wp->finalized = 1;
    return wp;
}


static bool chunk(Webs *wp, cchar *header)
{
    wp->rxChunkState = WEBS_CHUNK_START;
    wp->error = 0;
    bufFlush(&wp->rxbuf);
    bufPutStr(&wp->rxbuf, header);
    bufAddNull(&wp->rxbuf);
    filterChunkData(wp);
    return wp->error;
}


static void suiteA_chunkSize(void)
{
    Webs *wp;

    tinfo("\n--- Suite A: a chunk size past the limit is refused, however large the limit ---\n");

    wp = request("POST");
    websSetPostLimit(0);
    ttrue(!chunk(wp, "\r\n10\r\n"), "an ordinary chunk is accepted");
    teq(wp->rxRemaining, 16, "and read at its size");

    websSetPostLimit(MAXSSIZE);
    wp->rxBytes = 0;
    ttrue(!chunk(wp, "\r\n7fffffffffffffff\r\n"), "a chunk of exactly the limit is accepted");
    teq(wp->rxRemaining, MAXSSIZE, "and read at its size");

    wp->rxBytes = 0;
    ttrue(chunk(wp, "\r\n8000000000000000\r\n"), "a chunk one past the limit is refused");

    wp->rxBytes = 0;
    ttrue(chunk(wp, "\r\n80000000000000010\r\n"), "a chunk size that overflows must be refused, not wrapped");
    ttrue(wp->rxRemaining != 16, "the wrapped size must not frame the body");

    wp->rxBytes = 0;
    ttrue(chunk(wp, "\r\n1000000000000000000000000000010\r\n"), "any number of digits must be refused");
    websFree(wp);
}


static void suiteB_chunkTotal(void)
{
    Webs *wp;

    tinfo("\n--- Suite B: the chunked body total is refused past the limit, however large the limit ---\n");

    wp = request("POST");
    websSetPostLimit(0);
    wp->rxBytes = ME_GOAHEAD_LIMIT_POST - 16;
    ttrue(!chunk(wp, "\r\n10\r\n"), "a chunk that reaches the limit exactly is accepted");
    teq(wp->rxBytes, ME_GOAHEAD_LIMIT_POST, "and counted");
    ttrue(chunk(wp, "\r\n1\r\n"), "one byte more is refused");

    websSetPostLimit(MAXSSIZE);
    wp->rxBytes = MAXSSIZE - 10;
    ttrue(chunk(wp, "\r\n64\r\n"), "a total that overflows must be refused, not wrapped");
    teq(wp->rxBytes, MAXSSIZE - 10, "a refused chunk is not counted");
    websSetPostLimit(0);
    websFree(wp);
}


static void suiteC_putTotal(void)
{
    Webs *wp;
    char path[64];

    tinfo("\n--- Suite C: the PUT total is refused past the limit ---\n");

    fmt(path, sizeof(path), "body-limit-%d.tmp", (int) getpid());
    wp = request("PUT");
    wp->putfd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);
    ttrue(wp->putfd >= 0, "open a PUT target");

    bufPutStr(&wp->input, "0123456789");
    websProcessPutData(wp);
    ttrue(!wp->error, "PUT data within the limit is written");
    teq(wp->putLen, 10, "and counted");

    wp->putLen = ME_GOAHEAD_LIMIT_PUT - 5;
    bufPutStr(&wp->input, "0123456789");
    websProcessPutData(wp);
    ttrue(wp->error, "PUT data past the limit is refused");

    // Set the total near the 32-bit limit.
    wp->error = 0;
    wp->finalized = 1;
    if (wp->putfd < 0) {
        wp->putfd = open(path, O_WRONLY | O_BINARY, 0600);
    }
    wp->putLen = MAXSSIZE - 5;
    bufPutStr(&wp->input, "0123456789");
    websProcessPutData(wp);
    ttrue(wp->error, "a PUT total that overflows must be refused, not wrapped");

    if (wp->putfd >= 0) {
        close(wp->putfd);
        wp->putfd = -1;
    }
    unlink(path);
    websFree(wp);
}


static Webs *post(cchar *length)
{
    Webs *wp;
    char *text;

    wp = request("POST");
    wp->flags |= WEBS_KEEP_ALIVE;
    text = sfmt("Content-Length: %s\r\n\r\n", length);
    bufPutStr(&wp->rxbuf, text);
    bufAddNull(&wp->rxbuf);
    wfree(text);
    parseHeaders(wp);
    return wp;
}


static void suiteD_contentLength(void)
{
    Webs *wp;

    tinfo("\n--- Suite D: a Content-Length past the limit is refused, however large the limit ---\n");

    websSetPostLimit(MAXSSIZE);
    wp = post("18446744073709551616");
    ttrue(wp->error, "a length of 2^64 must be refused, not wrapped to zero");
    ttrue(!(wp->flags & WEBS_KEEP_ALIVE), "the connection is closed");
    ttrue(wp->rxLen != 0, "the wrapped length must not frame an empty body");
    websFree(wp);

    wp = post("18446744073709551620");
    ttrue(wp->error, "a length just past 2^64 must be refused");
    ttrue(wp->rxLen != 4, "the wrapped length must not frame a short body");
    websFree(wp);

    wp = post("1000000000000000000000000000000000000000");
    ttrue(wp->error, "a 40 digit length must be refused");
    ttrue(!(wp->flags & WEBS_KEEP_ALIVE), "and the connection is closed");
    websFree(wp);

    wp = post("9223372036854775808");
    ttrue(wp->error, "a length one past the limit is refused");
    websFree(wp);

    wp = post("9223372036854775807");
    ttrue(!wp->error, "a length of exactly the limit is accepted");
    teq(wp->rxLen, MAXSSIZE, "and read at its size");
    websFree(wp);

    websSetPostLimit(0);
    wp = post("0");
    ttrue(!wp->error, "a zero length is accepted");
    teq(wp->rxLen, 0, "as an empty body");
    websFree(wp);
}


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    suiteA_chunkSize();
    suiteB_chunkTotal();
    suiteC_putTotal();
    suiteD_contentLength();
    return 0;
}
