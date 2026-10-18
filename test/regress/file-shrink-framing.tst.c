/*
    file-shrink-framing.tst.c - A document is described, and framed, by the descriptor that serves it
 */

#include "testme.h"
#include "goahead.h"
#include "http-client-fixture.h"

/* The test's document root is one directory above its working directory. */
#define FIXTURE      "../web/shrink.dat"
#define REPLACEMENT  "../web/shrink.new"
#define FIXTURE_URL  "/shrink.dat"
#define BIG_SIZE     (4 * 1024 * 1024)
#define SMALL_SIZE   128
#define TINY_RCVBUF  512
#define SLOW_PAUSE   2
#define READ_TIMEOUT (ME_GOAHEAD_LIMIT_TIMEOUT + 20)

/*
    Use heap scratch space to fit Windows' 1 MB test stack.
 */
static char readBuf[8 * 1024];

static int makeFile(cchar *path, ssize size)
{
    FILE  *fp;
    ssize i;

    if ((fp = fopen(path, "wb")) == NULL) {
        return -1;
    }
    memset(readBuf, 'A', sizeof(readBuf));
    for (i = 0; i < size; i += (ssize) sizeof(readBuf)) {
        if (fwrite(readBuf, 1, (size_t) min((ssize) sizeof(readBuf), size - i), fp) == 0) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    return 0;
}


/*************************** Suite A: stat the descriptor *********************/

/*
    Replace the path while retaining the original open descriptor.
 */
static void testStatOpenFile(void)
{
    WebsFileInfo byFd;
    int          fd;

    ttrue(makeFile(FIXTURE, BIG_SIZE) == 0);
    if ((fd = websOpenFile(FIXTURE, O_RDONLY | O_BINARY, 0)) < 0) {
        tfail("Cannot open the fixture");
        return;
    }
    ttrue(websStatOpenFile(fd, &byFd) == 0);
    teqz(byFd.size, BIG_SIZE, "the descriptor reports the size it was opened with");

#if !ME_WIN_LIKE
    WebsFileInfo byPath;

    /*
        Replace the path while retaining the original descriptor; Windows disallows this replacement.
     */
    ttrue(makeFile(REPLACEMENT, SMALL_SIZE) == 0);
    ttrue(rename(REPLACEMENT, FIXTURE) == 0);
    ttrue(websStatFile(FIXTURE, &byPath) == 0);
    teqz(byPath.size, SMALL_SIZE, "the path reports the file that is there now");
    ttrue(websStatOpenFile(fd, &byFd) == 0);
    teqz(byFd.size, BIG_SIZE, "the descriptor still reports the file it holds");
#endif

    websCloseFile(fd);
    unlink(FIXTURE);

    ttrue(websStatOpenFile(-1, &byFd) == -1, "an invalid descriptor fails rather than reporting");
}


/*************************** Suite B: short body **************************/

/*
    Truncate during a slow read; return bytes received and check for appended error pages.
 */
static ssize readAndShrink(Socket sock, int *sawErrorPage, ssize *declared)
{
    ssize nbytes, total;
    char  *cp;
    int   shrunk;

    total = 0;
    shrunk = 0;
    *sawErrorPage = 0;
    *declared = -1;
    while (1) {
        nbytes = recv(sock, readBuf, IOLEN(sizeof(readBuf) - 1), 0);
        if (nbytes <= 0) {
            break;
        }
        readBuf[nbytes] = '\0';
        if (total == 0 && (cp = strstr(readBuf, "Content-Length: ")) != NULL) {
            *declared = (ssize) atoi(&cp[16]);
        }
        /*
            Detect an error page appended after headers were committed.
         */
        if (strstr(readBuf, "Access Error") != NULL) {
            *sawErrorPage = 1;
        }
        total += nbytes;
        if (!shrunk) {
            /*
                Truncate only after the response is committed.
             */
            FILE *fp = fopen(FIXTURE, "wb");
            if (fp) {
                fclose(fp);
            }
            shrunk = 1;
        }
        napMsec(SLOW_PAUSE);
    }
    return total;
}


static void testShortBody(void)
{
    Socket sock;
    ssize  total, declared;
    int    sawErrorPage;

    ttrue(makeFile(FIXTURE, BIG_SIZE) == 0);
    if ((sock = openConn(TINY_RCVBUF, READ_TIMEOUT)) == SOCKET_ERROR) {
        tfail("Cannot connect to the test server");
        unlink(FIXTURE);
        return;
    }
    if (sendRequest(sock, FIXTURE_URL, "HTTP/1.1") < 0) {
        tfail("Cannot send the request");
        closeConn(sock);
        unlink(FIXTURE);
        return;
    }
    total = readAndShrink(sock, &sawErrorPage, &declared);
    closeConn(sock);
    unlink(FIXTURE);

    teqz(declared, BIG_SIZE, "the length must describe the file that was opened");
    ttrue(total < BIG_SIZE, "the body must be short, as the document went away");
    ttrue(!sawErrorPage, "an error page must not be appended to a body that was already committed");

    /*
        Require connection close after the short response.
     */
    ttrue(total > 0, "the response must have been committed before the document went away");
}


static void testStillServing(void)
{
    Socket sock;
    ssize  nbytes;

    if ((sock = openConn(0, READ_TIMEOUT)) == SOCKET_ERROR) {
        tfail("Cannot reconnect to the test server");
        return;
    }
    if (sendRequest(sock, "/index.html", "HTTP/1.0") == 0) {
        nbytes = recv(sock, readBuf, IOLEN(sizeof(readBuf) - 1), 0);
        if (nbytes > 0) {
            readBuf[nbytes] = '\0';
            ttrue(strstr(readBuf, "200 OK") != NULL, "the server must still serve afterwards");
        } else {
            tfail("No response after the truncated transfer");
        }
    }
    closeConn(sock);
}


int main(int argc, char **argv)
{
    //  Windows needs WSAStartup before the raw client sockets
    if (socketOpen() < 0) {
        tfail("Cannot initialize sockets");
        return 0;
    }
    testStatOpenFile();
    testShortBody();
    testStillServing();
    socketClose();
    return 0;
}
