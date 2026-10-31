/*
    tls-read-stale-error-queue.tst.c - Errors another connection's failed TLS read left in OpenSSL's thread-wide error
    queue must not make a healthy connection's partial record look like a protocol error.
 */
#define ME_GOAHEAD_SSL_NO_V1_2 0
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
#include <fcntl.h>
#include <sys/socket.h>

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    One connection: the server side is a real Webs over a socketpair, read only through sslRead; the client side is an
    OpenSSL client whose records can be delivered whole or in part.
 */
typedef struct Conn {
    Webs       *wp;
    WebsSocket *sp;
    SSL        *client;
    BIO        *clientIn, *clientOut;
    int        fds[2];
} Conn;

static SSL_CTX *clientCtx;

static Conn *openConn(int version)
{
    Conn *c;
    int  sid, wid;

    c = walloc(sizeof(Conn));
    memset(c, 0, sizeof(Conn));
    ttrue(socketpair(AF_UNIX, SOCK_STREAM, 0, c->fds) == 0, "create a socket pair");
    fcntl(c->fds[1], F_SETFL, O_NONBLOCK);
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    c->sp = socketPtr(sid);
    c->sp->sock = c->fds[0];
    socketSetBlock(sid, 0);
    wid = websAlloc(sid);
    c->wp = webs[wid];
    c->wp->flags |= WEBS_SECURE;
    teq(sslUpgrade(c->wp), 0, "upgrade the server side to TLS");
    c->client = SSL_new(clientCtx);
    c->clientIn = BIO_new(BIO_s_mem());
    c->clientOut = BIO_new(BIO_s_mem());
    SSL_set_bio(c->client, c->clientIn, c->clientOut);
    SSL_set_min_proto_version(c->client, version);
    SSL_set_max_proto_version(c->client, version);
    SSL_set_connect_state(c->client);
    return c;
}


static ssize serverRead(Conn *c)
{
    char buf[1024];

    return sslRead(c->wp, buf, sizeof(buf));
}


/*
    Shuttle the client's bytes to the server socket and the server's bytes back, until both sides are established.
 */
static bool handshake(Conn *c)
{
    uchar buf[16384];
    int   i, n, rc;

    for (i = 0; i < 20; i++) {
        rc = SSL_do_handshake(c->client);
        if (rc <= 0 && SSL_get_error(c->client, rc) != SSL_ERROR_WANT_READ) {
            return 0;
        }
        while ((n = BIO_read(c->clientOut, buf, sizeof(buf))) > 0) {
            write(c->fds[1], buf, (size_t) n);
        }
        if (serverRead(c) < 0) {
            return 0;
        }
        while ((n = (int) read(c->fds[1], buf, sizeof(buf))) > 0) {
            BIO_write(c->clientIn, buf, n);
        }
        if (SSL_is_init_finished(c->client) && SSL_is_init_finished(c->wp->ssl)) {
            return 1;
        }
    }
    return 0;
}


/*
    An application record whose body is not valid ciphertext: the server's read fails with a MAC error
 */
static void sendCorruptRecord(Conn *c)
{
    uchar record[5 + 32] = { 0x17, 0x03, 0x03, 0x00, 0x20 };

    memset(&record[5], 0x5a, 32);
    write(c->fds[1], record, sizeof(record));
}


static void closeConn(Conn *c)
{
    SSL_free(c->client);
    c->sp->flags |= SOCKET_EOF;
    websFree(c->wp);
    close(c->fds[1]);
    wfree(c);
}


int main(int argc, char **argv)
{
    Conn  *victim, *attacker[3];
    uchar record[4096];
    char  body[100];
    ssize rc;
    int   i, n;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websSetSslCertFile("../pki/self.crt");
    websSetSslKeyFile("../pki/self.key");
    teq(sslOpen(), 0, "open TLS with TLS 1.2 enabled");
    teq(socketOpen(), 0, "open the socket layer");
    clientCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(clientCtx, SSL_VERIFY_NONE, NULL);

    victim = openConn(TLS1_3_VERSION);
    ttrue(handshake(victim), "the healthy TLS 1.3 connection is established");

    /*
        Failed reads leave multiple error entries; none may affect the healthy connection.
     */
    for (i = 0; i < 3; i++) {
        attacker[i] = openConn(TLS1_2_VERSION);
        ttrue(handshake(attacker[i]), "a TLS 1.2 connection is established");
    }
    for (i = 0; i < 3; i++) {
        sendCorruptRecord(attacker[i]);
        teq(serverRead(attacker[i]), -1, "a corrupt record ends its own connection");
        ttrue((attacker[i]->sp->flags & SOCKET_EOF) != 0, "the failed connection is marked closed");
    }

    /*
        The healthy client sends a 100 byte request; only the first 40 bytes of its record arrive.
     */
    memset(body, 'x', sizeof(body));
    teq(SSL_write(victim->client, body, sizeof(body)), (int) sizeof(body), "the client writes its request");
    n = BIO_read(victim->clientOut, record, sizeof(record));
    ttrue(n > 40, "the request forms one record");
    write(victim->fds[1], record, 40);
    rc = serverRead(victim);
    teq(rc, 0, "a partial record is not yet readable, and is not an error");
    ttrue((victim->sp->flags & SOCKET_EOF) == 0, "a partial record on a healthy connection does not close it");

    write(victim->fds[1], record + 40, (size_t) (n - 40));
    rc = serverRead(victim);
    teq(rc, (ssize) sizeof(body), "the rest of the record delivers the request");

    for (i = 0; i < 3; i++) {
        closeConn(attacker[i]);
    }
    closeConn(victim);
    SSL_CTX_free(clientCtx);
    sslClose();
    socketClose();
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("OpenSSL on POSIX only");
    return 0;
}
#endif
