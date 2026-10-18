/*
    mbedtls-crl-coverage-fixture.c - A client chain handshake against the CRL and authority files given at build time
 */
#include "goahead.h"
#include "mbedtls/ssl.h"

static WebsSocket testSocket;
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
#define socketPtr checkedSocket
#include "../../src/mbedtls.c"
#undef socketPtr

#include "mbedtls-pipe-fixture.h"


static cchar *handshake(cchar *chainFile, cchar *keyFile)
{
    mbedtls_ssl_config  conf;
    mbedtls_ssl_context client;
    mbedtls_x509_crt    chain;
    mbedtls_pk_context  key;
    MbedSocket          mb;
    Webs                wp;
    Pipe                clientPipe = { &toClient, &toServer }, serverPipe = { &toServer, &toClient };
    int                 i, rc;

    toServer.len = toClient.len = 0;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &cfg.ctr);
    mbedtls_x509_crt_init(&chain);
    mbedtls_pk_init(&key);
    if (mbedtls_x509_crt_parse_file(&chain, chainFile) != 0) {
        return "unreadable chain";
    }
    if (mbedtls_pk_parse_keyfile(&key, keyFile, NULL, mbedtls_ctr_drbg_random, &cfg.ctr) != 0) {
        return "unreadable key";
    }
    mbedtls_ssl_conf_own_cert(&conf, &chain, &key);
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
    mbedtls_x509_crt_free(&chain);
    mbedtls_pk_free(&key);
    return rc > 0 ? "accepted" : rc < 0 ? "refused" : "stalled";
}


int main(int argc, char **argv)
{
    char path[1024];

    if (argc < 4) {
        fprintf(stderr, "usage: mbedtls-crl-coverage CERTS_DIR CLIENT_CHAIN CLIENT_KEY\n");
        return 1;
    }
    fmt(path, sizeof(path), "%s/self.crt", argv[1]);
    websSetSslCertFile(path);
    fmt(path, sizeof(path), "%s/self.key", argv[1]);
    websSetSslKeyFile(path);
    if (sslOpen() < 0) {
        fprintf(stderr, "sslOpen failed\n");
        return 1;
    }
    printf("client chain %s\n", handshake(argv[2], argv[3]));
    sslClose();
    return 0;
}
