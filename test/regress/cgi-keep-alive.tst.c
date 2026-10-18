/* A CGI response on a keep-alive connection leaves the connection open */

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

#if ME_GOAHEAD_CGI

#define RECV_TIMEOUT    10
#define LAST_CHUNK      "\r\n0\r\n\r\n"

static char readBuf[32 * 1024];

static int postForm(Socket sock, cchar *body)
{
    char  request[512];
    ssize len;

    fmt(request, sizeof(request), "POST /cgi-bin/cgitest HTTP/1.1\r\nHost: 127.0.0.1\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n%s", (int) slen(body), body);
    len = (ssize) slen(request);
    return send(sock, request, IOLEN(len), 0) == len ? 0 : -1;
}



static int readResponse(Socket sock)
{
    ssize nbytes, len;

    len = 0;
    readBuf[0] = '\0';
    while (!strstr(readBuf, LAST_CHUNK) && len < (ssize) sizeof(readBuf) - 1) {
        nbytes = recv(sock, &readBuf[len], IOLEN((ssize) sizeof(readBuf) - 1 - len), 0);
        if (nbytes <= 0) {
            return -1;
        }
        len += nbytes;
        readBuf[len] = '\0';
    }
    if (strncmp(readBuf, "HTTP/1.1 ", 9) != 0 || !strstr(readBuf, LAST_CHUNK)) {
        return -1;
    }
    return atoi(&readBuf[9]);
}


/*
    Remove chunk framing before matching body text.
 */
static void dechunk(void)
{
    char *in, *out, *end;
    long size;

    if ((in = strstr(readBuf, "\r\n\r\n")) == NULL) {
        return;
    }
    in += 4;
    out = in;
    while ((size = strtol(in, &end, 16)) > 0 && (end = strstr(end, "\r\n")) != NULL) {
        in = end + 2;
        if (size > (long) slen(in)) {
            break;
        }
        memmove(out, in, (size_t) size);
        out += size;
        in += size + 2;
    }
    *out = '\0';
}


int main(int argc, char **argv)
{
    Socket sock;
    int    status, rc;

    teq(socketOpen(), 0, "open the socket layer");
    sock = openConn(0, RECV_TIMEOUT);
    ttrue(sock != SOCKET_ERROR, "connect");

    rc = postForm(sock, "name=John");
    teq(rc, 0, "send the first post");
    status = readResponse(sock);
    teq(status, 200, "the first post is answered");
    ttrue(strstr(readBuf, "Connection: keep-alive") != NULL, "and the connection is kept alive");

    rc = postForm(sock, "name=Jane");
    teq(rc, 0, "send a second post on the same connection");
    status = readResponse(sock);
    teq(status, 200, "the second post is answered on the kept connection");
    dechunk();
    ttrue(strstr(readBuf, "Jane") != NULL, "with its own response");

    closeConn(sock);
    socketClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("CGI is not enabled");
    return 0;
}
#endif
