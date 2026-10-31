/*
    action-slow-reader.tst.c - An action handler writing to a client that is not reading must not block the server.
    Output within ME_GOAHEAD_LIMIT_RESPONSE_BUFFER is buffered and drained by writable events, so the handler
    returns at once and the client later receives the whole response. A watchdog fails a blocking write.
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

#if ME_UNIX_LIKE

#define BLOCK_LIMIT     10          /* Seconds before the handler is deemed blocked in a flush */
#define HANDLER_LIMIT   2           /* Seconds the handler may take with buffered output */
#define DRIVE_PASSES    40          /* Event loop passes for the request to run, 50ms each */
#define READ_PASSES     400         /* Client read passes while the server drains, 50ms each */
#define SMALL_BUFFER    4096        /* Socket buffer size at both ends, so the fill is quick and stays full */
#define BODY_SIZE       (48 * 1024) /* Far over the output ring, under ME_GOAHEAD_LIMIT_RESPONSE_BUFFER */
#define BODY_CHAR       '#'

static int    listener;
static ushort port;
static int    actionDone;
static char   readBuf[16 * 1024];

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static void blocked(int signo)
{
    static const char msg[] = "FAIL: the action handler blocked the server on a client that is not reading\n";

    (void) !write(2, msg, sizeof(msg) - 1);
    _exit(1);
}


static void bigWrite(Webs *wp)
{
    char block[1024];
    int  i;

    memset(block, BODY_CHAR, sizeof(block));
    websSetStatus(wp, 200);
    websWriteHeaders(wp, -1, 0);
    websWriteHeader(wp, "Content-Type", "text/plain");
    websWriteEndHeaders(wp);
    for (i = 0; i < BODY_SIZE / (int) sizeof(block); i++) {
        websWriteBlock(wp, block, sizeof(block));
    }
    websDone(wp);
    actionDone = 1;
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
    Fill both socket buffers from the server end, so any response output the ring cannot hold waits on the client
    however much the stack buffers. Junk is not BODY_CHAR, so readBody skips it.
 */
static ssize fillSendBuffer(Socket sock)
{
    char  junk[4096];
    ssize nbytes, total;
    int   flags, idle, size;

    memset(junk, 'J', sizeof(junk));
    flags = fcntl(sock, F_GETFL);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    //  An explicit size also stops the stack growing the buffer later, which would make room for the response
    size = SMALL_BUFFER;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*) &size, sizeof(size));
    for (total = 0, idle = 0; idle < 2; ) {
        for (nbytes = 0; (nbytes = send(sock, junk, sizeof(junk), 0)) > 0; total += nbytes) {
            idle = -1;
        }
        idle++;
        napMsec(50);
    }
    fcntl(sock, F_SETFL, flags);
    return total;
}


/*
    Drive the server while reading the response to its close. Return the count of body characters received.
 */
static ssize readBody(Socket client)
{
    ssize nbytes, count;
    int   i, passes;

    count = 0;
    for (passes = 0; passes < READ_PASSES; passes++) {
        drive(1);
        if ((nbytes = recv(client, readBuf, sizeof(readBuf), 0)) == 0) {
            return count;
        }
        for (i = 0; i < nbytes; i++) {
            count += readBuf[i] == BODY_CHAR;
        }
    }
    return -1;
}


static void run(cchar *protocol)
{
    WebsSocket *sp;
    WebsTime   start, elapsed;
    Socket     client;
    char       request[256];
    ssize      body, filled;
    int        rc;

    client = openClient();
    ttrue(client != SOCKET_ERROR, "%s: connect to the test listener", protocol);
    drive(2);
    teq(websGetConnectionCount(), 1, "%s: the connection is accepted", protocol);

    //  The accepted socket takes the handle after the listener
    sp = socketPtr(listener + 1);
    ttrue(sp != NULL && !(sp->flags & SOCKET_LISTENING), "%s: find the accepted socket", protocol);
    filled = fillSendBuffer(sp->sock);
    ttrue(filled > 0, "%s: the send buffer is full", protocol);

    fmt(request, sizeof(request), "GET /action/bigWrite %s\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n",
        protocol);
    rc = send(client, request, IOLEN(slen(request)), 0) == (ssize) slen(request) ? 0 : -1;
    teq(rc, 0, "%s: send the request", protocol);

    //  Run the request without reading the response
    actionDone = 0;
    signal(SIGALRM, blocked);
    alarm(BLOCK_LIMIT);
    start = time(0);
    for (rc = 0; rc < DRIVE_PASSES && !actionDone; rc++) {
        drive(1);
    }
    elapsed = time(0) - start;
    alarm(0);
    ttrue(actionDone, "%s: the action runs", protocol);
    ttrue(elapsed <= HANDLER_LIMIT, "%s: the action took %d secs with the client not reading", protocol,
          (int) elapsed);

    body = readBody(client);
    teqz(body, BODY_SIZE, "%s: the client then receives the whole response", protocol);
    closeConn(client);
    drive(2);
    teq(websGetConnectionCount(), 0, "%s: the connection is released", protocol);
}


int main(int argc, char **argv)
{
    struct sockaddr_in address;
    Socklen            length;
    char               *routes;
    int                rc;

    signal(SIGPIPE, SIG_IGN);
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    routes = getenv("TM_ROUTES");
    rc = websOpen("../web", routes && *routes ? routes : "../route.txt");
    teq(rc, 0, "initialize the HTTP engine");
    websDefineAction("bigWrite", bigWrite);
    listener = socketListen("127.0.0.1", 0, websAccept, 0);
    ttrue(listener >= 0, "create the listener");
    length = sizeof(address);
    rc = getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length);
    teq(rc, 0, "read the listening address");
    port = ntohs(address.sin_port);

    //  HTTP/1.0 buffers in the output ring; HTTP/1.1 chunks through the chunk buffer
    run("HTTP/1.0");
    run("HTTP/1.1");

    socketCloseConnection(listener);
    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Needs POSIX sockets and signals");
    return 0;
}
#endif
