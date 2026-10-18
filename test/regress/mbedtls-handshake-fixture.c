/*
    mbedtls-handshake-fixture.c - Real handshakes against the server configuration the MbedTLS backend builds
 */
#include "goahead.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

static WebsSocket testSocket;
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
#define socketPtr checkedSocket
#include "../../src/mbedtls.c"
#undef socketPtr

#include "mbedtls-pipe-fixture.h"

/*
    Issue a self-signed certificate for the key, valid only on 1 January 2020
 */
static int makeExpired(mbedtls_x509_crt *cert, mbedtls_pk_context *key)
{
    mbedtls_x509write_cert crt;
    uchar                  serial[] = { 1 };
    uchar                  pem[4096];
    int                    rc;

    mbedtls_x509write_crt_init(&crt);
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, key);
    mbedtls_x509write_crt_set_issuer_key(&crt, key);
    mbedtls_x509write_crt_set_subject_name(&crt, "CN=localhost");
    mbedtls_x509write_crt_set_issuer_name(&crt, "CN=localhost");
    mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial));
    mbedtls_x509write_crt_set_validity(&crt, "20200101000000", "20200102000000");
    rc = mbedtls_x509write_crt_pem(&crt, pem, sizeof(pem), mbedtls_ctr_drbg_random, &cfg.ctr);
    mbedtls_x509write_crt_free(&crt);
    if (rc == 0) {
        rc = mbedtls_x509_crt_parse(cert, pem, strlen((char*) pem) + 1);
    }
    return rc;
}

static cchar *handshake(cchar *certs, cchar *name)
{
    mbedtls_ssl_config  conf;
    mbedtls_ssl_context client;
    mbedtls_x509_crt    cert;
    mbedtls_pk_context  key;
    MbedSocket          mb;
    Webs                wp;
    Pipe                clientPipe = { &toClient, &toServer }, serverPipe = { &toServer, &toClient };
    char                path[1024];
    int                 i, rc;

    toServer.len = toClient.len = 0;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &cfg.ctr);
    mbedtls_x509_crt_init(&cert);
    mbedtls_pk_init(&key);
    if (smatch(name, "expired")) {
        fmt(path, sizeof(path), "%s/ec.key", certs);
        if (mbedtls_pk_parse_keyfile(&key, path, NULL, mbedtls_ctr_drbg_random, &cfg.ctr) != 0) {
            return "unreadable key";
        }
        if (makeExpired(&cert, &key) != 0) {
            return "cannot issue certificate";
        }
        mbedtls_ssl_conf_own_cert(&conf, &cert, &key);
    } else if (!smatch(name, "none")) {
        fmt(path, sizeof(path), "%s/%s.crt", certs, name);
        if (mbedtls_x509_crt_parse_file(&cert, path) != 0) {
            return "unreadable certificate";
        }
        fmt(path, sizeof(path), "%s/%s.key", certs, name);
        if (mbedtls_pk_parse_keyfile(&key, path, NULL, mbedtls_ctr_drbg_random, &cfg.ctr) != 0) {
            return "unreadable key";
        }
        mbedtls_ssl_conf_own_cert(&conf, &cert, &key);
    }
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
    mbedtls_x509_crt_free(&cert);
    mbedtls_pk_free(&key);
    return rc > 0 ? "accepted" : rc < 0 ? "refused" : "stalled";
}

int main(int argc, char **argv)
{
    cchar *clients[] = { "none", "self", "test", "ec", "expired" };
    char  path[1024];
    int   i;

    if (argc < 2) {
        fprintf(stderr, "usage: mbedtls-handshake CERTS_DIR\n");
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
    printf("certificate %s\n", sslHasCertificate() ? "installed" : "missing");
    for (i = 0; i < (int) (sizeof(clients) / sizeof(clients[0])); i++) {
        printf("%s %s\n", clients[i], handshake(argv[1], clients[i]));
    }
    sslClose();
    return 0;
}
