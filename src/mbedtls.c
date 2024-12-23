/*
    mbedtls.c - MbedTLS interface to GoAhead

    To build MbedTLS, use:
        git checkout RELEASE-TAG
        cmake -DCMAKE_BUILD_TYPE=Debug .
        make VERBOSE=1

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Include *********************************/

#include    "goahead.h"

#if ME_COM_MBEDTLS
   /*
      Indent to bypass MakeMe dependencies
    */
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

/************************************* Defines ********************************/

typedef struct MbedConfig {
    mbedtls_x509_crt            ca;             /* Certificate authority bundle to verify peer */
    mbedtls_ssl_cache_context   cache;          /* Session cache context */
    mbedtls_ssl_config          conf;           /* SSL configuration */
    mbedtls_x509_crt            cert;           /* Certificate (own) */
    mbedtls_ctr_drbg_context    ctr;            /* Counter random generator state */
    mbedtls_ssl_ticket_context  tickets;        /* Session tickets */
    mbedtls_entropy_context     entropy;        /* Entropy context */
    mbedtls_x509_crl            revoke;         /* Certificate authority bundle to verify peer */
    mbedtls_pk_context          pkey;           /* Private key */
    int                         *ciphers;       /* Set of acceptable ciphers - null terminated */
} MbedConfig;

/*
    Per socket state
 */
typedef struct MbedSocket {
    mbedtls_ssl_context         ctx;            /* SSL state */
    mbedtls_ssl_session         session;        /* SSL sessions */
} MbedSocket;

static MbedConfig   cfg;

/*
    GoAhead Log level to start SSL tracing
 */
static int          mbedLogLevel = ME_GOAHEAD_SSL_LOG_LEVEL;

/************************************ Forwards ********************************/

static int *getCipherSuite(char *ciphers, int *len);
static int  mbedHandshake(Webs *wp);
static int  parseCert(mbedtls_x509_crt *cert, char *file);
static int  parseCrl(mbedtls_x509_crl *crl, char *path);
static int  parseKey(mbedtls_pk_context *key, char *path);
static void merror(int rc, char *fmt, ...);
static char *replaceHyphen(char *cipher, char from, char to);
static void traceMbed(void *context, int level, cchar *file, int line, cchar *str);

/************************************** Code **********************************/

PUBLIC int sslOpen()
{
    mbedtls_ssl_config  *conf;
    cuchar              dhm_p[] = MBEDTLS_DHM_RFC3526_MODP_2048_P_BIN;
    cuchar              dhm_g[] = MBEDTLS_DHM_RFC3526_MODP_2048_G_BIN;
    int                 rc;

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

    if ((rc = mbedtls_ctr_drbg_seed(&cfg.ctr, mbedtls_entropy_func, &cfg.entropy, (cuchar*) ME_NAME, slen(ME_NAME))) < 0) {
        merror(rc, "Cannot seed rng");
        return -1;
    }

    /*
        Set the server certificate and key files
     */
    if (*ME_GOAHEAD_SSL_KEY) {
        /*
            Load a decrypted PEM format private key. The last arg is the private key.
         */
        if (parseKey(&cfg.pkey, ME_GOAHEAD_SSL_KEY) < 0) {
            return -1;
        }
    }
    if (*ME_GOAHEAD_SSL_CERTIFICATE) {
        /*
            Load a PEM format certificate file
         */
        if (parseCert(&cfg.cert, ME_GOAHEAD_SSL_CERTIFICATE) < 0) {
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
    if (ME_GOAHEAD_SSL_CIPHERS && *ME_GOAHEAD_SSL_CIPHERS) {
        cfg.ciphers = getCipherSuite(ME_GOAHEAD_SSL_CIPHERS, NULL);
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
    if ((rc = mbedtls_ssl_conf_dh_param_bin(conf, dhm_p, sizeof(dhm_g), dhm_g, sizeof(dhm_g))) < 0) {
        merror(rc, "Cannot set DH params");
        return -1;
    }

    /*
        Set auth mode if peer cert should be verified
     */
	mbedtls_ssl_conf_authmode(conf, ME_GOAHEAD_SSL_VERIFY_PEER ? MBEDTLS_SSL_VERIFY_OPTIONAL : MBEDTLS_SSL_VERIFY_NONE);

    /*
        Configure ticket-based sessions
     */
    if (ME_GOAHEAD_SSL_TICKET) {
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
    if (ME_GOAHEAD_SSL_CACHE) {
        mbedtls_ssl_conf_session_cache(conf, &cfg.cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
        mbedtls_ssl_cache_set_max_entries(&cfg.cache, ME_GOAHEAD_SSL_CACHE);
        mbedtls_ssl_cache_set_timeout(&cfg.cache, ME_GOAHEAD_SSL_TIMEOUT);
    }

    /*
        Configure explicit cipher suite selection
     */
    if (cfg.ciphers) {
        mbedtls_ssl_conf_ciphersuites(conf, cfg.ciphers);
    }

    /*
        Configure CA certificate bundle and revocation list
     */
    mbedtls_ssl_conf_ca_chain(conf, *ME_GOAHEAD_SSL_AUTHORITY ? &cfg.ca : NULL, *ME_GOAHEAD_SSL_REVOKE ? &cfg.revoke : NULL);

    /*
        Configure server cert and key
     */
    if (ME_GOAHEAD_SSL_KEY[0] != '\0' && ME_GOAHEAD_SSL_CERTIFICATE[0] != '\0') {
        if ((rc = mbedtls_ssl_conf_own_cert(conf, &cfg.cert, &cfg.pkey)) < 0) {
            merror(rc, "Cannot define certificate and private key");
            return -1;
        }
    }
    if (websGetLogLevel() >= 5) {
        char    cipher[80];
        cint    *cp;
        trace(5, "mbedtls: Supported Ciphers");
        for (cp = mbedtls_ssl_list_ciphersuites(); *cp; cp++) {
            scopy(cipher, sizeof(cipher), (char*) mbedtls_ssl_get_ciphersuite_name(*cp));
            replaceHyphen(cipher, '-', '_');
            trace(5, "mbedtls: %s (0x%04X)", cipher, *cp);
        }
    }
    return 0;
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
    wfree(cfg.ciphers);
}


PUBLIC int sslUpgrade(Webs *wp)
{
    MbedSocket          *mb;
    WebsSocket          *sp;
    mbedtls_ssl_context *ctx;

    assert(wp);
    if ((mb = walloc(sizeof(MbedSocket))) == 0) {
        return -1;
    }
    memset(mb, 0, sizeof(MbedSocket));
    wp->ssl = mb;
    sp = socketPtr(wp->sid);
    ctx = &mb->ctx;

    mbedtls_ssl_init(ctx);
    mbedtls_ssl_setup(ctx, &cfg.conf);
	mbedtls_ssl_set_bio(ctx, &sp->sock, mbedtls_net_send, mbedtls_net_recv, 0);

    if (mbedHandshake(wp) < 0) {
        return -1;
    }
    return 0;
}


PUBLIC void sslFree(Webs *wp)
{
    MbedSocket  *mb;
    WebsSocket  *sp;

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
    WebsSocket  *sp;
    MbedSocket  *mb;
    int         rc, vrc;

    mb = (MbedSocket*) wp->ssl;
    rc = 0;

    sp = socketPtr(wp->sid);
    sp->flags |= SOCKET_HANDSHAKING;

    while ((rc = mbedtls_ssl_handshake(&mb->ctx)) != 0) {
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE)  {
            return 0;
        }
        break;
    }
    sp->flags &= ~SOCKET_HANDSHAKING;

    /*
        Analyze the handshake result
     */
    if (rc < 0) {
        if (rc == MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED &&
                (ME_GOAHEAD_SSL_KEY[0] == '\0' || ME_GOAHEAD_SSL_CERTIFICATE[0] == '\0')) {
            error("Missing required certificate and key");
        } else if (rc == MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED) {
            error("Server requires a client certificate");
        } else {
            char ebuf[256];
            mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
            error("%s: error -0x%x", ebuf, -rc);
        }
        sp->flags |= SOCKET_EOF;
        errno = EPROTO;
        return -1;

    } else if ((vrc = mbedtls_ssl_get_verify_result(&mb->ctx)) != 0) {
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
            if (!ME_GOAHEAD_SSL_VERIFY_ISSUER) {
                vrc = 0;
            }

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
    WebsSocket      *sp;
    MbedSocket      *mb;
    int             rc;

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
                trace(4, "mbedtls: read error -0x%", -rc);
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
    MbedSocket  *mb;
    WebsSocket  *sp;
    ssize       totalWritten;
    int         rc;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    mb = (MbedSocket*) wp->ssl;
    sp = socketPtr(wp->sid);

    if (sp->flags & SOCKET_HANDSHAKING) {
        if ((rc = mbedHandshake(wp)) <= 0) {
            return rc;
        }
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
                socketSetError(EAGAIN);
                return -1;
            }
            if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                trace(4, "mbedtls_ssl_write peer closed");
                return -1;
            } else {
                trace(4, "mbedtls_ssl_write failed rc %d", rc);
                return -1;
            }
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= rc;
        }
    } while (len > 0);

    socketHiddenData(socketPtr(wp->sid), mb->ctx.MBEDTLS_PRIVATE(in_left), SOCKET_WRITABLE);
    return totalWritten;
}


/*
    Convert string of IANA ciphers into a list of cipher codes
 */
static int *getCipherSuite(char *ciphers, int *len)
{
    char    *cipher, *next;
    cint    *cp;
    int     nciphers, i, *result, code;

    if (!ciphers || *ciphers == 0) {
        return 0;
    }
    for (nciphers = 0, cp = mbedtls_ssl_list_ciphersuites(); cp && *cp; cp++, nciphers++) { }
    result = walloc((nciphers + 1) * sizeof(int));

    next = ciphers = sclone(ciphers);
    for (i = 0; (cipher = stok(next, ":, \t", &next)) != 0; ) {
        replaceHyphen(cipher, '_', '-');
        if ((code = mbedtls_ssl_get_ciphersuite_id(cipher)) <= 0) {
            error("Unsupported cipher \"%s\"", cipher);
            continue;
        }
        result[i++] = code;
    }
    result[i] = 0;
    if (len) {
        *len = i;
    }
    wfree(ciphers);
    return result;
}


static int parseCert(mbedtls_x509_crt *cert, char *path)
{
    char    *buf;
    ssize   len;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read certificate %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        /* Looks PEM encoded so count the null in the length */
        len++;
    }
    if (mbedtls_x509_crt_parse(cert, (uchar*) buf, len) != 0) {
        memset(buf, 0, len);
        error("Unable to parse certificate %s", path);
        return -1;
    }
    memset(buf, 0, len);
    wfree(buf);
    return 0;
}


static int parseKey(mbedtls_pk_context *key, char *path)
{
    char    *buf;
    ssize   len;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read key %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        len++;
    }
    if (mbedtls_pk_parse_key(key, (uchar*) buf, len, NULL, 0, mbedtls_ctr_drbg_random, NULL) != 0) {
        memset(buf, 0, len);
        error("Unable to parse key %s", path);
        return -1;
    }
    memset(buf, 0, len);
    wfree(buf);
    return 0;
}


static int parseCrl(mbedtls_x509_crl *crl, char *path)
{
    char    *buf;
    ssize   len;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read crl %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        len++;
    }
    if (mbedtls_x509_crl_parse(crl, (uchar*) buf, len) != 0) {
        memset(buf, 0, len);
        error("Unable to parse crl %s", path);
        return -1;
    }
    memset(buf, 0, len);
    wfree(buf);
    return 0;
}


static void traceMbed(void *context, int level, cchar *file, int line, cchar *str)
{
    char    *buf;

    level += mbedLogLevel;
    if (level <= websGetLogLevel()) {
        buf = sclone((char*) str);
        buf[slen(buf) - 1] = '\0';
        trace(level, "%s", buf);
        wfree(buf);
    }
}


static void merror(int rc, char *fmt, ...)
{
    va_list     ap;
    char        ebuf[ME_MAX_BUFFER];

    va_start(ap, fmt);
    mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
    error("mbedtls", "mbedtls error: 0x%x %s %s", rc, sfmtv(fmt, ap), ebuf);
    va_end(ap);
}


static char *replaceHyphen(char *cipher, char from, char to)
{
    char    *cp;

    for (cp = cipher; *cp; cp++) {
        if (*cp == from) {
            *cp = to;
        }
    }
    return cipher;
}

#else
void mbedDummy() {}
#endif /* ME_COM_MBEDTLS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
