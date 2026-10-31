/*
    tls-verify-log-level.tst.c - A refused client certificate's subject, text the peer chooses, is traced only at a
    level production logs do not record.
 */
#define ME_GOAHEAD_SSL_VERIFY_PEER 1
#define ME_GOAHEAD_SSL_VERIFY_ISSUER 1
#define ME_GOAHEAD_SSL_AUTHORITY "tls-verify-log-level.pem"
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
#endif

#if ME_COM_OPENSSL && ME_GOAHEAD_LOGGING
#include "tls-cert-fixture.h"

#define MARKER "ZQMARKERZQ"

static int markerLevel = -1;

static void captureLog(int level, cchar *msg)
{
    level &= WEBS_LEVEL_MASK;
    if (msg && strstr(msg, MARKER) && (markerLevel < 0 || level < markerLevel)) {
        markerLevel = level;
    }
}


static X509 *makeRoot(cchar *cn, EVP_PKEY *key)
{
    X509 *cert;

    cert = makeCert(cn, key, NULL, NULL, 1, 1);
    addExtension(cert, cert, NID_subject_key_identifier, "hash");
    X509_sign(cert, key, EVP_sha256());
    return cert;
}


static X509 *makeLeaf(cchar *cn, EVP_PKEY *key, X509 *issuer, EVP_PKEY *issuerKey, long serial)
{
    X509 *cert;

    cert = makeCert(cn, key, issuer, issuerKey, serial, 0);
    addExtension(cert, issuer, NID_authority_key_identifier, "keyid:always");
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
    X509     *root, *forgerRoot, *forged;
    EVP_PKEY *rootKey, *forgerKey, *clientKey;
    BIO      *bio;
    char     name[200];

    logSetHandler(captureLog);
    rootKey = EVP_EC_gen("P-256");
    forgerKey = EVP_EC_gen("P-256");
    clientKey = EVP_EC_gen("P-256");
    root = makeRoot("Log Level Root", rootKey);
    ttrue((bio = BIO_new_file(ME_GOAHEAD_SSL_AUTHORITY, "w")) != 0, "write the authority");
    PEM_write_bio_X509(bio, root);
    BIO_free(bio);

    websSetSslCertFile("../pki/self.crt");
    websSetSslKeyFile("../pki/self.key");
    teq(websOpen("../web", NULL), 0, "open the server with client verification");
    //  Capture every level, so a subject traced at a level production logs record is seen
    websSetLogLevel(5);

    //  A client certificate the authority did not sign, carrying the peer's own text in its subject
    fmt(name, sizeof(name), "%s client %s", MARKER, MARKER);
    forgerRoot = makeRoot("Forger Root", forgerKey);
    forged = makeLeaf(name, clientKey, forgerRoot, forgerKey, 3);
    teq(connectWith(forged, clientKey), -1, "a client certificate from an unknown authority is refused");
    ttrue(markerLevel < 0 || markerLevel >= 4,
          "the refused subject appears only at trace level 4 or above, got level %d", markerLevel);

    websClose();
    unlink(ME_GOAHEAD_SSL_AUTHORITY);
    X509_free(forged);
    X509_free(forgerRoot);
    X509_free(root);
    EVP_PKEY_free(clientKey);
    EVP_PKEY_free(forgerKey);
    EVP_PKEY_free(rootKey);
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("OpenSSL with logging only");
    return 0;
}
#endif
