/*
    listener-closed-port.tst.c - A request on a connection whose listening socket has been closed is still parsed

    Invariant: parseFirstLine derives the request port from the listening socket, and when that socket is gone it
    falls back to the port in the request target. An origin-form target carries no port, so websUrlParse returns
    a NULL port, and the fallback must not read it. Against the defect, the first request on any connection whose
    listener has been closed reads NULL and the server dies.

    The listener is closed by the embedding application: socketCloseConnection is public API and an application
    that stops accepting on an endpoint while serving its open connections lands here. The connection is accepted
    first, the listener is closed, then the request bytes arrive.

    Oracle: the client receives an HTTP status line. Against the defect the test process crashes.

    Runs from the test directory: the documents are its web/ directory, as for the shared test server.
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

#if ME_UNIX_LIKE

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    Service the event loop for up to the given milliseconds, or until the predicate holds.
 */
static void pump(int ms, bool (*done)(void))
{
    int i;

    for (i = 0; i < ms / 10; i++) {
        if (done && done()) {
            return;
        }
        if (socketSelect(-1, 10)) {
            socketProcess();
        }
        websRunEvents();
    }
}

static bool accepted(void)
{
    return websGetConnectionCount() >= 1;
}

int main(int argc, char **argv)
{
    struct sockaddr_in address;
    struct timeval     timeout = { 5, 0 };
    WebsSocket         *lp;
    Socklen            length;
    Socket             client;
    cchar              *request;
    char               response[256];
    int                listener, rc, got;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    listener = websListen("http://127.0.0.1:0");
    ttrue(listener >= 0, "listen on an ephemeral port");
    if (listener < 0) {
        return 0;
    }
    lp = socketPtr(listener);
    length = sizeof(address);
    teq(getsockname(lp->sock, (struct sockaddr*) &address, &length), 0, "read the listening address");

    client = socket(AF_INET, SOCK_STREAM, 0);
    ttrue(client != SOCKET_ERROR, "create a client socket");
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout));
    teq(connect(client, (struct sockaddr*) &address, sizeof(address)), 0, "connect the client");

    pump(3000, accepted);
    teq(websGetConnectionCount(), 1, "the connection is accepted before the listener closes");

    /*
        The application stops listening while the accepted connection is still open.
     */
    socketCloseConnection(listener);
    ttrue(socketPtr(listener) == NULL, "the listening socket is gone");

    request = "GET / HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    rc = (int) send(client, request, strlen(request), 0);
    teq(rc, (int) strlen(request), "send a request with an origin-form target, which names no port");

    pump(3000, NULL);
    got = (int) recv(client, response, sizeof(response) - 1, 0);
    ttrue(got > 0, "the request is answered after the listener closed (got %d)", got);
    if (got > 0) {
        response[got] = '\0';
        ttrue(strncmp(response, "HTTP/1.", 7) == 0, "the answer is an HTTP response: %.40s", response);
    }
    closesocket(client);
    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Needs BSD sockets and an in-process server");
    return 0;
}
#endif
