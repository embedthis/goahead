/* Writes, the buffered-write flag and key parsing in the MbedTLS backend */
#include "goahead.h"
#include "mbedtls/ssl.h"

static WebsSocket testSocket;
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
#define socketPtr checkedSocket
#include "../../src/mbedtls.c"
#undef socketPtr

#include "mbedtls-pipe-fixture.h"

#define BIG 20000

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
static ssize drain(void)
{
    uchar  buf[4096];
    ssize  total;
    int    rc;

    total = 0;
    while ((rc = mbedtls_ssl_read(&client, buf, sizeof(buf))) > 0) {
        total += rc;
    }
    return total;
}

static void checkPartialWrite(void)
{
    char  *big;
    ssize rc, more, delivered;

    big = walloc(BIG);
    memset(big, 'z', BIG);
    toClient.len = 0;
    //  Room for one full record and a little more
    sendLimit = MBEDTLS_SSL_OUT_CONTENT_LEN + 256;
    rc = sslWrite(&wp, big, BIG);
    printf("partial %zd\n", rc);

    sendLimit = sizeof(toClient.data);
    delivered = drain();
    if (rc > 0 && rc < BIG) {
        more = sslWrite(&wp, big + rc, BIG - rc);
        if (more > 0) {
            delivered += drain();
        }
    }
    printf("delivered %zd\n", delivered);
    wfree(big);
}

static void checkBufferedWriteFlag(void)
{
    char buf[64];

    //  Leave part of a client record in the server's input
    mbedtls_ssl_write(&client, (cuchar*) "hello", 5);
    if (toServer.len > 8) {
        toServer.len = 8;
    }
    sslRead(&wp, buf, sizeof(buf));
    testSocket.flags &= ~SOCKET_BUFFERED_WRITE;
    sslWrite(&wp, "x", 1);
    printf("buffered-write %d\n", (testSocket.flags & SOCKET_BUFFERED_WRITE) ? 1 : 0);
}

static int serverReads;

static int countingRecv(void *ctx, uchar *buf, size_t len)
{
    serverReads++;
    return pipeRecv(ctx, buf, len);
}


static void checkWriteHandshaking(void)
{
    MbedSocket pendingSocket;
    Webs       pending;
    ssize      rc;

    memset(&pendingSocket, 0, sizeof(pendingSocket));
    memset(&pending, 0, sizeof(pending));
    pending.ssl = &pendingSocket;
    mbedtls_ssl_init(&pendingSocket.ctx);
    mbedtls_ssl_setup(&pendingSocket.ctx, &cfg.conf);
    mbedtls_ssl_set_bio(&pendingSocket.ctx, &serverPipe, pipeSend, countingRecv, NULL);
    toServer.len = 0;
    testSocket.flags = SOCKET_HANDSHAKING;
    serverReads = 0;
    rc = sslWrite(&pending, "x", 1);
    printf("handshaking write %zd eof %d reads %d\n", rc, (testSocket.flags & SOCKET_EOF) ? 1 : 0, serverReads);
    testSocket.flags = 0;
    mbedtls_ssl_free(&pendingSocket.ctx);
}

static void checkKeyWithoutPublicPoint(char *path)
{
    mbedtls_pk_context key;

    mbedtls_pk_init(&key);
    printf("nopublic %s\n", parseKey(&key, path) == 0 ? "parsed" : "refused");
    mbedtls_pk_free(&key);
}

int main(int argc, char **argv)
{
    char path[1024];

    if (argc < 3) {
        fprintf(stderr, "usage: mbedtls-io CERTS_DIR KEY_WITHOUT_PUBLIC_POINT\n");
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
    checkPartialWrite();
    checkBufferedWriteFlag();
    checkWriteHandshaking();
    fflush(stdout);
    checkKeyWithoutPublicPoint(argv[2]);

    mbedtls_ssl_free(&mb.ctx);
    mbedtls_ssl_free(&client);
    mbedtls_ssl_config_free(&clientConf);
    sslClose();
    return 0;
}
