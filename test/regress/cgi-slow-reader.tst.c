/*
    cgi-slow-reader.tst.c - Regression: a CGI client that stops reading must not stall the server for other clients.
    CGI output is relayed from the poll loop; it must be handed to the socket without blocking the event loop.
 */

#include "testme.h"
#include "goahead.h"
#include "http-client-fixture.h"

#if ME_GOAHEAD_CGI

/*
    Well under the flush deadline (ME_GOAHEAD_LIMIT_TIMEOUT) that a blocking relay would hold the server for.
 */
#define VICTIM_LIMIT    5
#define TINY_RCVBUF     4096
#define CGI_URL         "/cgi-bin/cgitest?HTTP_SWITCHES=-b%20400000"

static char readBuf[16 * 1024];

static ssize readAll(Socket sock)
{
    ssize nbytes, total;

    total = 0;
    while ((nbytes = recv(sock, readBuf, IOLEN(sizeof(readBuf)), 0)) > 0) {
        total += nbytes;
    }
    return nbytes < 0 && total == 0 ? -1 : total;
}


/*
    The CGI must be runnable on this server before the stall is measured.
 */
static int cgiAvailable(void)
{
    Socket sock;
    ssize  total;

    if ((sock = openConn(0, 20)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server");
        return 0;
    }
    ttrue(sendRequest(sock, "/cgi-bin/cgitest?HTTP_SWITCHES=-b%2010", "HTTP/1.0") == 0, "Cannot send request");
    total = readAll(sock);
    closeConn(sock);
    return total > 0 && strstr(readBuf, "200") == 0 ? 1 : total > 0;
}


static void testStalledCgiClientIsIsolated(void)
{
    WebsTime start, elapsed;
    ssize    nbytes;
    Socket   stalled, victim;

    /*
        Request a CGI response far larger than the socket buffers and never read it.
     */
    if ((stalled = openConn(TINY_RCVBUF, VICTIM_LIMIT + 2)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server");
        return;
    }
    ttrue(sendRequest(stalled, CGI_URL, "HTTP/1.1") == 0, "Cannot send request");
    napMsec(1500);

    start = time(0);
    if ((victim = openConn(0, VICTIM_LIMIT + 2)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server while a CGI client is stalled");
        closeConn(stalled);
        return;
    }
    if (sendRequest(victim, "/index.html", "HTTP/1.0") < 0) {
        tfail("Cannot send request");
        closeConn(victim);
        closeConn(stalled);
        return;
    }
    nbytes = readAll(victim);
    elapsed = time(0) - start;
    closeConn(victim);
    closeConn(stalled);

    tgtz(nbytes, 0, "The second client was not served while a CGI client stopped reading");
    tinfo("Second client served after %d secs (bound %d)", (int) elapsed, VICTIM_LIMIT);
    ttrue(elapsed <= VICTIM_LIMIT, "Serving the second client took %d secs, over the %d sec bound",
          (int) elapsed, VICTIM_LIMIT);
}


int main(int argc, char **argv)
{
    if (socketOpen() < 0) {
        tfail("Cannot initialize sockets");
        return 0;
    }
    if (!cgiAvailable()) {
        tskip("The cgitest program is not served at /cgi-bin/");
    } else {
        testStalledCgiClientIsIsolated();
    }
    socketClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("CGI is not enabled");
    return 0;
}
#endif /* ME_GOAHEAD_CGI */
