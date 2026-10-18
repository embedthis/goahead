/*
    tls-cert-fixture.h - Build certificates and run handshakes in-process for OpenSSL regression tests
 */

#ifndef _h_TLS_CERT_FIXTURE
#define _h_TLS_CERT_FIXTURE 1

#include <openssl/x509v3.h>

static void addExtension(X509 *cert, X509 *issuer, int nid, cchar *value)
{
    X509_EXTENSION *ext;
    X509V3_CTX     ctx;

    X509V3_set_ctx(&ctx, issuer, cert, NULL, NULL, 0);
    if ((ext = X509V3_EXT_conf_nid(NULL, &ctx, nid, value)) != 0) {
        X509_add_ext(cert, ext, -1);
        X509_EXTENSION_free(ext);
    }
}


/*
    A certificate for key, issued by issuer (self-signed when issuer is null).
 */
static X509 *makeCert(cchar *cn, EVP_PKEY *key, X509 *issuer, EVP_PKEY *issuerKey, long serial, bool ca)
{
    X509      *cert;
    X509_NAME *name;

    cert = X509_new();
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), serial);
    X509_gmtime_adj(X509_getm_notBefore(cert), -3600);
    X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
    X509_set_pubkey(cert, key);
    name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (cuchar*) cn, -1, -1, 0);
    X509_set_issuer_name(cert, issuer ? X509_get_subject_name(issuer) : name);
    addExtension(cert, issuer ? issuer : cert, NID_basic_constraints, ca ? "critical,CA:TRUE" : "CA:FALSE");
    if (ca) {
        addExtension(cert, issuer ? issuer : cert, NID_key_usage, "critical,keyCertSign,cRLSign");
    }
    X509_sign(cert, issuerKey ? issuerKey : key, EVP_sha256());
    return cert;
}


/*
    The caller frees the chain: leaf, intermediates, then root.
 */
static void makeChain(int count, X509 **certs, EVP_PKEY **keys)
{
    char name[32];
    int  i;

    keys[0] = EVP_EC_gen("P-256");
    certs[0] = makeCert("Chain Root", keys[0], NULL, NULL, 1, 1);
    for (i = 1; i <= count; i++) {
        keys[i] = EVP_EC_gen("P-256");
        snprintf(name, sizeof(name), "Chain CA %d", i);
        certs[i] = makeCert(name, keys[i], certs[i - 1], keys[i - 1], i + 1, 1);
    }
    keys[count + 1] = EVP_EC_gen("P-256");
    certs[count + 1] = makeCert("Chain Leaf", keys[count + 1], certs[count], keys[count], count + 2, 0);
}

/*
    Connect a client to a new server connection over a BIO pair and complete the handshake, then move
    one byte each way so the client receives any session ticket. Returns whether the server resumed a
    session, or -1 if the handshake failed.
 */
static int handshake(SSL_CTX *clientCtx, SSL_SESSION *session, SSL_SESSION **issued)
{
    SSL  *client, *server;
    BIO  *clientBio, *serverBio;
    char byte;
    int  i, rc, done;

    client = SSL_new(clientCtx);
    server = SSL_new(sslctx);
    BIO_new_bio_pair(&clientBio, 0, &serverBio, 0);
    SSL_set_bio(client, clientBio, clientBio);
    SSL_set_bio(server, serverBio, serverBio);
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);
    if (session) {
        SSL_set_session(client, session);
    }
    done = 0;
    for (i = 0; i < 20 && done != 3; i++) {
        if (!(done & 1) && SSL_do_handshake(client) == 1) {
            done |= 1;
        }
        if (!(done & 2) && SSL_do_handshake(server) == 1) {
            done |= 2;
        }
    }
    rc = -1;
    if (done == 3) {
        SSL_write(server, "s", 1);
        SSL_read(client, &byte, 1);
        SSL_write(client, "c", 1);
        SSL_read(server, &byte, 1);
        rc = SSL_session_reused(server);
        if (issued) {
            *issued = SSL_get1_session(client);
        }
    }
    // Shut down before freeing so the session remains resumable.
    SSL_shutdown(client);
    SSL_shutdown(server);
    SSL_free(client);
    SSL_free(server);
    return rc;
}

#endif /* _h_TLS_CERT_FIXTURE */
