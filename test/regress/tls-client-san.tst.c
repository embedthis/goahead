/* A trusted client certificate that names its holder only in subjectAltName, with no CN, is accepted */

#define ME_GOAHEAD_SSL_VERIFY_PEER 1
#define ME_GOAHEAD_SSL_VERIFY_ISSUER 1
#define ME_GOAHEAD_SSL_AUTHORITY "tls-client-san.pem"

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
    A leaf with an empty subject, identified by a critical DNS subjectAltName
 */
static X509 *makeSanLeaf(EVP_PKEY *key, X509 *issuer, EVP_PKEY *issuerKey)
{
    X509      *cert;
    X509_NAME *name;

    cert = makeCert("removed", key, issuer, issuerKey, 3, 0);
    name = X509_get_subject_name(cert);
    while (X509_NAME_entry_count(name) > 0) {
        X509_NAME_ENTRY_free(X509_NAME_delete_entry(name, 0));
    }
    addExtension(cert, issuer, NID_subject_alt_name, "critical,DNS:client.test");
    X509_sign(cert, issuerKey, EVP_sha256());
    return cert;
}


static int connectWith(X509 *cert, EVP_PKEY *key)
{
    SSL_CTX *clientCtx;
    int     rc;

    clientCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_use_certificate(clientCtx, cert);
    SSL_CTX_use_PrivateKey(clientCtx, key);
    SSL_CTX_set_verify(clientCtx, SSL_VERIFY_NONE, NULL);
    rc = handshake(clientCtx, NULL, NULL);
    SSL_CTX_free(clientCtx);
    return rc;
}


int main(int argc, char **argv)
{
    X509     *certs[2], *other[2], *san;
    EVP_PKEY *keys[2], *otherKeys[2], *sanKey;
    BIO      *bio;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    makeChain(0, certs, keys);
    makeChain(0, other, otherKeys);
    ttrue((bio = BIO_new_file(ME_GOAHEAD_SSL_AUTHORITY, "w")) != 0, "write the authority");
    PEM_write_bio_X509(bio, certs[0]);
    BIO_free(bio);

    websSetSslCertFile("../pki/self.crt");
    websSetSslKeyFile("../pki/self.key");
    teq(sslOpen(), 0, "open TLS with client and issuer verification");

    sanKey = EVP_EC_gen("P-256");
    san = makeSanLeaf(sanKey, certs[0], keys[0]);
    ttrue(X509_NAME_entry_count(X509_get_subject_name(san)) == 0, "the leaf has an empty subject");

    ttrue(connectWith(san, sanKey) >= 0, "a SAN-only client certificate from the authority is accepted");
    ttrue(connectWith(certs[1], keys[1]) >= 0, "a client certificate with a CN from the authority is accepted");
    teq(connectWith(other[1], otherKeys[1]), -1, "a client certificate from another CA is refused");

    sslClose();
    unlink(ME_GOAHEAD_SSL_AUTHORITY);
    X509_free(san);
    EVP_PKEY_free(sanKey);
    X509_free(certs[1]);
    X509_free(certs[0]);
    X509_free(other[1]);
    X509_free(other[0]);
    EVP_PKEY_free(keys[1]);
    EVP_PKEY_free(keys[0]);
    EVP_PKEY_free(otherKeys[1]);
    EVP_PKEY_free(otherKeys[0]);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL 3 is required");
    return 0;
}
#endif
