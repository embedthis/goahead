/*
    slow-reader.tst.c - Regression: a client that stops reading must not freeze the server forever.
 */

#include "testme.h"
#include "goahead.h"
#include "http-client-fixture.h"

#if ME_GOAHEAD_JAVASCRIPT

/*
    The flush deadline plus slack for connect, scheduling and the second request. The socket receive
    timeout is set to the same bound so a regression fails the assertion rather than hanging the suite.
 */
#define STALL_LIMIT     (ME_GOAHEAD_LIMIT_TIMEOUT + 20)
#define TINY_RCVBUF     512
#define SLOW_PAUSE      2
#define BIG_URL         "/huge.jst"

/*
    Keep scratch off the stack. The Windows CI runner gives a test exe a 1 MB stack.
 */
static char readBuf[64 * 1024];

/*
    Read the whole response, accumulating a length and an FNV-1a hash so multi-megabyte bodies can be
    compared without buffering them. Reads at most "chunk" bytes at a time and pauses "pause" msec
    between reads to model a slow reader. Headers are excluded from the hash and count: the Date
    header varies between requests. Returns the body byte count, or -1 on error.
 */
static ssize readResponse(Socket sock, int chunk, int pause, uint *hash)
{
    cchar *sep;
    ssize nbytes, body;
    int   i, matched;

    body = 0;
    matched = 0;
    sep = "\r\n\r\n";
    *hash = 2166136261U;
    if (chunk > (int) sizeof(readBuf)) {
        chunk = (int) sizeof(readBuf);
    }
    while (1) {
        nbytes = recv(sock, readBuf, IOLEN(chunk), 0);
        if (nbytes == 0) {
            break;
        }
        if (nbytes < 0) {
            return -1;
        }
        for (i = 0; i < nbytes; i++) {
            if (matched < 4) {
                /* Still in the headers. Track the blank line that ends them */
                if (readBuf[i] == sep[matched]) {
                    matched++;
                } else {
                    matched = (readBuf[i] == '\r') ? 1 : 0;
                }
                continue;
            }
            *hash = (*hash ^ (uchar) readBuf[i]) * 16777619U;
            body++;
        }
        if (pause) {
            napMsec(pause);
        }
    }
    return body;
}


/******************************** Suite A: integrity **************************/

/*
    A slow but reading client must receive exactly what a fast client receives.
 */
static void testSlowReaderIntegrity(void)
{
    ssize  fast, slow;
    uint   fastHash, slowHash;
    Socket sock;

    if ((sock = openConn(0, STALL_LIMIT)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server");
        return;
    }
    ttrue(sendRequest(sock, BIG_URL, "HTTP/1.0") == 0, "Cannot send request");
    fast = readResponse(sock, sizeof(readBuf), 0, &fastHash);
    closeConn(sock);
    tgtz(fast, 100000, "Response too small to reach the blocking flush path");
    if (fast <= 0) {
        return;
    }
    if ((sock = openConn(TINY_RCVBUF, STALL_LIMIT)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server");
        return;
    }
    ttrue(sendRequest(sock, BIG_URL, "HTTP/1.0") == 0, "Cannot send request");
    /*
        Read in small bites with a pause between them so the server's send buffer stays full and the
        response is written across many blocking flushes. The tiny receive buffer keeps the peer window
        closed, so the pause need only be long enough to yield - a longer one buys nothing but runtime,
        and the body is several megabytes read a few hundred bytes at a time.
     */
    slow = readResponse(sock, 16 * 1024, SLOW_PAUSE, &slowHash);
    closeConn(sock);

    teqz(slow, fast, "Slow reader received a different length");
    tequ(slowHash, fastHash, "Slow reader received different bytes");
}


/******************************** Suite B: recovery ***************************/

/*
    Stall the server with a client that requests a large dynamic response and never reads it, then
    verify a second client is served within the flush deadline. Before the fix the server blocked in
    send() indefinitely and the second request never completed.
 */
static void testStalledPeerIsBounded(void)
{
    WebsTime start, elapsed;
    ssize    nbytes;
    uint     hash;
    Socket   stalled, victim;

    if ((stalled = openConn(TINY_RCVBUF, STALL_LIMIT)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server");
        return;
    }
    ttrue(sendRequest(stalled, BIG_URL, "HTTP/1.1") == 0, "Cannot send request");

    /*
        Give the server time to accept, run the JST handler and block in the flush.
     */
    napMsec(500);
    start = time(0);

    if ((victim = openConn(0, STALL_LIMIT)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server");
        closeConn(stalled);
        return;
    }
    if (sendRequest(victim, "/index.html", "HTTP/1.1") < 0) {
        tfail("Cannot send request");
        closeConn(victim);
        closeConn(stalled);
        return;
    }
    nbytes = readResponse(victim, sizeof(readBuf), 0, &hash);
    elapsed = time(0) - start;

    closeConn(victim);
    closeConn(stalled);

    tgtz(nbytes, 0, "Server never served the second client - the event loop did not recover");
    tinfo("Second client served after %d secs (bound %d)", (int) elapsed, STALL_LIMIT);
    ttrue(elapsed <= STALL_LIMIT, "Recovery took %d secs, over the %d sec bound", (int) elapsed, STALL_LIMIT);
}


/******************************** Suite C: trickle ****************************/

#define TRICKLE_PAUSE   5

/*
    Drain the receive buffer periodically to let individual flushes finish, then check the whole-response deadline.
 */
static void testTricklingPeerIsBounded(void)
{
    WebsTime start, elapsed, lastRead;
    ssize    nbytes;
    Socket   trickler, victim;

    if ((trickler = openConn(TINY_RCVBUF, 1)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server");
        return;
    }
    ttrue(sendRequest(trickler, BIG_URL, "HTTP/1.1") == 0, "Cannot send request");
    napMsec(500);
    start = lastRead = time(0);

    if ((victim = openConn(0, 1)) == SOCKET_ERROR) {
        tfail("Cannot connect to test server");
        closeConn(trickler);
        return;
    }
    if (sendRequest(victim, "/index.html", "HTTP/1.1") < 0) {
        tfail("Cannot send request");
        closeConn(victim);
        closeConn(trickler);
        return;
    }
    nbytes = -1;
    while ((elapsed = time(0) - start) <= STALL_LIMIT) {
        if (time(0) - lastRead >= TRICKLE_PAUSE) {
            recv(trickler, readBuf, IOLEN(sizeof(readBuf)), 0);
            lastRead = time(0);
        }
        if ((nbytes = recv(victim, readBuf, IOLEN(sizeof(readBuf)), 0)) >= 0) {
            break;
        }
    }
    closeConn(victim);
    closeConn(trickler);

    tgtz(nbytes, 0, "Server did not serve the second client while a trickling client held it");
    tinfo("Second client served after %d secs (bound %d)", (int) elapsed, STALL_LIMIT);
    ttrue(elapsed <= STALL_LIMIT, "Recovery took %d secs, over the %d sec bound", (int) elapsed, STALL_LIMIT);
}


int main(int argc, char **argv)
{
    /*
        Initialize the socket layer. Windows needs WSAStartup before any socket call, and this test
        drives raw sockets rather than the server's socket API.
     */
    if (socketOpen() < 0) {
        tfail("Cannot initialize sockets");
        return 0;
    }
    testSlowReaderIntegrity();
    testStalledPeerIsBounded();
    testTricklingPeerIsBounded();
    socketClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("JavaScript templates are not enabled");
    return 0;
}
#endif /* ME_GOAHEAD_JAVASCRIPT */
