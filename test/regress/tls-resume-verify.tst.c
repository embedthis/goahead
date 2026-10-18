/*
    tls-resume-verify.tst.c - With client certificates required, a TLS session is never resumed
 */

#define ME_GOAHEAD_SSL_VERIFY_PEER 1

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
#endif

#if ME_COM_OPENSSL && OPENSSL_VERSION_NUMBER >= 0x30000000L
#include "tls-cert-fixture.h"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

int main(int argc, char **argv)
{
    X509         *certs[2];
    EVP_PKEY     *keys[2];
    SSL_CTX      *clientCtx;
    SSL_SESSION  *session;
    int          i;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websSetSslCertFile("../pki/self.crt");
    websSetSslKeyFile("../pki/self.key");
    teq(sslOpen(), 0, "open TLS with client certificates required");
    teq(SSL_CTX_get_timeout(sslctx), ME_GOAHEAD_SSL_TIMEOUT, "the configured session lifetime is applied");

    makeChain(0, certs, keys);
    X509_STORE_add_cert(SSL_CTX_get_cert_store(sslctx), certs[0]);
    clientCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_use_certificate(clientCtx, certs[1]);
    SSL_CTX_use_PrivateKey(clientCtx, keys[1]);
    SSL_CTX_set_verify(clientCtx, SSL_VERIFY_NONE, NULL);

    session = 0;
    teq(handshake(clientCtx, NULL, &session), 0, "a client with a trusted certificate completes a full handshake");

    for (i = 0; i < 2; i++) {
        teq(handshake(clientCtx, session, NULL), 0, "a later connection must not resume the session");
    }
    ttrue(session == 0 || !SSL_SESSION_is_resumable(session), "no resumable session is issued");

    SSL_SESSION_free(session);
    session = 0;
    SSL_CTX_set_max_proto_version(clientCtx, TLS1_2_VERSION);
    if (handshake(clientCtx, NULL, &session) == 0) {
        teq(handshake(clientCtx, session, NULL), 0, "a TLS 1.2 connection must not resume the session");
    } else {
        tinfo("TLS 1.2 is disabled; session id resumption not exercised");
    }
    SSL_SESSION_free(session);
    session = 0;
    SSL_CTX_set_max_proto_version(clientCtx, 0);

    SSL_CTX_clear_options(sslctx, SSL_OP_NO_TICKET);
    SSL_CTX_set_num_tickets(sslctx, 2);
    handshake(clientCtx, NULL, &session);
    teq(handshake(clientCtx, session, NULL), 1, "control: with tickets allowed the session is resumed");

    SSL_SESSION_free(session);
    SSL_CTX_free(clientCtx);
    for (i = 0; i < 2; i++) {
        X509_free(certs[i]);
        EVP_PKEY_free(keys[i]);
    }
    sslClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL 3 backend only");
    return 0;
}
#endif
