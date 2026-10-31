/*
    tls-handshake-blocked.tst.c - A TLS handshake whose output does not fit the socket buffer must still complete

    The server's first flight is larger than the socket can take at once, so OpenSSL reports that it wants to write.
    The event loop then has to wait for the socket to become writable and continue the handshake from a writable
    event; a loop that waits only for input never hears from a client that is itself waiting for the server.
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

#if ME_COM_OPENSSL && ME_UNIX_LIKE
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#include "../../src/socket.c"
#include "../../src/openssl.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define SMALL  1024                 /* Socket buffers smaller than the server's first flight */
#define ROUNDS 400                  /* Event loop turns of 5 ms allowed for the handshake */

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    A server connection on one end of a socket pair, registered as websAccept registers it, and an OpenSSL client
    on the other end. The server's send buffer and the client's receive buffer are both too small for the server's
    first flight.
 */
static Webs *connectPair(SSL_CTX *clientCtx, SSL **client, int *clientFd)
{
    Webs *wp;
    int  fds[2], sid, wid, size;

    ttrue(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "create a socket pair");
    size = SMALL;
    setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    socketPtr(sid)->sock = fds[0];
    socketHighestFd = max(socketHighestFd, fds[0]);
    socketSetBlock(sid, 0);
    wid = websAlloc(sid);
    wp = webs[wid];
    wp->flags |= WEBS_SECURE;
    teq(sslUpgrade(wp), 0, "start TLS on the server end");
    socketCreateHandler(sid, SOCKET_READABLE, socketEvent, wp);

    *client = SSL_new(clientCtx);
    SSL_set_fd(*client, fds[1]);
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
    SSL_set_connect_state(*client);
    *clientFd = fds[1];
    return wp;
}

/*
    Alternate the client with turns of the server's event loop until both ends finish the handshake. The client
    reads after it finishes so the session tickets the server sends can drain.
 */
static bool handshake(Webs *wp, SSL *client)
{
    char buf[256];
    bool clientDone;
    int  i;

    clientDone = 0;
    for (i = 0; i < ROUNDS; i++) {
        if (!clientDone && SSL_do_handshake(client) == 1) {
            clientDone = 1;
        } else if (clientDone) {
            SSL_read(client, buf, sizeof(buf));
        }
        if (socketSelect(-1, 5)) {
            socketProcess();
        }
        if (clientDone && wp->ssl && SSL_is_init_finished(wp->ssl)) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    Webs    *wp;
    SSL_CTX *clientCtx;
    SSL     *client;
    int     fd;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websSetSslCertFile("../pki/test.crt");
    websSetSslKeyFile("../pki/test.key");
    teq(sslOpen(), 0, "open TLS");
    teq(socketOpen(), 0, "open the socket layer");
    clientCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(clientCtx, SSL_VERIFY_NONE, NULL);

    wp = connectPair(clientCtx, &client, &fd);
    ttrue(handshake(wp, client),
          "a handshake whose output does not fit the socket buffer completes once the peer reads");
    ttrue(wp->ssl && SSL_is_init_finished(wp->ssl), "the server end finished its handshake");

    sslFree(wp);
    SSL_free(client);
    close(fd);
    SSL_CTX_free(clientCtx);
    sslClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL on a POSIX host is required");
    return 0;
}
#endif
