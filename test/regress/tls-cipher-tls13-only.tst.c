/*
    tls-cipher-tls13-only.tst.c - A cipher list naming only TLS 1.3 suites offers TLS 1.2 nothing, as the MbedTLS
    backend does, rather than OpenSSL's whole default list.
 */
#define ME_GOAHEAD_SSL_NO_V1_2 0
#define ME_GOAHEAD_SSL_CIPHERS "TLS_AES_256_GCM_SHA384"
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

int main(int argc, char **argv)
{
    STACK_OF(SSL_CIPHER) *ciphers;
    const SSL_CIPHER     *cipher;
    SSL                  *ssl;
    int                  i, legacy;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websSetSslCertFile("../pki/self.crt");
    websSetSslKeyFile("../pki/self.key");
    teq(sslOpen(), 0, "a TLS 1.3-only cipher list is accepted");
    ttrue(sslctx != 0, "the context exists");

    teq(SSL_CTX_get_min_proto_version(sslctx), TLS1_3_VERSION,
        "with no TLS 1.2 suite named, TLS 1.3 is the only protocol offered");

    ssl = SSL_new(sslctx);
    ciphers = SSL_get1_supported_ciphers(ssl);
    legacy = 0;
    for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
        cipher = sk_SSL_CIPHER_value(ciphers, i);
        if (SSL_CIPHER_get_protocol_id(cipher) < 0x1301 || SSL_CIPHER_get_protocol_id(cipher) > 0x1305) {
            legacy++;
        }
    }
    teq(legacy, 0, "no TLS 1.2 suite is usable (OpenSSL's default list is not substituted)");
    sk_SSL_CIPHER_free(ciphers);
    SSL_free(ssl);
    sslClose();
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("OpenSSL only");
    return 0;
}
#endif
