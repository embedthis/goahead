/*
    tls-config-loading.tst.c - The OpenSSL backend applies the configured ciphers, chain and key
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

#define CERTS "../pki/"

static char logged[1024];

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
    ssize len;

    len = slen(logged);
    if (msg && len < (ssize) sizeof(logged) - 1) {
        fmt(&logged[len], (ssize) sizeof(logged) - len, "%s\n", msg);
    }
}
#endif

static bool hasCipher(cchar *name)
{
    STACK_OF(SSL_CIPHER) *ciphers;
    int                  i;

    ciphers = SSL_CTX_get_ciphers(sslctx);
    for (i = 0; ciphers && i < sk_SSL_CIPHER_num(ciphers); i++) {
        if (smatch(SSL_CIPHER_get_name(sk_SSL_CIPHER_value(ciphers, i)), name)) {
            return 1;
        }
    }
    return 0;
}


static int openWith(cchar *cert, cchar *key)
{
    sslClose();
    websSetSslCertFile(cert);
    websSetSslKeyFile(key);
    return sslOpen();
}


static void suiteA_ciphers(void)
{
    tinfo("\n--- Suite A: the configured ciphers are applied to every TLS version ---\n");

    teq(openWith(CERTS "self.crt", CERTS "self.key"), 0, "open with the default cipher list");
    ttrue(hasCipher("TLS_AES_128_GCM_SHA256"), "the TLS 1.3 suites must be configured");
    ttrue(hasCipher("ECDHE-RSA-AES128-GCM-SHA256"), "the configured TLS 1.2 ciphers must be configured");
    ttrue(!hasCipher("AES128-SHA"), "static RSA, outside the configured list, must not be enabled");
    ttrue(!hasCipher("ECDHE-RSA-AES128-SHA"), "SHA-1 CBC, outside the configured list, must not be enabled");

    teq(setCiphers("TLS_AES_128_GCM_SHA256"), 0, "a TLS 1.3 only list is accepted");
    ttrue(!hasCipher("AES128-SHA"), "OpenSSL's earlier-version defaults must still carry the floor");

    ttrue(setCiphers("AES128-SHA") < 0, "a list of only floor-excluded ciphers must be refused");
}


static void suiteB_chain(void)
{
    STACK_OF(X509) *chain;
    char           *leaf, *ca, *combined;
    cchar          *path = "tls-config-chain.tmp";
    FILE           *fp;

    tinfo("\n--- Suite B: intermediate certificates in the certificate file are sent ---\n");

    leaf = websReadWholeFile(CERTS "test.crt");
    ca = websReadWholeFile(CERTS "ca.crt");
    ttrue(leaf && ca, "read the chain fixtures");
    combined = sfmt("%s%s", leaf ? leaf : "", ca ? ca : "");
    if ((fp = fopen(path, "w")) != 0) {
        fputs(combined, fp);
        fclose(fp);
    }
    teq(openWith(path, CERTS "test.key"), 0, "open with a leaf and CA chain file");
    chain = 0;
    SSL_CTX_get_extra_chain_certs(sslctx, &chain);
    teq(chain ? sk_X509_num(chain) : 0, 1, "the CA certificate after the leaf must be in the chain");
    unlink(path);
    wfree(combined);
    wfree(leaf);
    wfree(ca);
}


static void suiteC_ecKey(void)
{
    tinfo("\n--- Suite C: an EC key can be used ---\n");

    teq(openWith(CERTS "ec.crt", CERTS "ec.key"), 0, "open with an EC certificate and key");
}


static void suiteD_encryptedKey(void)
{
    EVP_PKEY *key;
    BIO      *bio;
    cchar    *path = "tls-config-encrypted.tmp";

    tinfo("\n--- Suite D: an encrypted key fails without prompting for a passphrase ---\n");

    key = 0;
    if ((bio = BIO_new_file(CERTS "test.key", "r")) != 0) {
        key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
        BIO_free(bio);
    }
    ttrue(key != 0, "read the key fixture");
    if ((bio = BIO_new_file(path, "w")) != 0) {
        PEM_write_bio_PrivateKey(bio, key, EVP_aes_256_cbc(), (uchar*) "secret", 6, NULL, NULL);
        BIO_free(bio);
    }
    EVP_PKEY_free(key);

    sslClose();
    sslctx = SSL_CTX_new(TLS_server_method());
    logged[0] = '\0';
    teq(sslSetKeyFile((char*) path), -1, "an encrypted key is refused");
#if ME_GOAHEAD_LOGGING
    ttrue(strstr(logged, "is encrypted") != 0, "the error says the key is encrypted: %s", logged);
#endif
    unlink(path);
}


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    suiteA_ciphers();
    suiteB_chain();
    suiteC_ecKey();
    suiteD_encryptedKey();
    sslClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL backend disabled");
    return 0;
}
#endif
