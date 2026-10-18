/*
    mbedtls.c - MbedTLS interface to GoAhead

    This code requires MbedTLS 3.6.7 or later.

    To build MbedTLS, use:
        git checkout RELEASE-TAG
        cmake -DCMAKE_BUILD_TYPE=Debug .
        make VERBOSE=1

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Include *********************************/

#include    "goahead.h"

#if ME_COM_MBEDTLS
    #include "mbedtls/mbedtls_config.h"
    #include "mbedtls/ssl.h"
    #include "mbedtls/ssl_cache.h"
    #include "mbedtls/ssl_ticket.h"
    #include "mbedtls/ctr_drbg.h"
    #include "mbedtls/net_sockets.h"
    #include "psa/crypto.h"
    #include "mbedtls/debug.h"
    #include "mbedtls/error.h"
    #include "mbedtls/check_config.h"

#if MBEDTLS_VERSION_NUMBER < 0x03060700
    #error "GoAhead requires MbedTLS 3.6.7 or later. Earlier releases carry known certificate verification flaws."
#endif

/************************************* Defines ********************************/

typedef struct MbedConfig {
    mbedtls_x509_crt ca;                        /* Certificate authority bundle to verify peer */
    mbedtls_ssl_cache_context cache;            /* Session cache context */
    mbedtls_ssl_config conf;                    /* SSL configuration */
    mbedtls_x509_crt cert;                      /* Certificate (own) */
    mbedtls_ctr_drbg_context ctr;               /* Counter random generator state */
    mbedtls_ssl_ticket_context tickets;         /* Session tickets */
    mbedtls_entropy_context entropy;            /* Entropy context */
    mbedtls_x509_crl revoke;                    /* Certificate authority bundle to verify peer */
    mbedtls_pk_context pkey;                    /* Private key */
    int *ciphers;                               /* Set of acceptable ciphers - null terminated */
    bool ownCert;                               /* Own certificate and key are installed */
} MbedConfig;

/*
    Per socket state
 */
typedef struct MbedSocket {
    mbedtls_ssl_context ctx;                    /* SSL state */
    mbedtls_ssl_session session;                /* SSL sessions */
} MbedSocket;

static MbedConfig cfg;

/*
    GoAhead Log level to start SSL tracing
 */
static int mbedLogLevel = ME_GOAHEAD_SSL_LOG_LEVEL;

/*
    Runtime-settable cert/key paths. Override the compile-time ME_GOAHEAD_SSL_CERTIFICATE /
    ME_GOAHEAD_SSL_KEY macros when websSetSslCertFile / websSetSslKeyFile are called
    before sslOpen(). Needed by test drivers that ship their own cert alongside the binary.
 */
static char *sslCertPath;
static char *sslKeyPath;

/************************************ Forwards ********************************/

static int *getCipherSuite(cchar *ciphers);
static int  mbedHandshake(Webs *wp);
static int  parseCert(mbedtls_x509_crt *cert, char *file);
static int  parseCrl(mbedtls_x509_crl *crl, char *path);
static int  parseKey(mbedtls_pk_context *key, char *path);
static void merror(int rc, PRINTF_FORMAT cchar *fmt, ...) PRINTF_ATTRIBUTE(2, 3);
static char *replaceHyphen(char *cipher, char from, char to);
static void traceMbed(void *context, int level, cchar *file, int line, cchar *str);
static bool sameName(mbedtls_x509_buf *a, mbedtls_x509_buf *b);
static int  verifyChain(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags);

/************************************** Code **********************************/

PUBLIC int sslOpen()
{
    mbedtls_ssl_config           *conf;
    mbedtls_ssl_protocol_version minVersion, maxVersion;
    cuchar                       dhm_p[] = MBEDTLS_DHM_RFC3526_MODP_2048_P_BIN;
    cuchar                       dhm_g[] = MBEDTLS_DHM_RFC3526_MODP_2048_G_BIN;
    char                         *keyPath, *certPath;
    int                          rc;

    trace(7, "Initializing MbedTLS SSL");

    psa_crypto_init();
    mbedtls_entropy_init(&cfg.entropy);

    conf = &cfg.conf;
    mbedtls_ssl_config_init(conf);

    mbedtls_ssl_conf_dbg(conf, traceMbed, NULL);
    if (websGetLogLevel() >= mbedLogLevel) {
        mbedtls_debug_set_threshold(websGetLogLevel() - mbedLogLevel);
    }
    mbedtls_pk_init(&cfg.pkey);
    mbedtls_ssl_cache_init(&cfg.cache);
    mbedtls_ctr_drbg_init(&cfg.ctr);
    mbedtls_x509_crt_init(&cfg.cert);
    mbedtls_ssl_ticket_init(&cfg.tickets);

    if ((rc =
             mbedtls_ctr_drbg_seed(&cfg.ctr, mbedtls_entropy_func, &cfg.entropy, (cuchar*) ME_NAME,
                                   slen(ME_NAME))) < 0) {
        merror(rc, "Cannot seed rng");
        return -1;
    }

    /*
        Set the server certificate and key files. Prefer runtime-provided paths
        (set via websSetSsl{Cert,Key}File) over the compile-time defaults.
     */
    keyPath = sslKeyPath ? sslKeyPath : (char*) ME_GOAHEAD_SSL_KEY;
    certPath = sslCertPath ? sslCertPath : (char*) ME_GOAHEAD_SSL_CERTIFICATE;
    if (keyPath && *keyPath) {
        /*
            Load a decrypted PEM format private key. The last arg is the private key.
         */
        if (parseKey(&cfg.pkey, keyPath) < 0) {
            return -1;
        }
    }
    if (certPath && *certPath) {
        /*
            Load a PEM format certificate file
         */
        if (parseCert(&cfg.cert, certPath) < 0) {
            return -1;
        }
    }
    if (*ME_GOAHEAD_SSL_AUTHORITY) {
        if (parseCert(&cfg.ca, ME_GOAHEAD_SSL_AUTHORITY) != 0) {
            return -1;
        }
    }
    if (*ME_GOAHEAD_SSL_REVOKE) {
        /*
            Load a PEM format certificate file
         */
        if (parseCrl(&cfg.revoke, (char*) ME_GOAHEAD_SSL_REVOKE) != 0) {
            return -1;
        }
    }
    cfg.ciphers = getCipherSuite((ME_GOAHEAD_SSL_CIPHERS && *ME_GOAHEAD_SSL_CIPHERS) ?
                                 ME_GOAHEAD_SSL_CIPHERS : ME_GOAHEAD_SSL_DEFAULT_CIPHERS);
    if (!cfg.ciphers) {
        error("No supported and permitted cipher in the cipher list");
        return -1;
    }

    if ((rc = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) < 0) {
        merror(rc, "Cannot set mbedtls defaults");
        return -1;
    }
    mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &cfg.ctr);

    /*
        Configure larger DH parameters
     */
    if ((rc = mbedtls_ssl_conf_dh_param_bin(conf, dhm_p, sizeof(dhm_p), dhm_g, sizeof(dhm_g))) < 0) {
        merror(rc, "Cannot set DH params");
        return -1;
    }

    /*
        Allow unknown issuers when configured, but still require a client certificate.
     */
    if (!ME_GOAHEAD_SSL_VERIFY_PEER) {
        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
    } else if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    } else {
        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    }
    mbedtls_ssl_conf_verify(conf, verifyChain, NULL);

    /*
        Apply configured protocol versions supported by this MbedTLS build.
     */
    minVersion = ME_GOAHEAD_SSL_NO_V1_2 ? MBEDTLS_SSL_VERSION_TLS1_3 : MBEDTLS_SSL_VERSION_TLS1_2;
    maxVersion = ME_GOAHEAD_SSL_NO_V1_3 ? MBEDTLS_SSL_VERSION_TLS1_2 : MBEDTLS_SSL_VERSION_TLS1_3;
#if !defined(MBEDTLS_SSL_PROTO_TLS1_2)
    minVersion = MBEDTLS_SSL_VERSION_TLS1_3;
#endif
#if !defined(MBEDTLS_SSL_PROTO_TLS1_3)
    maxVersion = MBEDTLS_SSL_VERSION_TLS1_2;
#endif
    if (minVersion > maxVersion) {
        error("No TLS version is enabled: check ME_GOAHEAD_SSL_NO_V1_2, ME_GOAHEAD_SSL_NO_V1_3 and the MbedTLS build");
        return -1;
    }
    mbedtls_ssl_conf_min_tls_version(conf, minVersion);
    mbedtls_ssl_conf_max_tls_version(conf, maxVersion);

    /*
        Disable resumption with client certificates so every connection verifies them.
     */
    if (ME_GOAHEAD_SSL_TICKET && !ME_GOAHEAD_SSL_VERIFY_PEER) {
        if ((rc = mbedtls_ssl_ticket_setup(&cfg.tickets, mbedtls_ctr_drbg_random, &cfg.ctr,
                                           MBEDTLS_CIPHER_AES_256_GCM, ME_GOAHEAD_SSL_TIMEOUT)) < 0) {
            merror(rc, "Cannot setup ticketing sessions");
            return -1;
        }
        mbedtls_ssl_conf_session_tickets_cb(conf, mbedtls_ssl_ticket_write, mbedtls_ssl_ticket_parse, &cfg.tickets);
    }

    /*
        Configure server-side session cache
     */
    if (ME_GOAHEAD_SSL_CACHE && !ME_GOAHEAD_SSL_VERIFY_PEER) {
        mbedtls_ssl_conf_session_cache(conf, &cfg.cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
        mbedtls_ssl_cache_set_max_entries(&cfg.cache, ME_GOAHEAD_SSL_CACHE);
        mbedtls_ssl_cache_set_timeout(&cfg.cache, ME_GOAHEAD_SSL_TIMEOUT);
    }

    mbedtls_ssl_conf_ciphersuites(conf, cfg.ciphers);

    /*
        Configure CA certificate bundle and revocation list
     */
    mbedtls_ssl_conf_ca_chain(conf, *ME_GOAHEAD_SSL_AUTHORITY ? &cfg.ca : NULL,
                              *ME_GOAHEAD_SSL_REVOKE ? &cfg.revoke : NULL);

    /*
        Configure server cert and key
     */
    if (keyPath && *keyPath && certPath && *certPath) {
        if ((rc = mbedtls_pk_check_pair(&cfg.cert.pk, &cfg.pkey, mbedtls_ctr_drbg_random, &cfg.ctr)) != 0) {
            merror(rc, "Private key %s does not match certificate %s", keyPath, certPath);
            return -1;
        }
        if ((rc = mbedtls_ssl_conf_own_cert(conf, &cfg.cert, &cfg.pkey)) < 0) {
            merror(rc, "Cannot define certificate and private key");
            return -1;
        }
        cfg.ownCert = 1;
    }
    if (websGetLogLevel() >= 5) {
        char cipher[80];
        cint *cp;
        trace(5, "mbedtls: Supported Ciphers");
        for (cp = mbedtls_ssl_list_ciphersuites(); *cp; cp++) {
            scopy(cipher, sizeof(cipher), (char*) mbedtls_ssl_get_ciphersuite_name(*cp));
            replaceHyphen(cipher, '-', '_');
            trace(5, "mbedtls: %s (0x%04X)", cipher, *cp);
        }
    }
    return 0;
}


/*
    Enforce runtime chain depth. With revocation enabled, require each issuer's CRL; MbedTLS skips missing CRLs.
 */
static int verifyChain(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
    mbedtls_x509_crl *crl;

    if (depth > ME_GOAHEAD_SSL_VERIFY_DEPTH) {
        *flags |= MBEDTLS_X509_BADCERT_OTHER;
    }
    if (crt && *ME_GOAHEAD_SSL_REVOKE && !sameName(&crt->subject_raw, &crt->issuer_raw)) {
        for (crl = &cfg.revoke; crl && crl->version; crl = crl->next) {
            if (sameName(&crl->issuer_raw, &crt->issuer_raw)) {
                return 0;
            }
        }
        logmsg(2, "No revocation list for issuer of certificate at depth %d", depth);
        *flags |= MBEDTLS_X509_BADCRL_NOT_TRUSTED;
    }
    return 0;
}


static bool sameName(mbedtls_x509_buf *a, mbedtls_x509_buf *b)
{
    return a->len == b->len && memcmp(a->p, b->p, a->len) == 0;
}


PUBLIC void sslClose()
{
    mbedtls_ctr_drbg_free(&cfg.ctr);
    mbedtls_pk_free(&cfg.pkey);
    mbedtls_x509_crt_free(&cfg.cert);
    mbedtls_x509_crt_free(&cfg.ca);
    mbedtls_x509_crl_free(&cfg.revoke);
    mbedtls_ssl_cache_free(&cfg.cache);
    mbedtls_ssl_ticket_free(&cfg.tickets);
    mbedtls_ssl_config_free(&cfg.conf);
    mbedtls_entropy_free(&cfg.entropy);
    mbedtls_psa_crypto_free();
    wfreeset(cfg.ciphers);
    cfg.ownCert = 0;
    wfree(sslCertPath);
    sslCertPath = NULL;
    wfree(sslKeyPath);
    sslKeyPath = NULL;
}


PUBLIC bool sslHasCertificate(void)
{
    return cfg.ownCert;
}


/*
    Runtime override for the server certificate path. Must be called before sslOpen().
 */
PUBLIC void websSetSslCertFile(cchar *path)
{
    wfree(sslCertPath);
    sslCertPath = path ? sclone(path) : NULL;
}


/*
    Runtime override for the server key path. Must be called before sslOpen().
 */
PUBLIC void websSetSslKeyFile(cchar *path)
{
    wfree(sslKeyPath);
    sslKeyPath = path ? sclone(path) : NULL;
}


PUBLIC int sslUpgrade(Webs *wp)
{
    MbedSocket          *mb;
    WebsSocket          *sp;
    mbedtls_ssl_context *ctx;
    int                 rc;

    assert(wp);
    if ((mb = walloc(sizeof(MbedSocket))) == 0) {
        return -1;
    }
    memset(mb, 0, sizeof(MbedSocket));
    wp->ssl = mb;
    sp = socketPtr(wp->sid);
    ctx = &mb->ctx;

    mbedtls_ssl_init(ctx);
    if ((rc = mbedtls_ssl_setup(ctx, &cfg.conf)) != 0) {
        error("Cannot configure TLS connection: %d", rc);
        mbedtls_ssl_free(ctx);
        wfreeset(wp->ssl);
        return -1;
    }
    mbedtls_ssl_set_bio(ctx, &sp->sock, mbedtls_net_send, mbedtls_net_recv, 0);

    if (mbedHandshake(wp) < 0) {
        return -1;
    }
    return 0;
}


PUBLIC void sslFree(Webs *wp)
{
    MbedSocket *mb;
    WebsSocket *sp;

    mb = wp->ssl;
    sp = socketPtr(wp->sid);
    if (mb) {
        if (!(sp->flags & SOCKET_EOF)) {
            mbedtls_ssl_close_notify(&mb->ctx);
        }
        mbedtls_ssl_free(&mb->ctx);
        wfree(mb);
        wp->ssl = 0;
    }
}


/*
    Initiate or continue SSL handshaking with the peer. This routine does not block.
    Return -1 on errors, 0 incomplete and awaiting I/O, 1 if successful
 */
static int mbedHandshake(Webs *wp)
{
    WebsSocket *sp;
    MbedSocket *mb;
    int        rc, vrc;

    mb = (MbedSocket*) wp->ssl;
    rc = 0;

    sp = socketPtr(wp->sid);
    sp->flags |= SOCKET_HANDSHAKING;

    while ((rc = mbedtls_ssl_handshake(&mb->ctx)) != 0) {
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return 0;
        }
        break;
    }
    sp->flags &= ~SOCKET_HANDSHAKING;

    /*
        Analyze the handshake result
     */
    if (rc < 0) {
        if (rc == MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED && !cfg.ownCert) {
            error("Missing required certificate and key");
        } else if (rc == MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED) {
            trace(2, "Server requires a client certificate");
        } else {
            char ebuf[256];
            mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
            trace(2, "%s: error -0x%x", ebuf, -rc);
        }
        sp->flags |= SOCKET_EOF;
        errno = EPROTO;
        return -1;
    }
    vrc = (int) mbedtls_ssl_get_verify_result(&mb->ctx);
    if (!ME_GOAHEAD_SSL_VERIFY_ISSUER) {
        /*
            SECURITY Acceptable: without issuer verification any client certificate is accepted, including
            self-signed and forged-issuer certificates, as in the OpenSSL backend. Other failures are kept.
         */
        vrc &= ~MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    }
    if (vrc != 0) {
        if (vrc & MBEDTLS_X509_BADCERT_MISSING) {
            logmsg(2, "Certificate missing");

        } else if (vrc & MBEDTLS_X509_BADCERT_EXPIRED) {
            logmsg(2, "Certificate expired");

        } else if (vrc & MBEDTLS_X509_BADCERT_REVOKED) {
            logmsg(2, "Certificate revoked");

        } else if (vrc & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
            logmsg(2, "Certificate common name mismatch");

        } else if (vrc & MBEDTLS_X509_BADCERT_KEY_USAGE || vrc & MBEDTLS_X509_BADCERT_EXT_KEY_USAGE) {
            logmsg(2, "Unauthorized key use in certificate");

        } else if (vrc & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
            logmsg(2, "Certificate not trusted");

        } else if (vrc & MBEDTLS_X509_BADCRL_NOT_TRUSTED) {
            logmsg(2, "Certificate revocation list missing or not trusted");

        } else if (vrc & MBEDTLS_X509_BADCERT_SKIP_VERIFY) {
            /*
                MBEDTLS_SSL_VERIFY_NONE requested, so ignore error
             */
            vrc = 0;

        } else {
            if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                logmsg(2, "Peer disconnected");

            } else {
                logmsg(2, "Cannot handshake: error -0x%x", -rc);
            }
        }
        if (vrc != 0 && ME_GOAHEAD_SSL_VERIFY_PEER) {
            if (mbedtls_ssl_get_peer_cert(&mb->ctx) == 0) {
                logmsg(2, "Peer did not provide a certificate");
            }
            sp->flags |= SOCKET_EOF;
            errno = EPROTO;
            return -1;
        }
    }
    return 1;
}


PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket *sp;
    MbedSocket *mb;
    int        rc;

    if (!wp->ssl) {
        assert(0);
        return -1;
    }
    mb = (MbedSocket*) wp->ssl;
    assert(mb);
    sp = socketPtr(wp->sid);

    if (sp->flags & SOCKET_HANDSHAKING) {
        if ((rc = mbedHandshake(wp)) <= 0) {
            return rc;
        }
    }
    while (1) {
        rc = mbedtls_ssl_read(&mb->ctx, buf, (int) len);
        if (rc < 0) {
            if (rc == MBEDTLS_ERR_SSL_WANT_READ ||
                rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
                rc == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ||
                rc == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS ||
                rc == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
                rc = 0;
                break;
            } else if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                sp->flags |= SOCKET_EOF;
                return -1;
            } else if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                trace(5, "mbedtls: connection reset");
                sp->flags |= SOCKET_EOF;
                return -1;
            } else {
                trace(4, "mbedtls: read error -0x%x", -rc);
                sp->flags |= SOCKET_EOF;
                return -1;
            }
        } else if (rc == 0) {
            sp->flags |= SOCKET_EOF;
            return -1;
        }
        break;
    }
    socketHiddenData(sp, mbedtls_ssl_get_bytes_avail(&mb->ctx), SOCKET_READABLE);
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    MbedSocket *mb;
    WebsSocket *sp;
    ssize      totalWritten;
    int        rc;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    mb = (MbedSocket*) wp->ssl;
    sp = socketPtr(wp->sid);

    if (!mbedtls_ssl_is_handshake_over(&mb->ctx)) {
        //  Continuing the handshake here would read from the peer inside a blocking flush
        sp->flags |= SOCKET_EOF;
        socketSetError(ECONNRESET);
        return -1;
    }
    totalWritten = 0;
    do {
        rc = mbedtls_ssl_write(&mb->ctx, (uchar*) buf, (int) len);
        trace(7, "mbedtls write: wrote %d of %zd", rc, len);
        if (rc <= 0) {
            if (rc == MBEDTLS_ERR_SSL_WANT_READ ||
                rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
                rc == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ||
                rc == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS ||
                rc == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
                break;
            }
            if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                trace(4, "mbedtls_ssl_write peer closed");
            } else {
                trace(4, "mbedtls_ssl_write failed rc %d", rc);
            }
            /* Set a hard socket error so a stale EAGAIN cannot cause retries. */
            sp->flags |= SOCKET_EOF;
            socketSetError(ECONNRESET);
            return -1;
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= rc;
        }
    } while (len > 0);

    //  Output MbedTLS still holds is flushed when the socket is next writable, and the mark goes once it is sent
    socketHiddenData(sp, (ssize) mb->ctx.MBEDTLS_PRIVATE(out_left), SOCKET_WRITABLE);
    /*
        Return bytes already sent so a later EAGAIN cannot cause retransmission.
     */
    if (totalWritten == 0 && len > 0) {
        socketSetError(EAGAIN);
        return -1;
    }
    return totalWritten;
}


/*
    Reject static RSA, anonymous, null, DES, 3DES, RC4, and MD5 TLS 1.2 suites.
    Inspect names because MbedTLS keeps cipher fields private.
 */
static bool belowFloor(cchar *name)
{
    return sstarts(name, "TLS-RSA-WITH-") || strstr(name, "-anon-") || strstr(name, "-NULL-") ||
           strstr(name, "-DES-") || strstr(name, "-3DES-") || strstr(name, "-RC4-") || strstr(name, "-MD5");
}


/*
    Return zero-terminated supported suite codes, excluding weak suites and duplicates; NULL if empty.
 */
static int *getCipherSuite(cchar *ciphers)
{
    char  *cipher, *list, *name, *next;
    cchar *cp;
    int   i, j, ntokens, *result, code;

    if (!ciphers || *ciphers == 0) {
        return 0;
    }
    //  Every name yields at most one suite, so the separators bound the result
    for (ntokens = 1, cp = ciphers; *cp; cp++) {
        if (strchr(":, \t", *cp)) {
            ntokens++;
        }
    }
    result = walloc((ntokens + 1) * sizeof(int));

    next = list = sclone(ciphers);
    for (i = 0; (cipher = stok(next, ":, \t", &next)) != 0; ) {
        replaceHyphen(cipher, '_', '-');
        //  TLS 1.3 suites have no WITH in their IANA name and are called TLS1-3-xxx by MbedTLS
        if (sstarts(cipher, "TLS-") && !strstr(cipher, "-WITH-")) {
            name = sfmt("TLS1-3-%s", &cipher[4]);
        } else {
            name = sclone(cipher);
        }
        if ((code = mbedtls_ssl_get_ciphersuite_id(name)) <= 0) {
            error("Unsupported cipher \"%s\"", cipher);
        } else if (belowFloor(mbedtls_ssl_get_ciphersuite_name(code))) {
            error("Cipher \"%s\" is below the minimum TLS policy and is not enabled", cipher);
        } else {
            for (j = 0; j < i && result[j] != code; j++) { }
            if (j == i) {
                result[i++] = code;
            }
        }
        wfree(name);
    }
    result[i] = 0;
    wfree(list);
    if (i == 0) {
        wfree(result);
        return 0;
    }
    return result;
}


static int parseCert(mbedtls_x509_crt *cert, char *path)
{
    char  *buf;
    ssize len;
    int   rc;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read certificate %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        /* Looks PEM encoded so count the null in the length */
        len++;
    }
    rc = mbedtls_x509_crt_parse(cert, (uchar*) buf, len);
    memset(buf, 0, len);
    wfree(buf);
    if (rc != 0) {
        error("Unable to parse certificate %s", path);
        return -1;
    }
    return 0;
}


static int parseKey(mbedtls_pk_context *key, char *path)
{
    char  *buf;
    ssize len;
    int   rc;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read key %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        len++;
    }
    //  The generator derives the public point of a key stored without it
    rc = mbedtls_pk_parse_key(key, (uchar*) buf, len, NULL, 0, mbedtls_ctr_drbg_random, &cfg.ctr);
    memset(buf, 0, len);
    wfree(buf);
    if (rc != 0) {
        if (rc == MBEDTLS_ERR_PK_PASSWORD_REQUIRED) {
            error("Key %s is encrypted. GoAhead requires an unencrypted private key", path);
        } else {
            error("Unable to parse key %s", path);
        }
        return -1;
    }
    return 0;
}


static int parseCrl(mbedtls_x509_crl *crl, char *path)
{
    char  *buf;
    ssize len;
    int   rc;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read crl %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        len++;
    }
    rc = mbedtls_x509_crl_parse(crl, (uchar*) buf, len);
    memset(buf, 0, len);
    wfree(buf);
    if (rc != 0) {
        error("Unable to parse crl %s", path);
        return -1;
    }
    return 0;
}


static void traceMbed(void *context, int level, cchar *file, int line, cchar *str)
{
    ssize len;

    level += mbedLogLevel;
    if (level <= websGetLogLevel()) {
        len = slen(str);
        if (len > 0 && str[len - 1] == '\n') {
            len--;
        }
        trace(level, "%.*s", (int) len, str);
    }
}


static void merror(int rc, PRINTF_FORMAT cchar *fmt, ...)
{
    va_list ap;
    char    ebuf[ME_MAX_BUFFER], *msg;

    va_start(ap, fmt);
    msg = sfmtv(fmt, ap);
    va_end(ap);
    mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
    error("mbedtls error: -0x%x %s %s", -rc, msg, ebuf);
    wfree(msg);
}


static char *replaceHyphen(char *cipher, char from, char to)
{
    char *cp;

    for (cp = cipher; *cp; cp++) {
        if (*cp == from) {
            *cp = to;
        }
    }
    return cipher;
}

#else
void mbedDummy()
{
}
#endif /* ME_COM_MBEDTLS */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
