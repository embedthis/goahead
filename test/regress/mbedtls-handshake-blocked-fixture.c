/*
    mbedtls-handshake-blocked-fixture.c - A TLS handshake whose output does not fit the socket buffer must complete

    Builds the HTTP, socket and MbedTLS layers into one program and turns the real event loop over a socket pair
    whose buffers are smaller than the server's first flight. The loop has to continue the handshake from a
    writable event: a client waiting for the rest of the flight sends nothing to wake a loop that waits only for
    input.
 */
#include "goahead.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include <sys/socket.h>

#include "../../src/http.c"
#include "../../src/socket.c"
#include "../../src/mbedtls.c"

#define SMALL  1024                 /* Socket buffers smaller than the server's first flight */
#define ROUNDS 400                  /* Event loop turns of 5 ms allowed for the handshake */

static mbedtls_ssl_config  clientConf;
static mbedtls_ssl_context client;
static mbedtls_net_context clientNet;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    A server connection on one end of a socket pair, registered as websAccept registers it, and an MbedTLS client
    on the other end
 */
static Webs *connectPair(void)
{
    Webs *wp;
    int  fds[2], sid, wid, size;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        return NULL;
    }
    size = SMALL;
    setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

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

/*
    Alternate the client with turns of the server's event loop until both ends finish the handshake
 */
static bool handshake(Webs *wp)
{
    MbedSocket *mb;
    bool       clientDone;
    int        i;

    mb = wp->ssl;
    clientDone = 0;
    for (i = 0; i < ROUNDS; i++) {
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
    Webs *wp;
    char path[1024];

    if (argc < 2) {
        fprintf(stderr, "usage: mbedtls-handshake-blocked CERTS_DIR\n");
        return 1;
    }
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    fmt(path, sizeof(path), "%s/test.crt", argv[1]);
    websSetSslCertFile(path);
    fmt(path, sizeof(path), "%s/test.key", argv[1]);
    websSetSslKeyFile(path);
    if (sslOpen() < 0 || socketOpen() < 0) {
        fprintf(stderr, "cannot open TLS\n");
        return 1;
    }
    if ((wp = connectPair()) == NULL) {
        fprintf(stderr, "cannot connect\n");
        return 1;
    }
    printf("handshake %s\n", handshake(wp) ? "completed" : "stalled");
    fflush(stdout);
    mbedtls_ssl_free(&client);
    mbedtls_ssl_config_free(&clientConf);
    return 0;
}
