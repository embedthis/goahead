/*
    openssl.c - OpensSSL socket layer for GoAhead

    This is the interface between GoAhead and the OpenSSL stack.
    This code expects at least Openssl 1.1.1.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/************************************ Include *********************************/

#include    "goahead.h"

#if ME_COM_OPENSSL

#if ME_UNIX_LIKE
/*
    Mac OS X OpenSSL stack is deprecated. Suppress those warnings.
 */
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* Clashes with WinCrypt.h */
#undef OCSP_RESPONSE

#ifndef ME_GOAHEAD_SSL_HANDSHAKES
    #define ME_GOAHEAD_SSL_HANDSHAKES  0    /* Defaults to infinite */
#endif
#ifndef ME_GOAHEAD_SSL_RENEGOTIATE
    #define ME_GOAHEAD_SSL_RENEGOTIATE 1
#endif

/*
   Indent includes to bypass MakeMe dependencies
 */
 #include    <openssl/ssl.h>
 #include    <openssl/evp.h>
 #include    <openssl/rand.h>
 #include    <openssl/err.h>
 #include    <openssl/dh.h>

/************************************* Defines ********************************/
/*
    Default ciphers from Mozilla (https://wiki.mozilla.org/Security/Server_Side_TLS).
    See cipher mappings at: https://wiki.mozilla.org/Security/Server_Side_TLS#Cipher_names_correspondence_table

    Rationale:
 * TLSv1.3 ciphers are preferred and listed first.
 * TLSv1 and v2 ciphers are legacy and only for backward compatibility with legacy systems.
 * For TLSv2, AES256-GCM is prioritized above its 128 bits variant, and ChaCha20 because we assume that most modern
      devices support AESNI instructions and thus benefit from fast and constant time AES.
 * For TLSv2, We recommend ECDSA certificates with P256 as other curves may not be supported everywhere. RSA signatures
      on ECDSA certificates are permitted because very few CAs sign with ECDSA at the moment.
 * For TLSv2, DHE is removed entirely because it is slow in comparison with ECDHE, and all modern clients support
      elliptic curve key exchanges.
 * For TLSv2, SHA1 signature algorithm is removed in favor of SHA384 for AES256 and SHA256 for AES128.
 * Recommended RSA and DH parameter size: 2048 bits.
 */
#define OPENSSL_TLSV3_DEFAULT_CIPHERS "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256"
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    #define OPENSSL_DEFAULT_CIPHERS   OPENSSL_TLSV3_DEFAULT_CIPHERS \
            ":ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256"
#else
    #define OPENSSL_DEFAULT_CIPHERS   OPENSSL_TLSV3_DEFAULT_CIPHERS \
            ":ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA256:DHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA:ECDHE-RSA-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:AES256-GCM-SHA384:AES128-GCM-SHA256:AES256-SHA256:AES128-SHA256:AES256-SHA:AES128-SHA:DES-CBC3-SHA:HIGH:!aNULL:!eNULL:!EXPORT:!DES:!MD5:!PSK:!RC4:!SSLv3"
#endif

/*
    Map Iana names to OpenSSL names
 */
typedef struct CipherMap {
    int code;
    char *name;
    char *ossName;
} CipherMap;

static CipherMap cipherMap[] = {
    { 0x0004, "TLS_RSA_WITH_RC4_128_MD5", "RC4-MD5" },
    { 0x0005, "TLS_RSA_WITH_RC4_128_SHA", "RC4-SHA" },
    { 0x0007, "TLS_RSA_WITH_IDEA_CBC_SHA", "IDEA-CBC-SHA" },
    { 0x0009, "TLS_RSA_WITH_DES_CBC_SHA", "DES-CBC-SHA" },
    { 0x000A, "TLS_RSA_WITH_3DES_EDE_CBC_SHA", "DES-CBC3-SHA" },
    { 0x000C, "TLS_DH_DSS_WITH_DES_CBC_SHA", "DH-DSS-DES-CBC-SHA" },
    { 0x000D, "TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA", "DH-DSS-DES-CBC3-SHA" },
    { 0x000F, "TLS_DH_RSA_WITH_DES_CBC_SHA", "DH-RSA-DES-CBC-SHA" },
    { 0x0010, "TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA", "DH-RSA-DES-CBC3-SHA" },
    { 0x0012, "TLS_DHE_DSS_WITH_DES_CBC_SHA", "EDH-DSS-DES-CBC-SHA" },
    { 0x0013, "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA", "EDH-DSS-DES-CBC3-SHA" },
    { 0x0015, "TLS_DHE_RSA_WITH_DES_CBC_SHA", "EDH-RSA-DES-CBC-SHA" },
    { 0x0016, "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA", "EDH-RSA-DES-CBC3-SHA" },
    { 0x002F, "TLS_RSA_WITH_AES_128_CBC_SHA", "AES128-SHA" },
    { 0x0030, "TLS_DH_DSS_WITH_AES_128_CBC_SHA", "DH-DSS-AES128-SHA" },
    { 0x0031, "TLS_DH_RSA_WITH_AES_128_CBC_SHA", "DH-RSA-AES128-SHA" },
    { 0x0032, "TLS_DHE_DSS_WITH_AES_128_CBC_SHA", "DHE-DSS-AES128-SHA" },
    { 0x0033, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA", "DHE-RSA-AES128-SHA" },
    { 0x0035, "TLS_RSA_WITH_AES_256_CBC_SHA", "AES256-SHA" },
    { 0x0036, "TLS_DH_DSS_WITH_AES_256_CBC_SHA", "DH-DSS-AES256-SHA" },
    { 0x0037, "TLS_DH_RSA_WITH_AES_256_CBC_SHA", "DH-RSA-AES256-SHA" },
    { 0x0038, "TLS_DHE_DSS_WITH_AES_256_CBC_SHA", "DHE-DSS-AES256-SHA" },
    { 0x0039, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA", "DHE-RSA-AES256-SHA" },
    { 0x003C, "TLS_RSA_WITH_AES_128_CBC_SHA256", "AES128-SHA256" },
    { 0x003D, "TLS_RSA_WITH_AES_256_CBC_SHA256", "AES256-SHA256" },
    { 0x003E, "TLS_DH_DSS_WITH_AES_128_CBC_SHA256", "DH-DSS-AES128-SHA256" },
    { 0x003F, "TLS_DH_RSA_WITH_AES_128_CBC_SHA256", "DH-RSA-AES128-SHA256" },
    { 0x0040, "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256", "DHE-DSS-AES128-SHA256" },
    { 0x0041, "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA", "CAMELLIA128-SHA" },
    { 0x0042, "TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA", "DH-DSS-CAMELLIA128-SHA" },
    { 0x0043, "TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA", "DH-RSA-CAMELLIA128-SHA" },
    { 0x0044, "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA", "DHE-DSS-CAMELLIA128-SHA" },
    { 0x0045, "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA", "DHE-RSA-CAMELLIA128-SHA" },
    { 0x0067, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256", "DHE-RSA-AES128-SHA256" },
    { 0x0068, "TLS_DH_DSS_WITH_AES_256_CBC_SHA256", "DH-DSS-AES256-SHA256" },
    { 0x0069, "TLS_DH_RSA_WITH_AES_256_CBC_SHA256", "DH-RSA-AES256-SHA256" },
    { 0x006A, "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256", "DHE-DSS-AES256-SHA256" },
    { 0x006B, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256", "DHE-RSA-AES256-SHA256" },
    { 0x0084, "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA", "CAMELLIA256-SHA" },
    { 0x0085, "TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA", "DH-DSS-CAMELLIA256-SHA" },
    { 0x0086, "TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA", "DH-RSA-CAMELLIA256-SHA" },
    { 0x0087, "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA", "DHE-DSS-CAMELLIA256-SHA" },
    { 0x0088, "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA", "DHE-RSA-CAMELLIA256-SHA" },
    { 0x008A, "TLS_PSK_WITH_RC4_128_SHA", "PSK-RC4-SHA" },
    { 0x008B, "TLS_PSK_WITH_3DES_EDE_CBC_SHA", "PSK-3DES-EDE-CBC-SHA" },
    { 0x008C, "TLS_PSK_WITH_AES_128_CBC_SHA", "PSK-AES128-CBC-SHA" },
    { 0x008D, "TLS_PSK_WITH_AES_256_CBC_SHA", "PSK-AES256-CBC-SHA" },
    { 0x0096, "TLS_RSA_WITH_SEED_CBC_SHA", "SEED-SHA" },
    { 0x0097, "TLS_DH_DSS_WITH_SEED_CBC_SHA", "DH-DSS-SEED-SHA" },
    { 0x0098, "TLS_DH_RSA_WITH_SEED_CBC_SHA", "DH-RSA-SEED-SHA" },
    { 0x0099, "TLS_DHE_DSS_WITH_SEED_CBC_SHA", "DHE-DSS-SEED-SHA" },
    { 0x009A, "TLS_DHE_RSA_WITH_SEED_CBC_SHA", "DHE-RSA-SEED-SHA" },
    { 0x009C, "TLS_RSA_WITH_AES_128_GCM_SHA256", "AES128-GCM-SHA256" },
    { 0x009D, "TLS_RSA_WITH_AES_256_GCM_SHA384", "AES256-GCM-SHA384" },
    { 0x009E, "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256", "DHE-RSA-AES128-GCM-SHA256" },
    { 0x009F, "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384", "DHE-RSA-AES256-GCM-SHA384" },
    { 0x00A0, "TLS_DH_RSA_WITH_AES_128_GCM_SHA256", "DH-RSA-AES128-GCM-SHA256" },
    { 0x00A1, "TLS_DH_RSA_WITH_AES_256_GCM_SHA384", "DH-RSA-AES256-GCM-SHA384" },
    { 0x00A2, "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256", "DHE-DSS-AES128-GCM-SHA256" },
    { 0x00A3, "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384", "DHE-DSS-AES256-GCM-SHA384" },
    { 0x00A4, "TLS_DH_DSS_WITH_AES_128_GCM_SHA256", "DH-DSS-AES128-GCM-SHA256" },
    { 0x00A5, "TLS_DH_DSS_WITH_AES_256_GCM_SHA384", "DH-DSS-AES256-GCM-SHA384" },
    { 0xC002, "TLS_ECDH_ECDSA_WITH_RC4_128_SHA", "ECDH-ECDSA-RC4-SHA" },
    { 0xC003, "TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA", "ECDH-ECDSA-DES-CBC3-SHA" },
    { 0xC004, "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA", "ECDH-ECDSA-AES128-SHA" },
    { 0xC005, "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA", "ECDH-ECDSA-AES256-SHA" },
    { 0xC007, "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA", "ECDHE-ECDSA-RC4-SHA" },
    { 0xC008, "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA", "ECDHE-ECDSA-DES-CBC3-SHA" },
    { 0xC009, "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA", "ECDHE-ECDSA-AES128-SHA" },
    { 0xC00A, "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA", "ECDHE-ECDSA-AES256-SHA" },
    { 0xC00C, "TLS_ECDH_RSA_WITH_RC4_128_SHA", "ECDH-RSA-RC4-SHA" },
    { 0xC00D, "TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA", "ECDH-RSA-DES-CBC3-SHA" },
    { 0xC00E, "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA", "ECDH-RSA-AES128-SHA" },
    { 0xC00F, "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA", "ECDH-RSA-AES256-SHA" },
    { 0xC011, "TLS_ECDHE_RSA_WITH_RC4_128_SHA", "ECDHE-RSA-RC4-SHA" },
    { 0xC012, "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA", "ECDHE-RSA-DES-CBC3-SHA" },
    { 0xC013, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA", "ECDHE-RSA-AES128-SHA" },
    { 0xC014, "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA", "ECDHE-RSA-AES256-SHA" },
    { 0xC01A, "TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA", "SRP-3DES-EDE-CBC-SHA" },
    { 0xC01B, "TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA", "SRP-RSA-3DES-EDE-CBC-SHA" },
    { 0xC01C, "TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA", "SRP-DSS-3DES-EDE-CBC-SHA" },
    { 0xC01D, "TLS_SRP_SHA_WITH_AES_128_CBC_SHA", "SRP-AES-128-CBC-SHA" },
    { 0xC01E, "TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA", "SRP-RSA-AES-128-CBC-SHA" },
    { 0xC01F, "TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA", "SRP-DSS-AES-128-CBC-SHA" },
    { 0xC020, "TLS_SRP_SHA_WITH_AES_256_CBC_SHA", "SRP-AES-256-CBC-SHA" },
    { 0xC021, "TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA", "SRP-RSA-AES-256-CBC-SHA" },
    { 0xC022, "TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA", "SRP-DSS-AES-256-CBC-SHA" },
    { 0xC023, "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256", "ECDHE-ECDSA-AES128-SHA256" },
    { 0xC024, "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384", "ECDHE-ECDSA-AES256-SHA384" },
    { 0xC025, "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256", "ECDH-ECDSA-AES128-SHA256" },
    { 0xC026, "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384", "ECDH-ECDSA-AES256-SHA384" },
    { 0xC027, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256", "ECDHE-RSA-AES128-SHA256" },
    { 0xC028, "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384", "ECDHE-RSA-AES256-SHA384" },
    { 0xC029, "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256", "ECDH-RSA-AES128-SHA256" },
    { 0xC02A, "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384", "ECDH-RSA-AES256-SHA384" },
    { 0xC02B, "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256", "ECDHE-ECDSA-AES128-GCM-SHA256" },
    { 0xC02C, "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384", "ECDHE-ECDSA-AES256-GCM-SHA384" },
    { 0xC02D, "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256", "ECDH-ECDSA-AES128-GCM-SHA256" },
    { 0xC02E, "TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384", "ECDH-ECDSA-AES256-GCM-SHA384" },
    { 0xC02F, "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256", "ECDHE-RSA-AES128-GCM-SHA256" },
    { 0xC030, "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384", "ECDHE-RSA-AES256-GCM-SHA384" },
    { 0xC031, "TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256", "ECDH-RSA-AES128-GCM-SHA256" },
    { 0xC032, "TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384", "ECDH-RSA-AES256-GCM-SHA384" },
    { 0x0000, 0 },
};

/*
    OpenSSL context (singleton)
 */
static SSL_CTX *sslctx = NULL;

typedef struct RandBuf {
    time_t now;
    int pid;
} RandBuf;

#define VERIFY_DEPTH             10

/*
    Used for OpenSSL versions < 1.0.2
 */
#ifndef ME_GOAHEAD_SSL_CURVE
    #define ME_GOAHEAD_SSL_CURVE "prime256v1"
#endif

/*
    DH parameters
 */
static DH  *dhKey;
static int maxHandshakes;

/************************************ Forwards ********************************/

static DH *dhcallback(SSL *handle, int is_export, int keylength);
static DH *getDhKey();
static char *mapCipherNames(char *ciphers);
static int  sslSetCertFile(char *certFile);
static int  sslSetKeyFile(char *keyFile);
static int  verifyClientCertificate(int ok, X509_STORE_CTX *ctx);
static void infoCallback(const SSL *ssl, int where, int rc);

/************************************** Code **********************************/
/*
    Open the SSL module
 */
PUBLIC int sslOpen()
{
    RandBuf    randBuf;
    X509_STORE *store;
    uchar      resume[16];
    char       *ciphers;
    int        rc;

    trace(7, "Initializing SSL");

    randBuf.now = time(0);
    randBuf.pid = getpid();
    RAND_seed((void*) &randBuf, sizeof(randBuf));
#if ME_UNIX_LIKE
    trace(6, "OpenSsl: Before calling RAND_load_file");
    RAND_load_file("/dev/urandom", 256);
    trace(6, "OpenSsl: After calling RAND_load_file");
#endif
#if !ME_WIN_LIKE
    OpenSSL_add_all_algorithms();
#endif
    SSL_library_init();
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();

    if ((sslctx = SSL_CTX_new(TLS_server_method())) == 0) {
        error("Unable to create SSL context");
        return -1;
    }

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    /* Enforce minimum TLS version 1.2 on modern OpenSSL */
    if (!SSL_CTX_set_min_proto_version(sslctx, TLS1_2_VERSION)) {
        error("Unable to set minimum TLS version to 1.2");
        sslClose();
        return -1;
    }
#endif

    /*
          Set the server certificate and key files
     */
    if (*ME_GOAHEAD_SSL_KEY && sslSetKeyFile(ME_GOAHEAD_SSL_KEY) < 0) {
        sslClose();
        return -1;
    }
    if (*ME_GOAHEAD_SSL_CERTIFICATE && sslSetCertFile(ME_GOAHEAD_SSL_CERTIFICATE) < 0) {
        sslClose();
        return -1;
    }

    if (ME_GOAHEAD_SSL_VERIFY_PEER) {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyClientCertificate);
        SSL_CTX_set_verify_depth(sslctx, VERIFY_DEPTH);
    } else {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyClientCertificate);
    }
    /*
          Set the client certificate verification locations
     */
    if (ME_GOAHEAD_SSL_AUTHORITY && *ME_GOAHEAD_SSL_AUTHORITY) {
        if ((!SSL_CTX_load_verify_locations(sslctx, ME_GOAHEAD_SSL_AUTHORITY, NULL)) ||
            (!SSL_CTX_set_default_verify_paths(sslctx))) {
            error("Unable to read cert verification locations");
            sslClose();
            return -1;
        }
        /*
            Define the list of CA certificates to send to the client before they send their client
            certificate for validation
         */
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(ME_GOAHEAD_SSL_AUTHORITY));
    }
    if (ME_GOAHEAD_SSL_REVOKE && *ME_GOAHEAD_SSL_REVOKE) {
        store = SSL_CTX_get_cert_store(sslctx);
        if (!X509_STORE_load_locations(store, ME_GOAHEAD_SSL_REVOKE, 0)) {
            error("Cannot load certificate revoke list: %s", ME_GOAHEAD_SSL_REVOKE);
            sslClose();
            return -1;
        }
    }

    /*
        Configure DH parameters
     */
    dhKey = getDhKey();
    SSL_CTX_set_tmp_dh_callback(sslctx, dhcallback);

    /*
        Configure cipher suite
     */
    if (ME_GOAHEAD_SSL_CIPHERS && *ME_GOAHEAD_SSL_CIPHERS) {
        ciphers = ME_GOAHEAD_SSL_CIPHERS;
    } else {
        ciphers = OPENSSL_DEFAULT_CIPHERS;
    }
    ciphers = mapCipherNames(ciphers);
    trace(5, "Using OpenSSL ciphers: %s", ciphers);
#if OPENSSL_VERSION_NUMBER >= 0x1010100L
    /*
        Set TLSv1.3 ciphers and <1.2 ciphers via (cipher_list).
        ciphersuites() fails if any cipher fails, cipher_list fails if all ciphers fail.
        This fails if we can't get a 1.3 or 1.2 cipher suite configured
     */
    rc = 1;
    if (SSL_CTX_set_ciphersuites(sslctx, ciphers) != 1 && SSL_CTX_set_cipher_list(sslctx, ciphers) != 1) {
        rc = 0;
    }
#else
    rc = SSL_CTX_set_cipher_list(sslctx, ciphers);
#endif
    if (rc != 1) {
        error("Unable to set cipher list \"%s\"", ciphers);
        sslClose();
        wfree(ciphers);
        return -1;
    }
    wfree(ciphers);

    /*
        Define default OpenSSL options
     */
    SSL_CTX_set_options(sslctx, SSL_OP_ALL);

    /*
        Ensure we generate a new private key for each connection
     */
    SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_DH_USE);

    /*
        Define a session reuse context
     */
    RAND_bytes(resume, sizeof(resume));
    SSL_CTX_set_session_id_context(sslctx, resume, sizeof(resume));

    /*
        Elliptic Curve initialization
     */
#if SSL_OP_SINGLE_ECDH_USE
    #ifdef SSL_CTX_set_ecdh_auto
    SSL_CTX_set_ecdh_auto(sslctx, 1);
    #else
    {
        EC_KEY *ecdh;
        cchar  *name;
        int    nid;

        name = ME_GOAHEAD_SSL_CURVE;
        if ((nid = OBJ_sn2nid(name)) == 0) {
            error("Unknown curve name \"%s\"", name);
            sslClose();
            return -1;
        }
        if ((ecdh = EC_KEY_new_by_curve_name(nid)) == 0) {
            error("Unable to create curve \"%s\"", name);
            sslClose();
            return -1;
        }
        SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_ECDH_USE);
        SSL_CTX_set_tmp_ecdh(sslctx, ecdh);
        EC_KEY_free(ecdh);
    }
    #endif
#endif

    SSL_CTX_set_mode(sslctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_AUTO_RETRY | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    SSL_CTX_set_options(sslctx, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(sslctx, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
    SSL_CTX_set_options(sslctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
#endif

    /*
        Select the required protocols
        Disable both SSLv2, SSLv3 and TLSv1.0 by default - they are insecure
     */
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv3);
#if defined(SSL_OP_NO_TLSv1)
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TLSv1);
#endif

#if defined(SSL_OP_NO_TLSv1_1)
    /* Disable TLS 1.1 by default on older OpenSSL */
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TLSv1_1);
#endif
#if defined(SSL_OP_NO_TLSv1_2) && ME_GOAHEAD_SSL_NO_V1_2
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TLSv1_2);
#endif
#if defined(SSL_OP_NO_TLSv1_3) && ME_GOAHEAD_SSL_NO_V1_3
    SSL_CTX_set_options(sslctx, SSL_OP_NO_TLSv1_3);
#endif

#if defined(SSL_OP_NO_TICKET)
    /*
        Ticket based session reuse is enabled by default
     */
    #if defined(ME_GOAHEAD_SSL_TICKET)
    if (ME_GOAHEAD_SSL_TICKET) {
        SSL_CTX_clear_options(sslctx, SSL_OP_NO_TICKET);
    } else {
        SSL_CTX_set_options(sslctx, SSL_OP_NO_TICKET);
    }
    #else
    SSL_CTX_clear_options(sslctx, SSL_OP_NO_TICKET);
    #endif
#endif

#if defined(SSL_OP_NO_COMPRESSION)
    /*
        CRIME attack targets compression
     */
    SSL_CTX_set_options(sslctx, SSL_OP_NO_COMPRESSION);
#endif

#ifdef SSL_OP_NO_RENEGOTIATION
    /* Disallow renegotiation entirely */
    SSL_CTX_set_options(sslctx, SSL_OP_NO_RENEGOTIATION);
#endif

#if defined(ME_GOAHEAD_SSL_HANDSHAKES)
    maxHandshakes = ME_GOAHEAD_SSL_HANDSHAKES;
#endif

#if defined(SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)
    /*
        Disables a countermeasure against a SSL 3.0/TLS 1.0 protocol vulnerability affecting CBC ciphers.
        Defaults to true.
     */
    #if defined(ME_GOAHEAD_SSL_EMPTY_FRAGMENTS)
    if (ME_GOAHEAD_SSL_EMPTY_FRAGMENTS) {
        /* SSL_OP_ALL disables empty fragments. Only needed for ancient browsers like IE-6 on SSL-3.0/TLS-1.0 */
        SSL_CTX_clear_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    } else {
        SSL_CTX_set_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    }
    #else
    SSL_CTX_set_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    #endif
#endif

#if defined(ME_GOAHEAD_SSL_CACHE)
    /*
        Set the number of sessions supported. Default in OpenSSL is 20K.
     */
    SSL_CTX_sess_set_cache_size(sslctx, ME_GOAHEAD_SSL_CACHE);
#else
    SSL_CTX_sess_set_cache_size(sslctx, 256);
#endif
    return 0;
}


/*
    Close the SSL module
 */
PUBLIC void sslClose()
{
    if (sslctx != NULL) {
        SSL_CTX_free(sslctx);
        sslctx = NULL;
        if (dhKey) {
            DH_free(dhKey);
            dhKey = NULL;
        }
    }
}


/*
    Upgrade a socket to use SSL
 */
PUBLIC int sslUpgrade(Webs *wp)
{
    WebsSocket *sptr;
    BIO        *bio;

    assert(wp);

    sptr = socketPtr(wp->sid);
    if ((wp->ssl = SSL_new(sslctx)) == 0) {
        return -1;
    }
    /*
        Create a socket bio. We don't use the BIO except as storage for the fd.
     */
    if ((bio = BIO_new_socket((int) sptr->sock, BIO_NOCLOSE)) == 0) {
        return -1;
    }
    SSL_set_bio(wp->ssl, bio, bio);
    SSL_set_accept_state(wp->ssl);
    SSL_set_app_data(wp->ssl, (void*) wp);
    if (ME_GOAHEAD_SSL_HANDSHAKES) {
        SSL_CTX_set_info_callback(sslctx, infoCallback);
    }
    return 0;
}

static void infoCallback(const SSL *ssl, int where, int rc)
{
    Webs       *wp;
    WebsSocket *sp;

    if (where & SSL_CB_HANDSHAKE_START) {
        if ((wp = (Webs*) SSL_get_app_data(ssl)) == 0) {
            return;
        }
        if ((sp = socketPtr(wp->sid)) != 0) {
            sp->handshakes++;
        }
    }
}


PUBLIC void sslFree(Webs *wp)
{
    if (wp->ssl) {
        SSL_set_shutdown(wp->ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        SSL_free(wp->ssl);
        wp->ssl = 0;
    }
}


/*
    Return the number of bytes read. Return -1 on errors and EOF. Distinguish EOF via mprIsSocketEof.
    If non-blocking, may return zero if no data or still handshaking.
 */
PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket *sp;
    char       ebuf[ME_GOAHEAD_LIMIT_STRING];
    ulong      serror;
    int        rc, err, retries, i;

    if (wp->ssl == 0 || len <= 0) {
        return -1;
    }
    /*
        Limit retries on WANT_READ. If non-blocking and no data, then this can spin forever.
     */
    sp = socketPtr(wp->sid);
    retries = 5;
    for (i = 0; i < retries; i++) {
        rc = SSL_read(wp->ssl, buf, (int) len);
        if (rc <= 0) {
            err = SSL_get_error(wp->ssl, rc);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_CONNECT || err == SSL_ERROR_WANT_ACCEPT) {
                continue;
            }
            if (err != SSL_ERROR_ZERO_RETURN) {
                serror = ERR_get_error();
                ERR_error_string_n(serror, ebuf, sizeof(ebuf) - 1);
                trace(5, "SSL_read %s", ebuf);
            }
        }
        break;
    }
    if (maxHandshakes && sp->handshakes > maxHandshakes) {
        error("TLS renegotiation attack");
        rc = -1;
        sp->flags |= SOCKET_EOF;
        return -1;
    }
    if (rc <= 0) {
        err = SSL_get_error(wp->ssl, rc);
        if (err == SSL_ERROR_WANT_READ) {
            rc = 0;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            rc = 0;
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            sp->flags |= SOCKET_EOF;
            rc = -1;
        } else if (err == SSL_ERROR_SYSCALL) {
            sp->flags |= SOCKET_EOF;
            rc = -1;
        } else {
            serror = ERR_get_error();
            ERR_error_string_n(serror, ebuf, sizeof(ebuf) - 1);
            trace(4, "OpenSSL: connection with protocol error: %s", ebuf);
            rc = -1;
            sp->flags |= SOCKET_EOF;
        }
    } else if (SSL_pending(wp->ssl) > 0) {
        socketHiddenData(sp, SSL_pending(wp->ssl), SOCKET_READABLE);
    }
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    WebsSocket *sp;
    ssize      totalWritten;
    int        err, rc;

    if (wp->ssl == 0 || len <= 0) {
        return -1;
    }
    sp = socketPtr(wp->sid);
    totalWritten = 0;
    err = 0;
    ERR_clear_error();

    do {
        rc = SSL_write(wp->ssl, buf, (int) len);
        trace(7, "OpenSSL: written %d, requested len %d", rc, len);
        if (rc <= 0) {
            err = SSL_get_error(wp->ssl, rc);
            if (err == SSL_ERROR_NONE || err == SSL_ERROR_WANT_WRITE) {
                break;
            }
            trace(7, "OpenSSL: error %d", err);
            return -1;
        } else if (maxHandshakes && sp->handshakes > maxHandshakes) {
            error("TLS renegotiation attack");
            rc = -1;
            sp->flags |= SOCKET_EOF;
            return -1;
        }
        totalWritten += rc;
        buf = (void*) ((char*) buf + rc);
        len -= rc;
        trace(7, "OpenSSL: write: len %d, written %d, total %d", len, rc, totalWritten);
    } while (len > 0);

    if (totalWritten == 0 && err == SSL_ERROR_WANT_WRITE) {
        socketSetError(EAGAIN);
        return -1;
    }
    return totalWritten;
}


/*
    Set certificate file for SSL context
 */
static int sslSetCertFile(char *certFile)
{
    X509 *cert;
    BIO  *bio;
    char *buf;
    int  rc;

    assert(sslctx);
    assert(certFile);

    rc = -1;
    bio = 0;
    buf = 0;
    cert = 0;

    if (sslctx == NULL) {
        return rc;
    }
    if ((buf = websReadWholeFile(certFile)) == 0) {
        error("Unable to read certificate %s", certFile);

    } else if ((bio = BIO_new_mem_buf(buf, -1)) == 0) {
        error("Unable to allocate memory for certificate %s", certFile);

    } else if ((cert = PEM_read_bio_X509(bio, NULL, 0, NULL)) == 0) {
        error("Unable to parse certificate %s", certFile);

    } else if (SSL_CTX_use_certificate(sslctx, cert) != 1) {
        error("Unable to use certificate %s", certFile);

    } else if (!SSL_CTX_check_private_key(sslctx)) {
        error("Unable to check certificate key %s", certFile);

    } else {
        rc = 0;
    }
    wfree(buf);
    if (bio) {
        BIO_free(bio);
    }
    if (cert) {
        X509_free(cert);
    }
    return rc;
}


/*
      Set key file for SSL context
 */
static int sslSetKeyFile(char *keyFile)
{
    RSA  *key;
    BIO  *bio;
    char *buf;
    int  rc;

    assert(sslctx);
    assert(keyFile);

    key = 0;
    bio = 0;
    buf = 0;
    rc = -1;

    if (sslctx == NULL) {
        ;

    } else if ((buf = websReadWholeFile(keyFile)) == 0) {
        error("Unable to read certificate %s", keyFile);

    } else if ((bio = BIO_new_mem_buf(buf, -1)) == 0) {
        error("Unable to allocate memory for key %s", keyFile);

    } else if ((key = PEM_read_bio_RSAPrivateKey(bio, NULL, 0, NULL)) == 0) {
        error("Unable to parse key %s", keyFile);

    } else if (SSL_CTX_use_RSAPrivateKey(sslctx, key) != 1) {
        error("Unable to use key %s", keyFile);

    } else {
        rc = 0;
    }
    wfree(buf);
    if (bio) {
        BIO_free(bio);
    }
    if (key) {
        RSA_free(key);
    }
    return rc;
}


static int verifyClientCertificate(int ok, X509_STORE_CTX *xContext)
{
    X509 *cert;
    char subject[260], issuer[260], peer[260];
    int  error, depth;

    subject[0] = issuer[0] = '\0';

    cert = X509_STORE_CTX_get_current_cert(xContext);
    error = X509_STORE_CTX_get_error(xContext);
    depth = X509_STORE_CTX_get_error_depth(xContext);

    ok = 1;
    if (X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) - 1) < 0) {
        ok = 0;
    }
    if (X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) - 1) < 0) {
        ok = 0;
    }
    if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, peer, sizeof(peer) - 1) < 0) {
        ok = 0;
    }
    if (ok && VERIFY_DEPTH < depth) {
        if (error == 0) {
            error = X509_V_ERR_CERT_CHAIN_TOO_LONG;
        }
    }
    switch (error) {
    case X509_V_OK:
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
            logmsg(3, "Self-signed certificate");
            ok = 0;
        }

    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
            logmsg(3, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
            logmsg(3, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_INVALID_CA:
    default:
        logmsg(3, "Certificate verification error %d", error);
        ok = 0;
        break;
    }
    if (ok) {
        trace(3, "OpenSSL: Certificate verified: subject %s", subject);
    } else {
        trace(1, "OpenSSL: Certification failed: subject %s (more trace at level 4)", subject);
        trace(4, "OpenSSL: Error: %d: %s", error, X509_verify_cert_error_string(error));
    }
    trace(4, "OpenSSL: Issuer: %s", issuer);
    trace(4, "OpenSSL: Peer: %s", peer);
    return ok;
}



/*
    Map iana names to OpenSSL names so users can provide IANA names as well as OpenSSL cipher names
 */
static char *mapCipherNames(char *ciphers)
{
    WebsBuf   buf;
    CipherMap *cp;
    char      *cipher, *next, *str;

    if (!ciphers || *ciphers == 0) {
        return 0;
    }
    bufCreate(&buf, 0, 0);
    ciphers = sclone(ciphers);
    for (next = ciphers; (cipher = stok(next, ":, \t", &next)) != 0; ) {
        for (cp = cipherMap; cp->name; cp++) {
            if (smatch(cp->name, cipher)) {
                bufPut(&buf, "%s:", cp->ossName);
                break;
            }
        }
        if (cp->name == 0) {
            bufPut(&buf, "%s:", cipher);
        }
    }
    wfree(ciphers);
    str = sclone(bufStart(&buf));
    bufFree(&buf);
    return str;
}


/*
    Get the DH parameters
 */

static DH *getDhKey()
{
    static unsigned char dh2048_p[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
        0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1, 0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
        0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
        0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
        0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45, 0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
        0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
        0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
        0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D, 0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
        0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
        0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
        0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D, 0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
        0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
        0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
        0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9, 0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
        0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
        0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    static unsigned char dh2048_g[] = {
        0x02,
    };
    DH                   *dh;

    if ((dh = DH_new()) == 0) {
        return 0;
    }
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    {
        BIGNUM *p, *g;

        p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
        g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
        if (!DH_set0_pqg(dh, p, NULL, g)) {
            BN_free(p);
            BN_free(g);
            DH_free(dh);
            return 0;
        }
    }
#else
    dh->p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
    dh->g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
    if ((dh->p == 0) || (dh->g == 0)) {
        DH_free(dh);
        return 0;
    }
#endif
    return dh;
}


/*
    Set the ephemeral DH key
 */
static DH *dhcallback(SSL *handle, int isExport, int keyLength)
{
    return dhKey;
}

#else
void opensslDummy()
{
}
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
