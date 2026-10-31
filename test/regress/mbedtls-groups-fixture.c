/*
    MbedTLS accepts elliptic curve key exchange groups and refuses finite-field groups.
 */
#include "goahead.h"
#include "mbedtls/ssl.h"

static WebsSocket testSocket;
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
#define socketPtr checkedSocket
#include "../../src/mbedtls.c"
#undef socketPtr

#include "mbedtls-pipe-fixture.h"

typedef struct Group {
    cchar    *name;
    uint16_t id;
} Group;

static Group groups[] = {
    { "x25519", MBEDTLS_SSL_IANA_TLS_GROUP_X25519 },
    { "secp256r1", MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1 },
    { "secp384r1", MBEDTLS_SSL_IANA_TLS_GROUP_SECP384R1 },
    { "ffdhe2048", MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE2048 },
    { "ffdhe8192", MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE8192 },
    { 0, 0 },
};

/*
    A TLS 1.3 client offering only the named group against the server configuration
 */
static cchar *handshake(Group *group)
{
    mbedtls_ssl_config  conf;
    mbedtls_ssl_context client;
    MbedSocket          mb;
    Webs                wp;
    Pipe                clientPipe = { &toClient, &toServer }, serverPipe = { &toServer, &toClient };
    uint16_t            offered[2];
    int                 i, rc;

    offered[0] = group->id;
    offered[1] = MBEDTLS_SSL_IANA_TLS_GROUP_NONE;
    toServer.len = toClient.len = 0;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &cfg.ctr);
    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_groups(&conf, offered);
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
    Group *group;
    char  path[1024];

    if (argc < 2) {
        fprintf(stderr, "usage: mbedtls-groups CERTS_DIR\n");
        return 1;
    }
    fmt(path, sizeof(path), "%s/test.crt", argv[1]);
    websSetSslCertFile(path);
    fmt(path, sizeof(path), "%s/test.key", argv[1]);
    websSetSslKeyFile(path);
    psa_crypto_init();

    if (sslOpen() < 0) {
        printf("sslOpen failed\n");
        sslClose();
        return 0;
    }
    printf("sslOpen ok\n");
    for (group = groups; group->name; group++) {
        printf("%s %s\n", group->name, handshake(group));
        fflush(stdout);
    }
    sslClose();
    return 0;
}
