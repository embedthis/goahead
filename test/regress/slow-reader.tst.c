/*
    slow-reader.tst.c - Regression: a client that stops reading must not freeze the server forever.

    websWriteBlock flushes the output buffer with a blocking send whenever the buffer fills, so every
    dynamic response (JST, CGI, error bodies, headers) writes from inside the handler. Without a send
    timeout, a peer that stops reading blocks send() in the kernel and, because GoAhead is single
    threaded, freezes websServiceEvents with it. Nothing recovers the server until the peer reads or
    the TCP stack errors.

    websFlush now bounds the blocking flush: SO_SNDTIMEO limits each send and a whole-flush deadline
    of ME_GOAHEAD_LIMIT_TIMEOUT limits the retry loop, after which the peer is treated as a connection
    error and dropped. The stall is bounded, not eliminated - a handler cannot yield mid-response in a
    server without fibers - so this test asserts recovery within the bound, not concurrency.

    Two suites:
      A) integrity - a slow but reading client receives the same bytes as a fast client. A send that
         reports failure after the stack has queued data would resend those bytes and corrupt the body.
      B) recovery - a client that never reads is dropped, and a second client is served, within the
         flush deadline. Before the fix the second client never completed.

    Both suites request huge.jst, which is larger than the kernel socket buffers on either side. A
    response that fits in those buffers never reaches the blocking path and tests nothing. Suite B
    deliberately runs for up to ME_GOAHEAD_LIMIT_TIMEOUT seconds - that bound is what is under test.
 */

#include "testme.h"
#include "goahead.h"

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
    Length argument for send and recv. Winsock declares it int and POSIX declares it size_t, so an
    explicit cast is needed either way: without it MSVC warns C4267 and gcc warns -Wsign-conversion.
 */
#if ME_WIN_LIKE
    #define IOLEN(len)  ((int) (len))
#else
    #define IOLEN(len)  ((size_t) (len))
#endif

/*
    Keep scratch off the stack. The Windows CI runner gives a test exe a 1 MB stack.
 */
static char readBuf[64 * 1024];

/******************************* Socket helpers *******************************/

static void napMsec(int msec)
{
#if ME_WIN_LIKE
    Sleep((DWORD) msec);
#else
    struct timespec ts;
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}


/*
    Return the HTTP port of the test server. TM_HTTP is of the form http://127.0.0.1:18080.
 */
static int serverPort(void)
{
    cchar *http, *colon;

    http = tget("TM_HTTP", "http://127.0.0.1:18080");
    if ((colon = strrchr(http, ':')) != 0) {
        return atoi(&colon[1]);
    }
    return 18080;
}


/*
    Connect to the test server. If rcvbuf is non-zero, shrink the receive buffer before connecting so
    the server cannot park the response in the client's kernel buffers. Returns SOCKET_ERROR on failure.
 */
static Socket openConn(int rcvbuf, int timeout)
{
    struct sockaddr_in addr;
    Socket             sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    if (rcvbuf) {
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*) &rcvbuf, sizeof(rcvbuf));
    }
    if (timeout) {
#if ME_WIN_LIKE
        DWORD tv = (DWORD) (timeout * 1000);
#else
        struct timeval tv;
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
#endif
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof(tv));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((ushort) serverPort());
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return SOCKET_ERROR;
    }
    return sock;
}


static void closeConn(Socket sock)
{
    if (sock != SOCKET_ERROR) {
        closesocket(sock);
    }
}


/*
    Send a request. HTTP/1.0 suppresses chunked encoding, so the body arrives without chunk framing
    whose boundaries would otherwise vary with flush timing and defeat a byte comparison.
 */
static int sendRequest(Socket sock, cchar *url, cchar *protocol)
{
    char  request[512];
    ssize len;

    fmt(request, sizeof(request), "GET %s %s\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", url, protocol);
    len = (ssize) slen(request);
    return send(sock, request, IOLEN(len), 0) == len ? 0 : -1;
}


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
