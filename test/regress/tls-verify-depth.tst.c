/*
    tls-verify-depth.tst.c - A client certificate chain longer than ME_GOAHEAD_SSL_VERIFY_DEPTH is refused
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

static int callback(X509 *cert, int error, int depth)
{
    X509_STORE     *store;
    X509_STORE_CTX *ctx;
    int            rc;

    store = X509_STORE_new();
    ctx = X509_STORE_CTX_new();
    X509_STORE_CTX_init(ctx, store, cert, NULL);
    X509_STORE_CTX_set_current_cert(ctx, cert);
    X509_STORE_CTX_set_error(ctx, error);
    X509_STORE_CTX_set_error_depth(ctx, depth);
    rc = verifyClientCertificate(error == X509_V_OK, ctx);
    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);
    return rc;
}


static void suiteA_callback(X509 *cert)
{
    int errors[] = {
        X509_V_OK,
        X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY,
        X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT,
        X509_V_ERR_CERT_UNTRUSTED,
        X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN,
        X509_V_ERR_CERT_CHAIN_TOO_LONG,
        -1
    };
    int i;

    tinfo("\n--- Suite A: the callback refuses a depth past the limit, whatever the error ---\n");

    for (i = 0; errors[i] >= 0; i++) {
        ttrue(callback(cert, errors[i], ME_GOAHEAD_SSL_VERIFY_DEPTH + 1) == 0,
              "depth %d with error %d (%s) must be refused", ME_GOAHEAD_SSL_VERIFY_DEPTH + 1, errors[i],
              X509_verify_cert_error_string(errors[i]));
    }

    ttrue(callback(cert, X509_V_OK, ME_GOAHEAD_SSL_VERIFY_DEPTH) == 1, "a verified certificate at the limit is accepted");
    ttrue(callback(cert, X509_V_OK, 0) == 1, "a verified leaf is accepted");
#if !ME_GOAHEAD_SSL_VERIFY_ISSUER
    ttrue(callback(cert, X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY, 1) == 1,
          "with issuer verification off, an unknown issuer within the limit is accepted");
#endif
}


/*
    Use the handshake's trust store, callback, and depth limit.
 */
static int verifyChain(X509 **certs, int count)
{
    X509_STORE     *store;
    X509_STORE_CTX *ctx;
    STACK_OF(X509) *untrusted;
    int            i, rc;

    store = X509_STORE_new();
    X509_STORE_add_cert(store, certs[0]);
    untrusted = sk_X509_new_null();
    for (i = 1; i <= count; i++) {
        sk_X509_push(untrusted, certs[i]);
    }
    ctx = X509_STORE_CTX_new();
    X509_STORE_CTX_init(ctx, store, certs[count + 1], untrusted);
    X509_STORE_CTX_set_verify_cb(ctx, verifyClientCertificate);
    X509_VERIFY_PARAM_set_depth(X509_STORE_CTX_get0_param(ctx), ME_GOAHEAD_SSL_VERIFY_DEPTH);
    rc = X509_verify_cert(ctx);
    X509_STORE_CTX_free(ctx);
    sk_X509_free(untrusted);
    X509_STORE_free(store);
    return rc;
}


static void freeChain(X509 **certs, EVP_PKEY **keys, int count)
{
    int i;

    for (i = 0; i <= count + 1; i++) {
        X509_free(certs[i]);
        EVP_PKEY_free(keys[i]);
    }
}


static void suiteB_verification(void)
{
    X509     *certs[ME_GOAHEAD_SSL_VERIFY_DEPTH + 4];
    EVP_PKEY *keys[ME_GOAHEAD_SSL_VERIFY_DEPTH + 4];

    tinfo("\n--- Suite B: a real chain past the limit fails verification ---\n");

    makeChain(1, certs, keys);
    ttrue(verifyChain(certs, 1) == 1, "a chain within the depth limit must verify");
    freeChain(certs, keys, 1);

    makeChain(ME_GOAHEAD_SSL_VERIFY_DEPTH + 1, certs, keys);
    ttrue(verifyChain(certs, ME_GOAHEAD_SSL_VERIFY_DEPTH + 1) != 1, "a chain past the depth limit must not verify");
    freeChain(certs, keys, ME_GOAHEAD_SSL_VERIFY_DEPTH + 1);
}


int main(int argc, char **argv)
{
    X509     *certs[3];
    EVP_PKEY *keys[3];

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    makeChain(0, certs, keys);
    suiteA_callback(certs[1]);
    freeChain(certs, keys, 0);

    suiteB_verification();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL 3 backend only");
    return 0;
}
#endif
