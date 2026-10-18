/* A TLS write that fails outright ends a blocking flush at once */

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

#define PROMPT_SECS 5

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


int main(int argc, char **argv)
{
    WebsSocket *sp;
    Webs       *wp;
    WebsTime   started;
    SSL_CTX    *clientCtx;
    SSL        *client;
    uchar      record[5 + 48];
    char       buf[256];
    ssize      rc;
    int        fds[2], sid, wid;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websSetSslCertFile("../../certs/self.crt");
    websSetSslKeyFile("../../certs/self.key");
    teq(sslOpen(), 0, "open TLS");
    teq(socketOpen(), 0, "open the socket layer");
    ttrue(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "create a socket pair");

    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    sp = socketPtr(sid);
    sp->sock = fds[0];
    socketSetBlock(sid, 0);
    wid = websAlloc(sid);
    wp = webs[wid];
    wp->flags |= WEBS_SECURE;
    teq(sslUpgrade(wp), 0, "start TLS on the server end");

    clientCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(clientCtx, SSL_VERIFY_NONE, NULL);
    client = SSL_new(clientCtx);
    SSL_set_fd(client, fds[1]);
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
    SSL_set_connect_state(client);
    ttrue(handshake(client, wp->ssl), "complete the handshake");

    //  An application-data record whose body is not a valid ciphertext for this connection
    memset(record, 0x5a, sizeof(record));
    record[0] = 0x17;
    record[1] = 0x03;
    record[2] = 0x03;
    record[3] = 0;
    record[4] = 48;
    ttrue(write(fds[1], record, sizeof(record)) == (ssize) sizeof(record), "send a corrupt record");
    rc = sslRead(wp, buf, sizeof(buf));
    ttrue(rc < 0, "the corrupt record fails the connection");

    //  The failed read closed the connection. The write must say so on its own account
    sp->flags &= ~SOCKET_EOF;
    errno = EAGAIN;
    rc = sslWrite(wp, "x", 1);
    ttrue(rc < 0, "a write to the failed connection fails");
    ttrue(errno != 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != ETIMEDOUT,
        "with a real error, not a stale one");
    ttrue(socketEof(sid), "and marks the connection closed");

    bufPutStr(&wp->output, "response body");
    errno = EAGAIN;
    started = time(0);
    ttrue(websFlush(wp, 1) < 0, "a blocking flush to the failed connection fails");
    ttrue(time(0) - started < PROMPT_SECS, "at once, not at its deadline");

    SSL_free(client);
    SSL_CTX_free(clientCtx);
    close(fds[1]);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL on a POSIX host is required");
    return 0;
}
#endif
