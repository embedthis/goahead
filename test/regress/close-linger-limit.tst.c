/* Lingering closes are bounded in number and in time */

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

#define EXCESS          4
#define COUNT           (ME_GOAHEAD_LIMIT_LINGER_CONNECTIONS + EXCESS)
#define RECV_TIMEOUT    10

static Socket socks[COUNT];


static int finish(Socket sock)
{
    cchar *request;
    char  buf[4096];
    ssize nbytes;

    request = "GET /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (send(sock, request, IOLEN(slen(request)), 0) < 0) {
        return 0;
    }
    while ((nbytes = recv(sock, buf, sizeof(buf), 0)) > 0) {
    }
    return nbytes == 0;
}

static void suiteA_lingerBound(void)
{
    char buf[16];
    int  i, finished, resets;

    tinfo("\n--- Suite A: at most the bound linger ---\n");
    finished = 0;
    for (i = 0; i < COUNT; i++) {
        if ((socks[i] = openConn(0, RECV_TIMEOUT)) != SOCKET_ERROR && finish(socks[i])) {
            finished++;
        }
    }
    teq(finished, COUNT, "every response is read through to the end of stream");

    /*
        The first send reaches a closed socket and draws a reset, which the second send or the read then reports
     */
    for (i = 0; i < COUNT; i++) {
        send(socks[i], "x", 1, 0);
    }
    napMsec(250);
    resets = 0;
    for (i = 0; i < COUNT; i++) {
        if (send(socks[i], "x", 1, 0) < 0 || recv(socks[i], buf, sizeof(buf), 0) < 0) {
            resets++;
        }
        closeConn(socks[i]);
    }
    ttrue(resets >= EXCESS, "connections past the linger bound close at once, got %d resets", resets);
    ttrue(resets < COUNT, "connections within the bound linger, got %d resets of %d", resets, COUNT);
}

static void suiteB_lingerExpires(void)
{
    Socket held, sock;
    char   buf[16];
    int    reset, rc;

    tinfo("\n--- Suite B: a linger ends when its time passes ---\n");
    held = openConn(0, RECV_TIMEOUT);
    rc = held != SOCKET_ERROR && finish(held);
    ttrue(rc, "finish a connection and leave it open");
    napMsec((ME_GOAHEAD_LIMIT_LINGER + 2) * 1000);
    send(held, "x", 1, 0);
    napMsec(250);
    reset = send(held, "x", 1, 0) < 0 || recv(held, buf, sizeof(buf), 0) < 0;
    closeConn(held);
    ttrue(reset, "the lingering connection is closed once the linger time passes");

    sock = openConn(0, RECV_TIMEOUT);
    rc = sock != SOCKET_ERROR && finish(sock);
    closeConn(sock);
    ttrue(rc, "the server goes on serving after a linger expires");
}

int main(int argc, char **argv)
{
    int rc;

#if ME_UNIX_LIKE
    signal(SIGPIPE, SIG_IGN);
#endif
    rc = socketOpen();
    teq(rc, 0, "open the socket layer");
    suiteA_lingerBound();
    suiteB_lingerExpires();
    socketClose();
    return 0;
}
