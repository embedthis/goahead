/*
    After a handler cancels its timeout, keep-alive reuse must restore a live timeout handle
    and close a partial second request at the parse deadline.
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

#define RESPONSE_PASSES 100                             /* 50ms event loop passes to serve the first request */
#define CLOSE_SECS      (ME_GOAHEAD_LIMIT_PARSE_TIMEOUT + 4) /* Seconds for the parse timeout to close a partial request */

static int    listener;
static ushort port;
static char   readBuf[4096];

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif


static void cancelTimeoutAction(Webs *wp)
{
    websCancelTimeout(wp);
    websSetStatus(wp, 200);
    websWriteHeaders(wp, 2, 0);
    websWriteEndHeaders(wp);
    websWriteBlock(wp, "ok", 2);
    websDone(wp);
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

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof(tv));
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
    Drive the server until the whole first response has arrived. Return the status code, or -1.
 */
static int readResponse(Socket client)
{
    ssize nbytes, len;
    int   passes;

    len = 0;
    readBuf[0] = '\0';
    for (passes = 0; passes < RESPONSE_PASSES; passes++) {
        drive(1);
        if ((nbytes = recv(client, &readBuf[len], IOLEN((ssize) sizeof(readBuf) - 1 - len), 0)) > 0) {
            len += nbytes;
            readBuf[len] = '\0';
            if (strstr(readBuf, "\r\n\r\nok")) {
                return strncmp(readBuf, "HTTP/1.1 ", 9) == 0 ? atoi(&readBuf[9]) : -1;
            }
        } else if (nbytes == 0) {
            return -1;
        }
    }
    return -1;
}


int main(int argc, char **argv)
{
    struct sockaddr_in address;
    Socklen            length;
    WebsSocket         *sp;
    Webs               *wp;
    Socket             client;
    cchar              *request, *partial, *routes;
    WebsTime           start;
    int                rc;
    bool               hasTimeout;

    signal(SIGPIPE, SIG_IGN);
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    routes = getenv("TM_ROUTES");
    rc = websOpen("../web", routes && *routes ? routes : "../route.txt");
    teq(rc, 0, "initialize the HTTP engine");
    websDefineAction("cancelTimeout", cancelTimeoutAction);
    listener = socketListen("127.0.0.1", 0, websAccept, 0);
    ttrue(listener >= 0, "create the listener");
    length = sizeof(address);
    rc = getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length);
    teq(rc, 0, "read the listening address");
    port = ntohs(address.sin_port);

    client = openClient();
    ttrue(client != SOCKET_ERROR, "connect to the test listener");
    drive(2);
    teq(websGetConnectionCount(), 1, "the connection is accepted");

    request = "GET /action/cancelTimeout HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    rc = send(client, request, IOLEN(slen(request)), 0) == (ssize) slen(request) ? 0 : -1;
    teq(rc, 0, "send the first request");
    teq(readResponse(client), 200, "the timeout-cancelling action answers");
    drive(2);
    teq(websGetConnectionCount(), 1, "the connection is kept alive");

    //  The accepted socket takes the handle after the listener, and its handler data is the connection's Webs
    sp = socketPtr(listener + 1);
    ttrue(sp != NULL && !(sp->flags & SOCKET_LISTENING), "find the accepted socket");
    wp = sp ? (Webs*) sp->handler_data : 0;
    ttrue(wp != NULL && websValid(wp), "find the connection");
    hasTimeout = wp && wp->timeout >= 0;

    //  A partial request line with no terminator: only the parse timeout can end it
    partial = "GET /never-finished";
    rc = send(client, partial, IOLEN(slen(partial)), 0) == (ssize) slen(partial) ? 0 : -1;
    teq(rc, 0, "send a partial second request");
    start = time(0);
    while (websGetConnectionCount() > 0 && (time(0) - start) < CLOSE_SECS) {
        drive(1);
    }
    teq(websGetConnectionCount(), 0, "the parse timeout closes the partial request within %d seconds", CLOSE_SECS);
    ttrue(hasTimeout, "the keep-alive reset left the connection with a timeout handle");

    closeConn(client);
    socketCloseConnection(listener);
    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Needs POSIX sockets");
    return 0;
}
#endif
