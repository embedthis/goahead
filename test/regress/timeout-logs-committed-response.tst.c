/*
    A committed response cut short by timeout must produce one access log entry.
    Use a private loopback server with a streaming handler and a client that stops reading.
 */

#define ME_GOAHEAD_ACCESS_LOG 1

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_UNIX_LIKE && !ME_ROM
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define SMALL_BUFFER    4096                /* Client receive buffer, so the transfer stalls at once */
#define STALL_PASSES    40                  /* Event loop passes of 50 ms the writer gets to fill the socket */
#define LARGE_BODY      (64 * 1024 * 1024)  /* Declared length of the streamed response */
#define SMALL_BODY      100

static int listener;
static int port;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif


static char *readLog(void)
{
    char  *buf;
    ssize len;
    int   fd;

    if ((fd = open("access.log", O_RDONLY | O_BINARY, 0)) < 0) {
        return sclone("");
    }
    buf = walloc(8192);
    len = read(fd, buf, 8191);
    close(fd);
    buf[len > 0 ? len : 0] = '\0';
    return buf;
}


static int countLines(cchar *text)
{
    int count;

    for (count = 0; *text; text++) {
        if (*text == '\n') {
            count++;
        }
    }
    return count;
}


/*
    Stream the declared body as the socket accepts it, as the file handler's writer does
 */
static void streamWriter(Webs *wp)
{
    char  buf[ME_GOAHEAD_LIMIT_BUFFER];
    ssize size, wrote, slice;
    int   err;

    memset(buf, 'S', sizeof(buf));
    slice = ME_GOAHEAD_LIMIT_SLICE;
    while (wp->txRemaining > 0 && slice > 0) {
        size = (ssize) min(wp->txRemaining, (Offset) sizeof(buf));
        if ((wrote = websWriteSocket(wp, buf, size)) < 0) {
            err = socketGetError(wp->sid);
            if (err != EWOULDBLOCK && err != EAGAIN) {
                wp->flags &= ~WEBS_KEEP_ALIVE;
                wp->state = WEBS_COMPLETE;
                websDone(wp);
            }
            return;
        }
        wp->txRemaining -= wrote;
        slice -= wrote;
    }
    if (wp->txRemaining <= 0) {
        websDone(wp);
    }
}


static bool stallHandler(Webs *wp)
{
    Offset length;

    length = smatch(wp->path, "/stall/large") ? LARGE_BODY : SMALL_BODY;
    websSetStatus(wp, HTTP_CODE_OK);
    websWriteHeaders(wp, length, 0);
    websWriteHeader(wp, "Content-Type", "text/plain");
    websWriteEndHeaders(wp);
    wp->txRemaining = length;
    websSetBackgroundWriter(wp, streamWriter);
    return 1;
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
    Socket             sock;
    int                size;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    size = SMALL_BUFFER;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*) &size, sizeof(size));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((ushort) port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        closesocket(sock);
        return SOCKET_ERROR;
    }
    return sock;
}


static Webs *liveRequest(void)
{
    int wid;

    for (wid = 0; wid < websMax; wid++) {
        if (webs[wid]) {
            return webs[wid];
        }
    }
    return 0;
}


static void stalledTransferTimesOut(void)
{
    Webs   *wp;
    Socket client;
    char   *log;
    cchar  *request;
    int    before;

    tinfo("\n--- A stalled transfer closed by the request timeout is logged ---\n");

    log = readLog();
    before = countLines(log);
    wfree(log);
    client = openClient();
    ttrue(client != SOCKET_ERROR, "connect a client that reads nothing");
    if (client == SOCKET_ERROR) {
        return;
    }
    request = "GET /stall/large HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    ttrue(send(client, request, strlen(request), 0) == (ssize) strlen(request), "send the request");
    drive(STALL_PASSES);

    wp = liveRequest();
    ttrue(wp != 0, "the request is still open: the client has not taken the response");
    if (wp) {
        ttrue((wp->flags & WEBS_HEADERS_CREATED) != 0, "the response headers were committed");
        ttrue(!wp->finalized, "the transfer has not completed");
        ttrue(wp->txRemaining > 0, "the response is stalled in the socket");
        /*
            Expire this request immediately by moving its idle timestamp back.
         */
        wp->timestamp -= ME_GOAHEAD_LIMIT_TIMEOUT + 1;
        checkTimeout(wp, wp->timeout);
        /* wp is freed */
        drive(2);
        ttrue(liveRequest() == 0, "the timeout closed and freed the request");
    }
    log = readLog();
    teq(countLines(log) - before, 1, "the truncated response must leave exactly one access log line");
    ttrue(strstr(log, "\"GET /stall/large HTTP/1.1\" 200 ") != 0, "the line must name the request and its status");
    wfree(log);
    closesocket(client);
}


static void completedTransferLoggedOnce(void)
{
    Socket client;
    char   buf[8192], *log;
    cchar  *request;
    ssize  nbytes;
    int    passes, before;

    tinfo("\n--- A completed transfer is logged exactly once ---\n");

    log = readLog();
    before = countLines(log);
    wfree(log);
    client = openClient();
    ttrue(client != SOCKET_ERROR, "connect a client");
    if (client == SOCKET_ERROR) {
        return;
    }
    request = "GET /stall/small HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    ttrue(send(client, request, strlen(request), 0) == (ssize) strlen(request), "send the request");
    for (passes = 0; passes < 400; passes++) {
        drive(1);
        while ((nbytes = recv(client, buf, sizeof(buf), MSG_DONTWAIT)) > 0) { }
        if (nbytes == 0) {
            break;
        }
    }
    drive(2);
    log = readLog();
    teq(countLines(log) - before, 1, "a completed transfer is logged once");
    ttrue(strstr(log, "\"GET /stall/small HTTP/1.1\" 200 ") != 0, "the line names the completed request");
    wfree(log);
    closesocket(client);
}

#endif /* ME_UNIX_LIKE && !ME_ROM */


int main(int argc, char **argv)
{
    tinfo("timeout-logs-committed-response: a response cut short by the request timeout is logged\n");
#if ME_UNIX_LIKE && !ME_ROM
    {
        struct sockaddr_in address;
        Socklen            length;

#if ME_GOAHEAD_LOGGING
        logSetHandler(quietLog);
#endif
        unlink("access.log");
        teq(websOpen("../web", NULL), 0, "initialize the server with the access log");
        websDefineHandler("stall", 0, stallHandler, 0, 0);
        ttrue(websAddRoute("/stall/", "stall", 0) != 0, "define the streaming route");
        listener = socketListen("127.0.0.1", 0, websAccept, 0);
        ttrue(listener >= 0, "create the listener");
        if (listener >= 0) {
            length = sizeof(address);
            teq(getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length), 0, "read the port");
            port = ntohs(address.sin_port);
            stalledTransferTimesOut();
            completedTransferLoggedOnce();
            socketCloseConnection(listener);
        }
        websClose();
        unlink("access.log");
    }
#else
    tskip("The access log needs a file system and POSIX sockets");
#endif
    return 0;
}
