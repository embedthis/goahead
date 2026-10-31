/*
    mbedtls-write-retry-fixture.c - A write retried after MbedTLS could not send must deliver every byte it reports

    MbedTLS keeps the record it built from a write that returned WANT_WRITE and expects the retry to name the same
    length. The HTTP layer retries from its output buffer, which may have grown in the meantime, so the backend must
    carry the pending length itself.
 */
#include "goahead.h"
#include "mbedtls/ssl.h"

static WebsSocket testSocket;
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
#define socketPtr checkedSocket
#include "../../src/mbedtls.c"
#undef socketPtr

#include "mbedtls-pipe-fixture.h"

#define FIRST  600                  /* Bytes of the first write, which MbedTLS holds in a record it cannot send */
#define SECOND 400                  /* Bytes the application appends before the write is retried */

static mbedtls_ssl_config  clientConf;
static mbedtls_ssl_context client;
static MbedSocket          mb;
static Webs                wp;
static Pipe                clientPipe = { &toClient, &toServer }, serverPipe = { &toServer, &toClient };

static bool connectPair(void)
{
    int i, rc;

    mbedtls_ssl_config_init(&clientConf);
    mbedtls_ssl_config_defaults(&clientConf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&clientConf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&clientConf, mbedtls_ctr_drbg_random, &cfg.ctr);
    mbedtls_ssl_init(&client);
    mbedtls_ssl_setup(&client, &clientConf);
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
    //  Let the client take anything the server sent after the handshake, such as session tickets
    for (i = 0; i < 10; i++) {
        uchar scratch[256];
        mbedtls_ssl_read(&client, scratch, sizeof(scratch));
    }
    return rc > 0;
}

//  Read everything the client has been sent
static ssize drain(uchar *buf, ssize max)
{
    ssize total;
    int   rc;

    total = 0;
    while (total < max && (rc = mbedtls_ssl_read(&client, buf + total, (size_t) (max - total))) > 0) {
        total += rc;
    }
    return total;
}

int main(int argc, char **argv)
{
    uchar data[FIRST + SECOND], got[FIRST + SECOND + 1];
    char  path[1024];
    ssize first, retry, delivered;

    if (argc < 2) {
        fprintf(stderr, "usage: mbedtls-write-retry CERTS_DIR\n");
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
    if (!connectPair()) {
        fprintf(stderr, "handshake failed\n");
        return 1;
    }
    memset(data, 'a', FIRST);
    memset(data + FIRST, 'b', SECOND);
    toClient.len = 0;

    //  The transport takes less than the record, so MbedTLS keeps it and the write reports nothing sent
    sendLimit = 200;
    first = sslWrite(&wp, data, FIRST);
    printf("first %zd\n", first);

    //  The application has appended more output by the time the transport opens up and the write is retried
    sendLimit = sizeof(toClient.data);
    retry = sslWrite(&wp, data, FIRST + SECOND);
    printf("retry %zd\n", retry);

    delivered = drain(got, sizeof(got));
    printf("delivered %zd\n", delivered);
    printf("content %s\n", (delivered == retry && memcmp(got, data, (size_t) delivered) == 0) ? "intact" : "lost");
    fflush(stdout);

    mbedtls_ssl_free(&mb.ctx);
    mbedtls_ssl_free(&client);
    mbedtls_ssl_config_free(&clientConf);
    sslClose();
    return 0;
}
