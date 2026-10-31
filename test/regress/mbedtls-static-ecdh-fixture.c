/*
    mbedtls-static-ecdh-fixture.c - The MbedTLS cipher floor must drop static ECDH suites, as OpenSSL cannot offer them

    A static ECDH suite (TLS-ECDH-ECDSA-*, TLS-ECDH-RSA-*) uses the server certificate's key for the key exchange, so
    it has no forward secrecy. OpenSSL removed these suites in 1.1.0, so an OpenSSL build can never negotiate one;
    the MbedTLS build must refuse them for the same configuration.
 */
#include "goahead.h"
#include "mbedtls/ssl.h"

static WebsSocket testSocket;
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
#define socketPtr checkedSocket
#include "../../src/mbedtls.c"
#undef socketPtr

#include "mbedtls-pipe-fixture.h"

#define STATIC_ECDH "TLS-ECDH-ECDSA-WITH-AES-128-GCM-SHA256"
#define ECDHE       "TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256"

static int count(int *list)
{
    int n;

    for (n = 0; list && list[n]; n++) { }
    return n;
}


static bool has(int *list, cchar *name)
{
    int code, i;

    if ((code = mbedtls_ssl_get_ciphersuite_id(name)) <= 0) {
        return 0;
    }
    for (i = 0; list && list[i]; i++) {
        if (list[i] == code) {
            return 1;
        }
    }
    return 0;
}


/*
    A TLS 1.2 client offering only the named suite against the server configuration
 */
static cchar *handshake(cchar *suiteName)
{
    mbedtls_ssl_config  conf;
    mbedtls_ssl_context client;
    MbedSocket          mb;
    Webs                wp;
    Pipe                clientPipe = { &toClient, &toServer }, serverPipe = { &toServer, &toClient };
    int                 offered[2], i, rc;

    if ((offered[0] = mbedtls_ssl_get_ciphersuite_id(suiteName)) <= 0) {
        return "unavailable";
    }
    offered[1] = 0;
    toServer.len = toClient.len = 0;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &cfg.ctr);
    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_ciphersuites(&conf, offered);
    mbedtls_ssl_init(&client);
    mbedtls_ssl_setup(&client, &conf);
    mbedtls_ssl_set_bio(&client, &clientPipe, pipeSend, pipeRecv, NULL);

    memset(&wp, 0, sizeof(wp));
    memset(&mb, 0, sizeof(mb));
    wp.ssl = &mb;
    mbedtls_ssl_init(&mb.ctx);
    mbedtls_ssl_setup(&mb.ctx, &cfg.conf);
    mbedtls_ssl_set_bio(&mb.ctx, &serverPipe, pipeSend, pipeRecv, NULL);

    for (i = 0, rc = 0; i < 100 && rc == 0; i++) {
        mbedtls_ssl_handshake(&client);
        rc = mbedHandshake(&wp);
    }
    mbedtls_ssl_free(&mb.ctx);
    mbedtls_ssl_free(&client);
    mbedtls_ssl_config_free(&conf);
    return rc > 0 ? "accepted" : rc < 0 ? "refused" : "stalled";
}


int main(int argc, char **argv)
{
    char path[1024];
    int  *list;

    if (argc < 2) {
        fprintf(stderr, "usage: mbedtls-static-ecdh CERTS_DIR\n");
        return 1;
    }
    //  An EC certificate, which a static ECDH suite can use for its key exchange
    fmt(path, sizeof(path), "%s/ec.crt", argv[1]);
    websSetSslCertFile(path);
    fmt(path, sizeof(path), "%s/ec.key", argv[1]);
    websSetSslKeyFile(path);
    psa_crypto_init();

    list = getCipherSuite("TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:"
                          "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256");
    printf("listed %d static-ecdh %s\n", count(list), has(list, STATIC_ECDH) ? "kept" : "dropped");
    wfree(list);

    if (sslOpen() < 0) {
        printf("sslOpen failed\n");
        sslClose();
        return 0;
    }
    printf("sslOpen ok\n");
    printf("static ecdh %s\n", handshake(STATIC_ECDH));
    printf("ecdhe %s\n", handshake(ECDHE));
    fflush(stdout);
    sslClose();
    return 0;
}
