/*
    Check TLS configuration and injected setup failures against the backend.
 */
#include "goahead.h"
#include "mbedtls/ssl.h"
static int failSetup = 1, freeCalls, bioCalls, handshakeCalls;
static WebsSocket testSocket;
static int checkedSetup(mbedtls_ssl_context *ctx, const mbedtls_ssl_config *conf)
{
    return failSetup ? MBEDTLS_ERR_SSL_ALLOC_FAILED : mbedtls_ssl_setup(ctx, conf);
}
static void checkedFree(mbedtls_ssl_context *ctx)
{
    freeCalls++;
    mbedtls_ssl_free(ctx);
}
static void checkedBio(mbedtls_ssl_context *ctx, void *bio, mbedtls_ssl_send_t *send,
                       mbedtls_ssl_recv_t *recv, mbedtls_ssl_recv_timeout_t *timeout)
{
    if (failSetup) { fprintf(stderr, "BIO reached after setup failure\n"); exit(1); }
    bioCalls++;
    mbedtls_ssl_set_bio(ctx, bio, send, recv, timeout);
}
static int checkedHandshake(mbedtls_ssl_context *ctx)
{
    handshakeCalls++;
    return MBEDTLS_ERR_SSL_WANT_READ;
}
static WebsSocket *checkedSocket(int sid) { return &testSocket; }
/*
    Inspect the configured DH group directly; handshakes can avoid an invalid DHE group.
 */
static size_t dhPrimeLen, dhGeneratorLen;
static unsigned char dhPrimeHead[8];
static int checkedDhParams(mbedtls_ssl_config *conf, const unsigned char *p, size_t plen,
                           const unsigned char *g, size_t glen)
{
    dhPrimeLen = plen;
    dhGeneratorLen = glen;
    memcpy(dhPrimeHead, p, plen < sizeof(dhPrimeHead) ? plen : sizeof(dhPrimeHead));
    return mbedtls_ssl_conf_dh_param_bin(conf, p, plen, g, glen);
}
static bool mismatchLogged, encryptedLogged;
#define LOGGED(flag) ((flag) || !ME_GOAHEAD_LOGGING)
#if ME_GOAHEAD_LOGGING
static char traced[80];
static int  traceCalls;
static void captureLog(int level, cchar *msg)
{
    traceCalls++;
    scopy(traced, sizeof(traced), msg);
    if (strstr(msg, "does not match certificate")) {
        mismatchLogged = 1;
    }
    if (strstr(msg, "is encrypted. GoAhead requires an unencrypted private key")) {
        encryptedLogged = 1;
    }
}
#endif
/*
    Track file buffers to detect leaks after parsing failures without a leak checker.
 */
static char *fileBuf;
static int  fileBufFreed;
static char *checkedReadWholeFile(cchar *path)
{
    fileBuf = websReadWholeFile(path);
    fileBufFreed = 0;
    return fileBuf;
}
static void checkedWfree(void *blk)
{
    if (blk && blk == fileBuf) {
        fileBufFreed = 1;
    }
    wfree(blk);
}
#define mbedtls_ssl_conf_dh_param_bin checkedDhParams
#define mbedtls_ssl_setup checkedSetup
#define mbedtls_ssl_free checkedFree
#define mbedtls_ssl_set_bio checkedBio
#define mbedtls_ssl_handshake checkedHandshake
#define socketPtr checkedSocket
#define websReadWholeFile checkedReadWholeFile
#define wfree checkedWfree
#include "../../src/mbedtls.c"
#undef websReadWholeFile
#undef wfree
#undef mbedtls_ssl_conf_dh_param_bin
#undef mbedtls_ssl_setup
#undef mbedtls_ssl_free
#undef mbedtls_ssl_set_bio
#undef mbedtls_ssl_handshake
#undef socketPtr
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "Failed: %s\n", #expr); return 1; } } while (0)
static int openWith(cchar *certs, cchar *cert, cchar *key)
{
    char path[1024];

    fmt(path, sizeof(path), "%s/%s", certs, cert);
    websSetSslCertFile(cert[0] == '/' ? cert : path);
    fmt(path, sizeof(path), "%s/%s", certs, key);
    websSetSslKeyFile(key[0] == '/' ? key : path);
    return sslOpen();
}
int main(int argc, char **argv)
{
    Webs wp;
    int rc, i;
    memset(&wp, 0, sizeof(wp));
    if (argc < 4) {
        fprintf(stderr, "usage: probe CERTS_DIR ENCRYPTED_KEY MALFORMED_FILE\n");
        return 1;
    }

#if ME_GOAHEAD_LOGGING
    logSetHandler(captureLog);
    websSetLogLevel(9);
    traceMbed(NULL, 0, "debug.c", 1, "line\n");
    CHECK(traceCalls == 1 && smatch(traced, "line"));
    traceMbed(NULL, 0, "debug.c", 1, "no newline");
    CHECK(traceCalls == 2 && smatch(traced, "no newline"));
    traceMbed(NULL, 0, "debug.c", 1, "");
    CHECK(traceCalls == 3 && smatch(traced, ""));
    traceMbed(NULL, 0, "debug.c", 1, "\n");
    CHECK(traceCalls == 4 && smatch(traced, ""));
    websSetLogLevel(0);
    puts("debug hook passed");
#endif

    /* RFC 3526 2048-bit prime: 256 bytes, first eight 0xFF. */
    failSetup = 0;
    CHECK(sslOpen() == 0);
    CHECK(dhPrimeLen == 256);
    CHECK(dhGeneratorLen >= 1);
    for (i = 0; i < 8; i++) {
        CHECK(dhPrimeHead[i] == 0xFF);
    }
    puts("dh group passed");

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    CHECK(cfg.conf.MBEDTLS_PRIVATE(min_tls_version) ==
          (ME_GOAHEAD_SSL_NO_V1_2 ? MBEDTLS_SSL_VERSION_TLS1_3 : MBEDTLS_SSL_VERSION_TLS1_2));
    CHECK(cfg.conf.MBEDTLS_PRIVATE(max_tls_version) ==
          (ME_GOAHEAD_SSL_NO_V1_3 ? MBEDTLS_SSL_VERSION_TLS1_2 : MBEDTLS_SSL_VERSION_TLS1_3));
#endif
    {
        uint32_t flags = 0;
        CHECK(verifyChain(NULL, NULL, ME_GOAHEAD_SSL_VERIFY_DEPTH, &flags) == 0 && flags == 0);
        CHECK(verifyChain(NULL, NULL, ME_GOAHEAD_SSL_VERIFY_DEPTH + 1, &flags) == 0 && flags != 0);
    }
    sslClose();
    puts("versions and depth passed");


    CHECK(openWith(argv[1], "test.crt", "ec.key") == -1 && !sslHasCertificate() && LOGGED(mismatchLogged));
    sslClose();
    mismatchLogged = 0;
    CHECK(openWith(argv[1], "test.crt", "self.key") == -1 && !sslHasCertificate() && LOGGED(mismatchLogged));
    sslClose();
    CHECK(openWith(argv[1], "test.crt", "test.key") == 0 && sslHasCertificate());
    sslClose();
    puts("key mismatch passed");

    CHECK(openWith(argv[1], "test.crt", argv[2]) == -1 && !sslHasCertificate() && LOGGED(encryptedLogged));
    CHECK(fileBufFreed);
    sslClose();
    puts("encrypted key passed");


    fileBufFreed = 0;
    CHECK(openWith(argv[1], "test.crt", argv[3]) == -1 && !sslHasCertificate());
    CHECK(fileBuf && fileBufFreed);
    sslClose();
    fileBufFreed = 0;
    CHECK(openWith(argv[1], argv[3], "test.key") == -1 && !sslHasCertificate());
    CHECK(fileBuf && fileBufFreed);
    sslClose();
    {
        mbedtls_x509_crl crl;

        mbedtls_x509_crl_init(&crl);
        fileBufFreed = 0;
        CHECK(parseCrl(&crl, argv[3]) == -1);
        CHECK(fileBuf && fileBufFreed);
        mbedtls_x509_crl_free(&crl);
    }
    puts("malformed files passed");
    websSetSslCertFile(NULL);
    websSetSslKeyFile(NULL);
    failSetup = 1;

    CHECK(psa_crypto_init() == PSA_SUCCESS);
    mbedtls_entropy_init(&cfg.entropy);
    mbedtls_ctr_drbg_init(&cfg.ctr);
    rc = mbedtls_ctr_drbg_seed(&cfg.ctr, mbedtls_entropy_func, &cfg.entropy, NULL, 0);
    CHECK(rc == 0);
    mbedtls_ssl_config_init(&cfg.conf);
    rc = mbedtls_ssl_config_defaults(&cfg.conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
    CHECK(rc == 0);
    mbedtls_ssl_conf_rng(&cfg.conf, mbedtls_ctr_drbg_random, &cfg.ctr);
    rc = sslUpgrade(&wp);
    CHECK(rc == -1);
    CHECK(wp.ssl == NULL);
    CHECK(freeCalls == 1 && bioCalls == 0 && handshakeCalls == 0);
    sslFree(&wp);
    CHECK(freeCalls == 1);
    failSetup = 0;
    rc = sslUpgrade(&wp);
    CHECK(rc == 0 && wp.ssl != NULL);
    CHECK(bioCalls == 1 && handshakeCalls == 1);
    testSocket.flags |= SOCKET_EOF;
    sslFree(&wp);
    CHECK(freeCalls == 2 && wp.ssl == NULL);
    mbedtls_ssl_config_free(&cfg.conf);
    mbedtls_ctr_drbg_free(&cfg.ctr);
    mbedtls_entropy_free(&cfg.entropy);
    puts("setup failure and success passed");
    return 0;
}
