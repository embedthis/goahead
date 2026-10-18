/* Client certificates are verified only against the configured authority */

#define ME_GOAHEAD_SSL_VERIFY_PEER 1
#define ME_GOAHEAD_SSL_VERIFY_ISSUER 1

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

#define STORE "tls-client-authority.pem"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static void setEnv(cchar *name, cchar *value)
{
#if ME_WIN_LIKE
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

int main(int argc, char **argv)
{
    X509     *certs[2];
    EVP_PKEY *keys[2];
    SSL_CTX  *clientCtx;
    BIO      *bio;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    makeChain(0, certs, keys);
    ttrue((bio = BIO_new_file(STORE, "w")) != 0, "write the stand-in system store");
    PEM_write_bio_X509(bio, certs[0]);
    BIO_free(bio);
    setEnv("SSL_CERT_FILE", STORE);
    setEnv("SSL_CERT_DIR", ".");

    websSetSslCertFile("../pki/self.crt");
    websSetSslKeyFile("../pki/self.key");
    teq(sslOpen(), 0, "open TLS with client and issuer verification and no authority");

    clientCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_use_certificate(clientCtx, certs[1]);
    SSL_CTX_use_PrivateKey(clientCtx, keys[1]);
    SSL_CTX_set_verify(clientCtx, SSL_VERIFY_NONE, NULL);
    teq(handshake(clientCtx, NULL, NULL), -1, "a client certificate from a CA that was not configured is refused");

    SSL_CTX_free(clientCtx);
    sslClose();
    unlink(STORE);
    X509_free(certs[1]);
    X509_free(certs[0]);
    EVP_PKEY_free(keys[1]);
    EVP_PKEY_free(keys[0]);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL 3 is required");
    return 0;
}
#endif
