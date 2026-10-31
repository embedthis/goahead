/*
    Require ephemeral keys on TLS 1.3 resumption to preserve forward secrecy.
 */
#include "goahead.h"
#include "mbedtls/ssl.h"

static WebsSocket testSocket;
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
#define socketPtr checkedSocket
#include "../../src/mbedtls.c"
#undef socketPtr

#include "mbedtls-pipe-fixture.h"

//  Collecting a ticket on the client needs the new session ticket signal of MbedTLS 3.6
#if MBEDTLS_VERSION_NUMBER >= 0x03060000 && defined(MBEDTLS_SSL_PROTO_TLS1_3) && defined(MBEDTLS_SSL_SESSION_TICKETS)

/*
    A TLS 1.3 handshake from a client limited to the given key exchange modes. With a session, the client resumes
    it; without one, it collects the ticket the server issues into ticket.
 */
static cchar *handshake(int modes, mbedtls_ssl_session *session, mbedtls_ssl_session *ticket)
{
    mbedtls_ssl_config  conf;
    mbedtls_ssl_context client;
    MbedSocket          mb;
    Webs                wp;
    Pipe                clientPipe = { &toClient, &toServer }, serverPipe = { &toServer, &toClient };
    uchar               buf[64];
    cchar               *result;
    int                 i, rc, crc;

    toServer.len = toClient.len = 0;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &cfg.ctr);
    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_tls13_key_exchange_modes(&conf, modes);
    mbedtls_ssl_conf_tls13_enable_signal_new_session_tickets(&conf, MBEDTLS_SSL_TLS1_3_SIGNAL_NEW_SESSION_TICKETS_ENABLED);
    mbedtls_ssl_init(&client);
    mbedtls_ssl_setup(&client, &conf);
    mbedtls_ssl_set_hostname(&client, "localhost");
    mbedtls_ssl_set_bio(&client, &clientPipe, pipeSend, pipeRecv, NULL);
    if (session) {
        mbedtls_ssl_set_session(&client, session);
    }
    memset(&wp, 0, sizeof(wp));
    memset(&mb, 0, sizeof(mb));
    wp.ssl = &mb;
    mbedtls_ssl_init(&mb.ctx);
    mbedtls_ssl_setup(&mb.ctx, &cfg.conf);
    mbedtls_ssl_set_bio(&mb.ctx, &serverPipe, pipeSend, pipeRecv, NULL);

    crc = MBEDTLS_ERR_SSL_WANT_READ;
    for (i = 0, rc = 0; i < 100 && (rc == 0 || crc != 0); i++) {
        if (crc != 0) {
            crc = mbedtls_ssl_handshake(&client);
        }
        if (rc == 0) {
            rc = mbedHandshake(&wp);
        }
        if (rc < 0 || (crc != 0 && crc != MBEDTLS_ERR_SSL_WANT_READ && crc != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            break;
        }
    }
    result = (rc > 0 && crc == 0) ? "accepted" : rc < 0 || crc != 0 ? "refused" : "stalled";
    if (ticket && rc > 0 && crc == 0) {
        //  The server sends its ticket after the handshake; reading on the client takes it in
        for (i = 0; i < 10; i++) {
            mbedtls_ssl_handshake(&mb.ctx);
            if (mbedtls_ssl_read(&client, buf, sizeof(buf)) == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
                mbedtls_ssl_get_session(&client, ticket);
                break;
            }
        }
        if (i == 10) {
            result = "no ticket";
        }
    }
    mbedtls_ssl_free(&mb.ctx);
    mbedtls_ssl_free(&client);
    mbedtls_ssl_config_free(&conf);
    return result;
}


int main(int argc, char **argv)
{
    mbedtls_ssl_session ticket;
    char                path[1024];

    if (argc < 2) {
        fprintf(stderr, "usage: mbedtls-psk-ke CERTS_DIR\n");
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
    mbedtls_ssl_session_init(&ticket);
    printf("full %s\n", handshake(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_ALL, NULL, &ticket));
    printf("psk_dhe_ke %s\n", handshake(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL, &ticket, NULL));
    printf("psk_ke %s\n", handshake(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK, &ticket, NULL));
    fflush(stdout);
    mbedtls_ssl_session_free(&ticket);
    sslClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    printf("unsupported\n");
    return 0;
}
#endif
