/*
    cgi-length-overflow.tst.c - The CGI Content-Length accounting cannot be defeated by overflow
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

#if ME_GOAHEAD_CGI
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/cgi.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define OUTPUT 100

static char path[64];

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    Return the number of CGI body bytes forwarded.
 */
static ssize gather(Cgi *cgip, ssize txLen, ssize txBytes, ssize txExcess)
{
    Webs  *wp;
    ssize forwarded;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    bufCreate(&wp->output, ME_GOAHEAD_LIMIT_BUFFER + 1, ME_GOAHEAD_LIMIT_BUFFER + 1);
    wp->flags = WEBS_HEADERS_CREATED | WEBS_KEEP_ALIVE;

    memset(cgip, 0, sizeof(Cgi));
    cgip->wp = wp;
    cgip->stdOut = path;
    cgip->txLen = txLen;
    cgip->txBytes = txBytes;
    cgip->txExcess = txExcess;

    websCgiGatherOutput(cgip);
    forwarded = bufLen(&wp->output);
    bufFree(&wp->output);
    wfree(wp);
    return forwarded;
}


static void suiteA_ordinary(void)
{
    Cgi cgi;

    tinfo("\n--- Suite A: the body is forwarded up to the declared length ---\n");

    teq(gather(&cgi, OUTPUT, 0, 0), OUTPUT, "a body of the declared length is forwarded");
    teq(cgi.txBytes, OUTPUT, "and counted");
    teq(cgi.txExcess, 0, "with nothing dropped");

    teq(gather(&cgi, 50, 0, 0), 50, "a body past the declared length is cut at it");
    teq(cgi.txBytes, 50, "the count stops at the declared length");
    teq(cgi.txExcess, OUTPUT - 50, "the surplus is tallied");

    teq(gather(&cgi, -1, 0, 0), OUTPUT, "with no declared length the whole body is forwarded");
}


static void suiteB_overflow(void)
{
    Cgi cgi;

    tinfo("\n--- Suite B: the accounting holds at the top of its range ---\n");

    teq(gather(&cgi, MAXSSIZE, MAXSSIZE - 10, 0), 10, "a surplus must be cut when the count is near its maximum");
    teq(cgi.txBytes, MAXSSIZE, "the count stops at the declared length");
    teq(cgi.txExcess, OUTPUT - 10, "the surplus is tallied");

    gather(&cgi, 0, 0, MAXSSIZE - 5);
    teq(cgi.txExcess, MAXSSIZE, "the surplus tally saturates rather than wrapping");

    gather(&cgi, -1, MAXSSIZE - 5, 0);
    teq(cgi.txBytes, MAXSSIZE - 5, "with no declared length nothing is counted");
}


int main(int argc, char **argv)
{
    char body[OUTPUT];
    int  fd;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    fmt(path, sizeof(path), "cgi-length-%d.tmp", (int) getpid());
    memset(body, 'x', sizeof(body));
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);
    ttrue(fd >= 0, "create the CGI output file");
    ttrue(write(fd, body, sizeof(body)) == sizeof(body), "write the CGI output");
    close(fd);

    suiteA_ordinary();
    suiteB_overflow();

    unlink(path);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("CGI is not enabled");
    return 0;
}
#endif
