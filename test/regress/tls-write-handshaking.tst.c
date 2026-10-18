/* A TLS write before the handshake completes fails without driving the handshake or reading from the peer */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if ME_COM_OPENSSL
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/openssl.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int reads;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static long countReads(BIO *bio, int oper, cchar *argp, size_t len, int argi, long argl, int ret, size_t *processed)
{
    if (oper == BIO_CB_READ) {
        reads++;
    }
    return ret;
}


int main(int argc, char **argv)
{
    WebsSocket *sp;
    Webs       wp;
    BIO        *serverBio, *clientBio;
    ssize      rc;
    int        sid;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websSetSslCertFile("../../certs/self.crt");
    websSetSslKeyFile("../../certs/self.key");
    teq(sslOpen(), 0, "open TLS");
    teq(socketOpen(), 0, "open the socket layer");
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    ttrue(sid >= 0, "allocate a socket");
    sp = socketPtr(sid);

    //  A server connection whose client has not yet sent its ClientHello
    memset(&wp, 0, sizeof(wp));
    wp.sid = sid;
    wp.ssl = SSL_new(sslctx);
    BIO_new_bio_pair(&serverBio, 0, &clientBio, 0);
    BIO_set_callback_ex(serverBio, countReads);
    SSL_set_bio(wp.ssl, serverBio, serverBio);
    SSL_set_accept_state(wp.ssl);

    rc = sslWrite(&wp, "x", 1);
    teq(rc, (ssize) -1, "a write before the handshake completes fails");
    ttrue(sp->flags & SOCKET_EOF, "and marks the connection closed");
    teq(reads, 0, "without reading from the peer");

    SSL_free(wp.ssl);
    BIO_free(clientBio);
    socketFree(sid);
    socketClose();
    sslClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL backend disabled");
    return 0;
}
#endif
