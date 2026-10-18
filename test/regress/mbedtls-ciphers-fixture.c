/*
    mbedtls-ciphers-fixture.c - The cipher list the MbedTLS backend builds, and real handshakes against it
 */
#include "goahead.h"
#include "mbedtls/ssl.h"

static WebsSocket testSocket;
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
#define socketPtr checkedSocket
#include "../../src/mbedtls.c"
#undef socketPtr

#include "mbedtls-pipe-fixture.h"

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "Failed: %s\n", #expr); return 1; } } while (0)

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
    char *repeated, path[1024];
    int  *list, i;

    if (argc < 2) {
        fprintf(stderr, "usage: mbedtls-ciphers CERTS_DIR\n");
        return 1;
    }
    fmt(path, sizeof(path), "%s/self.crt", argv[1]);
    websSetSslCertFile(path);
    fmt(path, sizeof(path), "%s/self.key", argv[1]);
    websSetSslKeyFile(path);
    psa_crypto_init();

    //  A repeated name is used once, however many times it repeats
    repeated = sfmt("TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256");
    for (i = 1; i < 400; i++) {
        char *more = sfmt("%s:TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256", repeated);
        wfree(repeated);
        repeated = more;
    }
    list = getCipherSuite(repeated);
    CHECK(list != NULL && count(list) == 1);
    wfree(list);
    wfree(repeated);
    printf("repeats passed\n");

    //  IANA TLS 1.3 names resolve, and a name below the floor or unsupported is dropped
    list = getCipherSuite("TLS_AES_128_GCM_SHA256, TLS_RSA_WITH_AES_128_CBC_SHA:NOT_A_CIPHER,"
                          "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256");
    CHECK(count(list) == 2);
    CHECK(has(list, "TLS1-3-AES-128-GCM-SHA256") && has(list, "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256"));
    wfree(list);
    CHECK(getCipherSuite("NOT_A_CIPHER") == NULL);
    CHECK(getCipherSuite("TLS_RSA_WITH_AES_128_CBC_SHA") == NULL);
    CHECK(getCipherSuite("TLS_RSA_WITH_AES_128_GCM_SHA256") == NULL);
    CHECK(getCipherSuite("TLS_RSA_WITH_3DES_EDE_CBC_SHA:TLS_RSA_WITH_RC4_128_SHA:TLS_RSA_WITH_NULL_SHA") == NULL);
    CHECK(getCipherSuite(",:") == NULL && getCipherSuite("") == NULL && getCipherSuite(NULL) == NULL);
    printf("filtering passed\n");

    if (sslOpen() < 0) {
        printf("sslOpen failed\n");
        sslClose();
        return 0;
    }
    printf("sslOpen ok\n");
    printf("configured %d\n", count(cfg.ciphers));
    CHECK(cfg.conf.MBEDTLS_PRIVATE(ciphersuite_list) == cfg.ciphers);
    if (!*ME_GOAHEAD_SSL_CIPHERS) {
        CHECK(has(cfg.ciphers, "TLS1-3-AES-256-GCM-SHA384"));
        CHECK(has(cfg.ciphers, "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256"));
        CHECK(has(cfg.ciphers, "TLS-ECDHE-ECDSA-WITH-AES-256-CBC-SHA384"));
        CHECK(!has(cfg.ciphers, "TLS-RSA-WITH-AES-128-CBC-SHA"));
        CHECK(!has(cfg.ciphers, "TLS-ECDHE-RSA-WITH-AES-128-CBC-SHA"));
        printf("default list passed\n");
    }
    printf("static RSA gcm %s\n", handshake("TLS-RSA-WITH-AES-128-GCM-SHA256"));
    printf("static RSA cbc %s\n", handshake("TLS-RSA-WITH-AES-128-CBC-SHA"));
    printf("ecdhe sha1 cbc %s\n", handshake("TLS-ECDHE-RSA-WITH-AES-128-CBC-SHA"));
    printf("ecdhe gcm %s\n", handshake("TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256"));
    sslClose();
    return 0;
}
