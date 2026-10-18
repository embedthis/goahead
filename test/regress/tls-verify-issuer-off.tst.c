/*
    tls-verify-issuer-off.tst.c - Without ME_GOAHEAD_SSL_VERIFY_ISSUER any client certificate is accepted, and
    websOpen warns that the configured authority is not enforced
 */

#define ME_GOAHEAD_SSL_VERIFY_PEER 1
#define ME_GOAHEAD_SSL_VERIFY_ISSUER 0
#define ME_GOAHEAD_SSL_AUTHORITY "tls-verify-issuer-off.pem"

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

static bool warned;

static void captureLog(int level, cchar *msg)
{
    if (msg && strstr(msg, "ME_GOAHEAD_SSL_VERIFY_ISSUER is off")) {
        warned = 1;
    }
}


/*
    A CA certificate with a subject key identifier, as CA tooling issues them
 */
static X509 *makeRoot(EVP_PKEY *key)
{
    X509 *cert;

    cert = makeCert("Issuer Off Root", key, NULL, NULL, 1, 1);
    addExtension(cert, cert, NID_subject_key_identifier, "hash");
    X509_sign(cert, key, EVP_sha256());
    return cert;
}


/*
    A leaf whose authority key identifier names the key that signed it
 */
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
    X509     *root, *forgerRoot, *client, *forged, *expired;
    EVP_PKEY *rootKey, *forgerKey, *clientKey;
    BIO      *bio;

    logSetHandler(captureLog);
    rootKey = EVP_EC_gen("P-256");
    forgerKey = EVP_EC_gen("P-256");
    clientKey = EVP_EC_gen("P-256");
    root = makeRoot(rootKey);
    ttrue((bio = BIO_new_file(ME_GOAHEAD_SSL_AUTHORITY, "w")) != 0, "write the authority");
    PEM_write_bio_X509(bio, root);
    BIO_free(bio);

    websSetSslCertFile("../../certs/self.crt");
    websSetSslKeyFile("../../certs/self.key");
    teq(websOpen("../web", NULL), 0, "open the server with client verification and an authority");
    ttrue(warned, "websOpen must warn that the authority is not enforced without issuer verification");

    //  The documented contract: any client certificate is accepted, but expired certificates are not
    client = makeLeaf("Client", clientKey, root, rootKey, 2);
    //  The forger's root has the same name as the authority, so the forged leaf names the authority as issuer
    forgerRoot = makeRoot(forgerKey);
    forged = makeLeaf("Forged Client", clientKey, forgerRoot, forgerKey, 3);
    expired = makeLeaf("Expired Client", clientKey, root, rootKey, 4);
    X509_gmtime_adj(X509_getm_notBefore(expired), -7200);
    X509_gmtime_adj(X509_getm_notAfter(expired), -3600);
    X509_sign(expired, rootKey, EVP_sha256());

    ttrue(connectWith(client, clientKey) >= 0, "a client certificate from the authority is accepted");
    ttrue(connectWith(forged, clientKey) >= 0,
          "a client certificate naming the authority but signed by another key is accepted");
    teq(connectWith(expired, clientKey), -1, "an expired client certificate is refused");

    websClose();
    unlink(ME_GOAHEAD_SSL_AUTHORITY);
    X509_free(client);
    X509_free(forged);
    X509_free(expired);
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
    tskip("The OpenSSL backend and logging are required");
    return 0;
}
#endif
