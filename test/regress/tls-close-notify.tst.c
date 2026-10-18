/* A healthy TLS connection is closed with a close_notify alert; a failed or unfinished one is closed silently */

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
#include "../../src/openssl.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    Complete a handshake between client and the server's connection, both non-blocking.
 */
static bool handshake(SSL *client, SSL *server)
{
    int i, done;

    done = 0;
    for (i = 0; i < 50 && done != 3; i++) {
        if (!(done & 1) && SSL_do_handshake(client) == 1) {
            done |= 1;
        }
        if (!(done & 2) && SSL_do_handshake(server) == 1) {
            done |= 2;
        }
    }
    return done == 3;
}



static Webs *connectPair(SSL_CTX *clientCtx, SSL **client, int *clientFd)
{
    Webs *wp;
    int  fds[2], sid, wid;

    ttrue(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "create a socket pair");
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    socketPtr(sid)->sock = fds[0];
    socketSetBlock(sid, 0);
    wid = websAlloc(sid);
    wp = webs[wid];
    wp->flags |= WEBS_SECURE;
    teq(sslUpgrade(wp), 0, "start TLS on the server end");

    *client = SSL_new(clientCtx);
    SSL_set_fd(*client, fds[1]);
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
    SSL_set_connect_state(*client);
    *clientFd = fds[1];
    return wp;
}


static void finish(Webs *wp, SSL *client, int clientFd)
{
    wp->ssl = 0;
    SSL_free(client);
    close(clientFd);
}


int main(int argc, char **argv)
{
    Webs    *wp;
    SSL_CTX *clientCtx;
    SSL     *client;
    char    buf[256];
    int     fd, rc;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websSetSslCertFile("../../certs/self.crt");
    websSetSslKeyFile("../../certs/self.key");
    teq(sslOpen(), 0, "open TLS");
    teq(socketOpen(), 0, "open the socket layer");
    clientCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(clientCtx, SSL_VERIFY_NONE, NULL);

    //  A healthy connection: the client sees an orderly end of stream, not a truncation
    wp = connectPair(clientCtx, &client, &fd);
    ttrue(handshake(client, wp->ssl), "complete the handshake");
    sslFree(wp);
    ttrue(wp->ssl == 0, "the server end is released");
    rc = SSL_read(client, buf, sizeof(buf));
    teq(rc, 0, "the client reads end of stream");
    teq(SSL_get_error(client, rc), SSL_ERROR_ZERO_RETURN, "the server sent close_notify");
    finish(wp, client, fd);

    //  A connection that already saw end of file has nothing to say to the peer
    wp = connectPair(clientCtx, &client, &fd);
    ttrue(handshake(client, wp->ssl), "complete the handshake");
    rc = SSL_read(client, buf, sizeof(buf));
    teq(SSL_get_error(client, rc), SSL_ERROR_WANT_READ, "the client has read the session tickets");
    socketPtr(wp->sid)->flags |= SOCKET_EOF;
    sslFree(wp);
    rc = SSL_read(client, buf, sizeof(buf));
    teq(SSL_get_error(client, rc), SSL_ERROR_WANT_READ, "no alert is sent after end of file");
    finish(wp, client, fd);

    //  A connection closed during the handshake sends no alert
    wp = connectPair(clientCtx, &client, &fd);
    rc = SSL_do_handshake(client);
    teq(SSL_get_error(client, rc), SSL_ERROR_WANT_READ, "the client has sent its hello");
    sslFree(wp);
    rc = SSL_do_handshake(client);
    teq(SSL_get_error(client, rc), SSL_ERROR_WANT_READ, "no alert is sent during the handshake");
    finish(wp, client, fd);

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
