/*
    tls-crl-chain.tst.c - Revoking an intermediate CA revokes the certificates it issued
 */

#define ME_GOAHEAD_SSL_REVOKE "tls-crl-chain.crl"

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

/*
    A CRL from issuer, revoking the certificate with the given serial, or nothing if it is zero.
 */
static X509_CRL *makeCrl(X509 *issuer, EVP_PKEY *issuerKey, long serial)
{
    X509_CRL     *crl;
    X509_REVOKED *revoked;
    ASN1_INTEGER *number;
    ASN1_TIME    *when;

    crl = X509_CRL_new();
    X509_CRL_set_version(crl, 1);
    X509_CRL_set_issuer_name(crl, X509_get_subject_name(issuer));
    when = ASN1_TIME_new();
    X509_gmtime_adj(when, -60);
    X509_CRL_set1_lastUpdate(crl, when);
    if (serial) {
        revoked = X509_REVOKED_new();
        number = ASN1_INTEGER_new();
        ASN1_INTEGER_set(number, serial);
        X509_REVOKED_set_serialNumber(revoked, number);
        X509_REVOKED_set_revocationDate(revoked, when);
        X509_CRL_add0_revoked(crl, revoked);
        ASN1_INTEGER_free(number);
    }
    X509_gmtime_adj(when, 3600);
    X509_CRL_set1_nextUpdate(crl, when);
    ASN1_TIME_free(when);
    X509_CRL_sort(crl);
    X509_CRL_sign(crl, issuerKey, EVP_sha256());
    return crl;
}


/*
    Verify leaf through intermediate against store. Returns the verification error, X509_V_OK if none.
 */
static int verifyChain(X509_STORE *store, X509 *leaf, X509 *intermediate)
{
    X509_STORE_CTX *ctx;
    STACK_OF(X509) *untrusted;
    int            rc;

    ctx = X509_STORE_CTX_new();
    untrusted = sk_X509_new_null();
    sk_X509_push(untrusted, intermediate);
    X509_STORE_CTX_init(ctx, store, leaf, untrusted);
    rc = X509_verify_cert(ctx) == 1 ? X509_V_OK : X509_STORE_CTX_get_error(ctx);
    X509_STORE_CTX_free(ctx);
    sk_X509_free(untrusted);
    return rc;
}


int main(int argc, char **argv)
{
    EVP_PKEY   *rootKey, *midKey, *leafKey;
    X509       *root, *mid, *leaf;
    X509_CRL   *rootCrl, *midCrl;
    X509_STORE *plain;
    BIO        *bio;
    int        rc;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    rootKey = EVP_EC_gen("P-256");
    midKey = EVP_EC_gen("P-256");
    leafKey = EVP_EC_gen("P-256");
    root = makeCert("Test Root", rootKey, NULL, NULL, 1, 1);
    mid = makeCert("Test Intermediate", midKey, root, rootKey, 2, 1);
    leaf = makeCert("Test Client", leafKey, mid, midKey, 3, 0);

    rootCrl = makeCrl(root, rootKey, 2);
    midCrl = makeCrl(mid, midKey, 0);
    //  A BIO, not a FILE: OpenSSL on Windows cannot use a FILE from another C runtime
    if ((bio = BIO_new_file(ME_GOAHEAD_SSL_REVOKE, "w")) == 0) {
        tfail("cannot write the CRL file");
        return 1;
    }
    PEM_write_bio_X509_CRL(bio, rootCrl);
    PEM_write_bio_X509_CRL(bio, midCrl);
    BIO_free(bio);

    plain = X509_STORE_new();
    X509_STORE_add_cert(plain, root);
    teq(verifyChain(plain, leaf, mid), X509_V_OK, "the chain must verify without revocation checks");
    X509_STORE_free(plain);

    websSetSslCertFile("../pki/self.crt");
    websSetSslKeyFile("../pki/self.key");
    teq(sslOpen(), 0, "open TLS with the revocation list");
    X509_STORE_add_cert(SSL_CTX_get_cert_store(sslctx), root);
    rc = verifyChain(SSL_CTX_get_cert_store(sslctx), leaf, mid);
    teq(rc, X509_V_ERR_CERT_REVOKED, "a client certificate under a revoked intermediate must be refused as revoked");
    sslClose();

    unlink(ME_GOAHEAD_SSL_REVOKE);
    X509_CRL_free(rootCrl);
    X509_CRL_free(midCrl);
    X509_free(leaf);
    X509_free(mid);
    X509_free(root);
    EVP_PKEY_free(leafKey);
    EVP_PKEY_free(midKey);
    EVP_PKEY_free(rootKey);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL 3 backend only");
    return 0;
}
#endif
