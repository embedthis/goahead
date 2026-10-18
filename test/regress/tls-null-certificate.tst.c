/* An absent current certificate must fail before certificate inspection. */
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
#include <openssl/ssl.h>
static X509_NAME *checkedSubject(const X509 *cert)
{
    tnotnull(cert, "certificate exists before reading subject");
    return X509_get_subject_name(cert);
}
#define X509_get_subject_name checkedSubject
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/openssl.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef X509_get_subject_name
#endif
int main(int argc, char **argv)
{
#if ME_COM_OPENSSL
    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    int rc;
    tnotnull(ctx, "create verification context");
    X509_STORE_CTX_set_error(ctx, X509_V_ERR_UNSPECIFIED);
    rc = verifyClientCertificate(0, ctx);
    teq(rc, 0, "missing certificate fails closed on verification error");
    X509_STORE_CTX_set_error(ctx, X509_V_OK);
    rc = verifyClientCertificate(1, ctx);
    teq(rc, 0, "missing certificate fails closed even without an error code");
    X509_STORE_CTX_free(ctx);
#else
    tskip("OpenSSL backend disabled");
#endif
    return 0;
}
