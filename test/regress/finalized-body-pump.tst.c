/*
    A response finalized before its request body arrives must drain and close.
    Fill the send buffer to keep the response pending, then exercise both an oversized chunked body
    and a complete fixed-length body. Neither may re-enter the body filter or lose write interest.
    A watchdog bounds each exchange.
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
#include "http-client-fixture.h"

#if ME_UNIX_LIKE && ME_GOAHEAD_AUTH

#define SPIN_LIMIT      30          /* Seconds one exchange may take before the event loop is spinning */
#define INGEST_PASSES   60          /* Event loop passes the server gets to take the body, 50ms each */
#define READ_PASSES     200         /* Client read passes while the server drains, 50ms each */
#define CHUNK_SIZE      4096
#define CHUNKS          5           /* 5 * 4096 exceeds the 16K POST limit */
#define BODY_SIZE       8192
#define SMALL_BUFFER    4096        /* Socket buffer size at both ends, so the fill is quick and stays full */

static int listener;
static ushort port;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static void spinning(int signo)
{
    static const char msg[] = "FAIL: the event loop is spinning on a finalized request\n";

    (void) !write(2, msg, sizeof(msg) - 1);
    _exit(1);
}


static void drive(int passes)
{
    while (passes-- > 0) {
        if (socketSelect(-1, 50)) {
            socketProcess();
        }
        websRunEvents();
    }
}


static Socket openClient(void)
{
    struct sockaddr_in addr;
    struct timeval     tv;
    Socket             sock;
    int                size;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof(tv));
    size = SMALL_BUFFER;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*) &size, sizeof(size));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        closesocket(sock);
        return SOCKET_ERROR;
    }
    return sock;
}


/*
    Fill both socket buffers to keep the next response pending. Return the queued byte count.
 */
static ssize fillSendBuffer(Socket sock)
{
    char  junk[4096];
    ssize nbytes, total;

    int   idle, size;

    memset(junk, 'J', sizeof(junk));
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
    //  An explicit size also stops the stack growing the buffer later, which would make room for the response
    size = SMALL_BUFFER;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*) &size, sizeof(size));
    /*
        Queued bytes move to the peer's receive buffer after send reports the buffer full, which frees room
        again. Keep topping up until two passes queue nothing, so both ends are full.
     */
    for (total = 0, idle = 0; idle < 2; ) {
        for (nbytes = 0; (nbytes = send(sock, junk, sizeof(junk), 0)) > 0; total += nbytes) {
            idle = -1;
        }
        idle++;
        napMsec(50);
    }
    return total;
}


/*
    Drive the server while draining output. Return 1 for a redirect, 0 if absent, or -1 if still open.
 */
static int readResponse(Socket client)
{
    char  buf[16384], window[64];
    ssize nbytes, keep;
    int   found, passes;

    found = 0;
    memset(window, 0, sizeof(window));
    for (passes = 0; passes < READ_PASSES; passes++) {
        drive(1);
        memcpy(buf, window, sizeof(window));
        nbytes = recv(client, &buf[sizeof(window)], sizeof(buf) - sizeof(window) - 1, 0);
        if (nbytes == 0) {
            return found;
        }
        if (nbytes < 0) {
            continue;
        }
        buf[sizeof(window) + nbytes] = '\0';
        if (strstr(buf, "HTTP/1.1 302 ") != 0) {
            found = 1;
        }
        //  Keep a tail so a status line split across reads is still seen
        keep = min(nbytes, (ssize) sizeof(window));
        memset(window, 0, sizeof(window));
        memcpy(window, &buf[sizeof(window) + nbytes - keep], keep);
    }
    return found ? -1 : 0;
}


/*
    Send a request that redirects before its body is read.
 */
static void run(cchar *scenario, cchar *framing, cchar *body, ssize bodyLen)
{
    WebsSocket *sp;
    Socket     client;
    char       request[512];
    ssize      filled;
    int        rc;

    client = openClient();
    ttrue(client != SOCKET_ERROR, "%s: connect to the test listener", scenario);
    drive(2);
    teq(websGetConnectionCount(), 1, "%s: the connection is accepted", scenario);

    //  The accepted socket takes the handle after the listener
    sp = socketPtr(listener + 1);
    ttrue(sp != NULL && !(sp->flags & SOCKET_LISTENING), "%s: find the accepted socket", scenario);
    filled = fillSendBuffer(sp->sock);
    ttrue(filled > 0, "%s: the send buffer is full", scenario);

    fmt(request, sizeof(request), "POST /auth/form/page.html HTTP/1.1\r\nHost: 127.0.0.1\r\n%s\r\n", framing);
    rc = send(client, request, IOLEN(slen(request)), 0) == (ssize) slen(request) ? 0 : -1;
    teq(rc, 0, "%s: send the request head", scenario);
    rc = send(client, body, IOLEN(bodyLen), 0) == bodyLen ? 0 : -1;
    teq(rc, 0, "%s: send the body", scenario);

    /*
        Process the body before reading the response, so the send buffer stays full.
        The watchdog bounds the exchange.
     */
    signal(SIGALRM, spinning);
    alarm(SPIN_LIMIT);
    drive(INGEST_PASSES);
    rc = readResponse(client);
    alarm(0);
    teq(rc, 1, "%s: the client reads the redirect and the connection then closes", scenario);
    closeConn(client);
    drive(2);
    teq(websGetConnectionCount(), 0, "%s: the connection is released", scenario);
}


int main(int argc, char **argv)
{
    struct sockaddr_in address;
    Socklen            length;
    char               *chunked, *body, *routes;
    ssize              len;
    int                i, rc;

    signal(SIGPIPE, SIG_IGN);
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    routes = getenv("TM_ROUTES");
    rc = websOpen("../web", routes && *routes ? routes : "../route.txt");
    teq(rc, 0, "initialize the HTTP engine");
    listener = socketListen("127.0.0.1", 0, websAccept, 0);
    ttrue(listener >= 0, "create the listener");
    length = sizeof(address);
    rc = getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length);
    teq(rc, 0, "read the listening address");
    port = ntohs(address.sin_port);

    /*
        Scenario 1: a chunked body over the POST limit. The refusal in filterChunkData consumes nothing.
     */
    chunked = walloc(CHUNKS * (CHUNK_SIZE + 16) + 8);
    len = 0;
    for (i = 0; i < CHUNKS; i++) {
        fmt(&chunked[len], 16, "%x\r\n", CHUNK_SIZE);
        len += slen(&chunked[len]);
        memset(&chunked[len], 'x', CHUNK_SIZE);
        len += CHUNK_SIZE;
        fmt(&chunked[len], 4, "\r\n");
        len += 2;
    }
    fmt(&chunked[len], 8, "0\r\n\r\n");
    len += 5;
    run("chunked", "Transfer-Encoding: chunked\r\n", chunked, len);
    wfree(chunked);

    /*
        Scenario 2: a Content-Length body that completes. processContent must not drop the write interest.
     */
    body = walloc(BODY_SIZE);
    memset(body, 'x', BODY_SIZE);
    run("content-length", "Content-Length: 8192\r\n", body, BODY_SIZE);
    wfree(body);

    socketCloseConnection(listener);
    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Needs POSIX sockets, signals and authentication");
    return 0;
}
#endif
