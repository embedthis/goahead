/*
    trace-framing.tst.c - A TRACE response declares the length of the body it sends
 */

#undef ME_GOAHEAD_STEALTH
#define ME_GOAHEAD_STEALTH 0

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#include "../../src/options.c"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int loadRoutes(cchar *contents)
{
    cchar *path = "trace-framing.tmp";
    FILE  *fp;
    int   rc;

    if ((fp = fopen(path, "w")) == 0) {
        return -1;
    }
    fputs(contents, fp);
    fclose(fp);
    rc = websLoad(path);
    unlink(path);
    return rc;
}


static int readable(Socket sock)
{
    struct timeval tv;
    fd_set         readers;

    FD_ZERO(&readers);
    FD_SET(sock, &readers);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return select((int) sock + 1, &readers, NULL, NULL, &tv) > 0;
}


static ssize exchange(Socket client, char *buf, ssize bufsize)
{
    ssize len;
    int   i, n;

    len = 0;
    for (i = 0; i < 500; i++) {
        socketSelect(-1, 10);
        socketProcess();
        while (readable(client)) {
            n = (int) recv(client, &buf[len], (int) (bufsize - len - 1), 0);
            if (n <= 0) {
                buf[len] = '\0';
                return len;
            }
            len += n;
        }
    }
    buf[len] = '\0';
    return len;
}


int main(int argc, char **argv)
{
    struct sockaddr_in address;
    Socklen            length;
    Socket             client;
    cchar              *request, *echo, *headerEnd, *cl, *next;
    char               response[8192];
    ssize              declared;
    int                listener, rc;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    rc = websOpen("../web", NULL);
    teq(rc, 0, "initialize the server");
    websDefineHandler("traceProbe", 0, optionsHandler, 0, 0);
    teq(loadRoutes("route uri=/ methods=TRACE handler=traceProbe\n"), 0, "load the TRACE route");

    listener = socketListen("127.0.0.1", 0, websAccept, 0);
    ttrue(listener >= 0, "create listener");
    length = sizeof(address);
    rc = getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length);
    teq(rc, 0, "read listening address");

    client = socket(AF_INET, SOCK_STREAM, 0);
    ttrue(client != SOCKET_ERROR, "create client");
    rc = connect(client, (struct sockaddr*) &address, sizeof(address));
    teq(rc, 0, "connect client");

    request = "TRACE /first HTTP/1.1\r\nHost: x\r\n\r\n"
              "TRACE /second HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    rc = (int) send(client, request, (int) slen(request), 0);
    teq(rc, (int) slen(request), "send two pipelined TRACE requests");

    exchange(client, response, sizeof(response));
    ttrue(sstarts(response, "HTTP/1.1 200"), "the first TRACE must be answered");

    echo = "TRACE /first HTTP/1.1\r\n";
    headerEnd = strstr(response, "\r\n\r\n");
    cl = strstr(response, "Content-Length: ");
    ttrue(headerEnd != 0 && cl != 0 && cl < headerEnd, "the first response must declare a length");
    if (headerEnd && cl && cl < headerEnd) {
        declared = (ssize) atoi(&cl[16]);
        teq(declared, slen(echo), "the declared length must be the length of the echo");
        ttrue(sncmp(&headerEnd[4], echo, slen(echo)) == 0, "the body must be the echoed request line");
        next = &headerEnd[4 + declared];
        ttrue(sstarts(next, "HTTP/1.1 200"), "the second response must begin where the first one's length ends");
        ttrue(strstr(next, "TRACE /second HTTP/1.1") != 0, "the second response must echo the second request");
    }
    ttrue(strstr(response, "Content-Type: message/http") != 0, "a TRACE response is message/http");
    closesocket(client);
    socketCloseConnection(listener);
    websClose();
    return 0;
}
