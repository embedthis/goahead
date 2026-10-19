/*
    An MbedTLS peer that stops reading must leave the event loop waiting for socket writability.
    Count loop turns over a fixed interval to detect busy looping.
 */
#include "goahead.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include <sys/socket.h>
#include <time.h>

#include "../../src/http.c"
#include "../../src/socket.c"
#include "../../src/mbedtls.c"

#define OUTPUT 20000                /* Response bytes queued, more than the socket pair can hold */
#define WATCH  500                  /* Milliseconds to watch the loop */

static mbedtls_ssl_config  clientConf;
static mbedtls_ssl_context client;
static mbedtls_net_context clientNet;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int64 nowMs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/*
    Connect the real server event loop to an MbedTLS client over a socket pair.
 */
static Webs *connectPair(void)
{
    Webs *wp;
    int  fds[2], sid, wid;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        return NULL;
    }
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    socketPtr(sid)->sock = fds[0];
    socketHighestFd = max(socketHighestFd, fds[0]);
    socketSetBlock(sid, 0);
    wid = websAlloc(sid);
    wp = webs[wid];
    wp->flags |= WEBS_SECURE;
    if (sslUpgrade(wp) < 0) {
        return NULL;
    }
    socketCreateHandler(sid, SOCKET_READABLE, socketEvent, wp);

    clientNet.fd = fds[1];
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
    mbedtls_ssl_config_init(&clientConf);
    mbedtls_ssl_config_defaults(&clientConf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&clientConf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&clientConf, mbedtls_ctr_drbg_random, &cfg.ctr);
    mbedtls_ssl_init(&client);
    mbedtls_ssl_setup(&client, &clientConf);
    mbedtls_ssl_set_bio(&client, &clientNet, mbedtls_net_send, mbedtls_net_recv, NULL);
    return wp;
}

static bool handshake(Webs *wp, int rounds)
{
    MbedSocket *mb;
    bool       clientDone;
    int        i;

    mb = wp->ssl;
    clientDone = 0;
    for (i = 0; i < rounds; i++) {
        if (!clientDone && mbedtls_ssl_handshake(&client) == 0) {
            clientDone = 1;
        }
        if (socketSelect(-1, 5)) {
            socketProcess();
        }
        if (clientDone && mbedtls_ssl_is_handshake_over(&mb->ctx)) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    Webs    *wp;
    WebsBuf *op;
    char    *block, path[1024];
    int64   start;
    int     turns;

    if (argc < 2) {
        fprintf(stderr, "usage: mbedtls-stalled-reader CERTS_DIR\n");
        return 1;
    }
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    fmt(path, sizeof(path), "%s/self.crt", argv[1]);
    websSetSslCertFile(path);
    fmt(path, sizeof(path), "%s/self.key", argv[1]);
    websSetSslKeyFile(path);
    if (sslOpen() < 0 || socketOpen() < 0) {
        fprintf(stderr, "cannot open TLS\n");
        return 1;
    }
    if ((wp = connectPair()) == NULL || !handshake(wp, 200)) {
        fprintf(stderr, "handshake failed\n");
        return 1;
    }
    /*
        Queue more than the socket pair can hold, leaving output pending while the peer never reads.
     */
    op = &wp->output;
    bufFree(op);
    bufCreate(op, OUTPUT + 1, OUTPUT + 1);
    block = walloc(OUTPUT);
    memset(block, 'z', OUTPUT);
    bufPutBlk(op, block, OUTPUT);
    wp->state = WEBS_RUNNING;
    socketCreateHandler(wp->sid, SOCKET_WRITABLE, socketEvent, wp);

    start = nowMs();
    for (turns = 0; nowMs() - start < WATCH; turns++) {
        if (socketSelect(-1, 100)) {
            socketProcess();
        }
    }
    printf("turns %d\n", turns);
    printf("queued %d\n", (int) bufLen(op));
    fflush(stdout);
    wfree(block);
    mbedtls_ssl_free(&client);
    mbedtls_ssl_config_free(&clientConf);
    return 0;
}
