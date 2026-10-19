/*
    openssl.c - OpensSSL socket layer for GoAhead

    This is the interface between GoAhead and the OpenSSL stack.
    This code requires OpenSSL 3.0 or later.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/************************************ Include *********************************/

#include    "goahead.h"

#if ME_COM_OPENSSL

#if ME_UNIX_LIKE
/*
    The DH parameter API used for TLS 1.2 DHE is deprecated in OpenSSL 3. Suppress those warnings.
 */
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#if _MSC_VER
/*
    Suppress OpenSSL 3.0 deprecated-declaration warnings for legacy DH/RSA APIs used below.
 */
    #pragma warning(disable : 4996)
#endif

/* Clashes with WinCrypt.h */
#undef OCSP_RESPONSE


 #include    <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    #error "GoAhead requires OpenSSL 3.0 or later. See SECURITY-DEPLOYMENT.md for the supported TLS library versions."
#endif

 #include    <openssl/ssl.h>
 #include    <openssl/evp.h>
 #include    <openssl/rand.h>
 #include    <openssl/err.h>
 #include    <openssl/dh.h>
 #include    <openssl/core_names.h>

/************************************* Defines ********************************/
/*
    Ciphers never permitted for TLS 1.2 and earlier, whatever the configured list says
 */
#define OPENSSL_CIPHER_FLOOR "!aNULL:!eNULL:!EXPORT:!DES:!3DES:!RC4:!MD5:!kRSA"

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
    { 0x0000, 0, 0 },
};

/*
    OpenSSL context (singleton)
 */
static SSL_CTX *sslctx = NULL;

typedef struct RandBuf {
    time_t now;
    int pid;
} RandBuf;


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

/*
    Runtime-settable cert/key paths. Override the compile-time ME_GOAHEAD_SSL_CERTIFICATE /
    ME_GOAHEAD_SSL_KEY macros when websSetSslCertFile / websSetSslKeyFile are called
    before sslOpen(). Needed by test drivers that ship their own cert alongside the binary.
 */
static char *sslCertPath;
static char *sslKeyPath;

/*
    Session ticket keys, the current one first and the one it replaced second. See ticketKeyCallback.
 */
typedef struct TicketKey {
    uchar name[16];
    uchar cipher[32];
    uchar mac[32];
    time_t created;                     /* Zero for no key */
} TicketKey;

static TicketKey ticketKeys[2];

/************************************ Forwards ********************************/

static DH *dhcallback(SSL *handle, int is_export, int keylength);
static DH *getDhKey();
static char *mapCipherNames(char *ciphers);
static int  setCiphers(cchar *ciphers);
static int  sslSetCertFile(char *certFile);
static int  sslSetKeyFile(char *keyFile);
static int  verifyClientCertificate(int ok, X509_STORE_CTX *ctx);
static int  noPassphrase(char *buf, int size, int rwflag, void *arg);
static void infoCallback(const SSL *ssl, int where, int rc);
static int  rotateTicketKeys(void);
static int  ticketKeyCallback(SSL *ssl, uchar *name, uchar *iv, EVP_CIPHER_CTX *ctx, EVP_MAC_CTX *hctx, int enc);

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

    /*
        Apply the configured protocol versions. TLS 1.1 and earlier are never offered.
     */
    if (!SSL_CTX_set_min_proto_version(sslctx, ME_GOAHEAD_SSL_NO_V1_2 ? TLS1_3_VERSION : TLS1_2_VERSION) ||
        !SSL_CTX_set_max_proto_version(sslctx, ME_GOAHEAD_SSL_NO_V1_3 ? TLS1_2_VERSION : TLS1_3_VERSION)) {
        error("Unable to set the TLS protocol versions");
        sslClose();
        return -1;
    }

    /*
          Set the server certificate and key files. Prefer runtime-provided paths
          (set via websSetSsl{Cert,Key}File) over the compile-time defaults.
     */
    {
        char *keyPath = sslKeyPath ? sslKeyPath : (char*) ME_GOAHEAD_SSL_KEY;
        char *certPath = sslCertPath ? sslCertPath : (char*) ME_GOAHEAD_SSL_CERTIFICATE;

        if (keyPath && *keyPath && sslSetKeyFile(keyPath) < 0) {
            sslClose();
            return -1;
        }
        if (certPath && *certPath && sslSetCertFile(certPath) < 0) {
            sslClose();
            return -1;
        }
    }

    if (ME_GOAHEAD_SSL_VERIFY_PEER) {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyClientCertificate);
        SSL_CTX_set_verify_depth(sslctx, ME_GOAHEAD_SSL_VERIFY_DEPTH);
    } else {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyClientCertificate);
    }
    /*
        Verify client certificates only against the configured authority, never the system public CA store. Without
        an authority, refuse issuer verification.
     */
    if (ME_GOAHEAD_SSL_AUTHORITY && *ME_GOAHEAD_SSL_AUTHORITY) {
        if (!SSL_CTX_load_verify_locations(sslctx, ME_GOAHEAD_SSL_AUTHORITY, NULL)) {
            error("Unable to read cert verification locations");
            sslClose();
            return -1;
        }
        /*
            Define the list of CA certificates to send to the client before they send
            their client certificate for validation
         */
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(ME_GOAHEAD_SSL_AUTHORITY));
    } else if (ME_GOAHEAD_SSL_VERIFY_PEER && ME_GOAHEAD_SSL_VERIFY_ISSUER) {
        error("Client certificate issuers are verified but ME_GOAHEAD_SSL_AUTHORITY is not set: every client "
              "certificate will be refused");
    }
    if (ME_GOAHEAD_SSL_REVOKE && *ME_GOAHEAD_SSL_REVOKE) {
        store = SSL_CTX_get_cert_store(sslctx);
        if (!X509_STORE_load_locations(store, ME_GOAHEAD_SSL_REVOKE, 0)) {
            error("Cannot load certificate revoke list: %s", ME_GOAHEAD_SSL_REVOKE);
            sslClose();
            return -1;
        }
        /*
            Check revocation for the full chain; require a CRL for each CA.
         */
        X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
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
        ciphers = ME_GOAHEAD_SSL_DEFAULT_CIPHERS;
    }
    ciphers = mapCipherNames(ciphers);
    trace(5, "Using OpenSSL ciphers: %s", ciphers);
    rc = setCiphers(ciphers);
    wfree(ciphers);
    if (rc < 0) {
        sslClose();
        return -1;
    }

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
    if (RAND_bytes(resume, sizeof(resume)) != 1) {
        error("Cannot get random bytes for the TLS session context");
        return -1;
    }
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
    SSL_CTX_set_timeout(sslctx, ME_GOAHEAD_SSL_TIMEOUT);
    if (rotateTicketKeys() < 0) {
        error("Cannot get random bytes for the session ticket key");
        return -1;
    }
    SSL_CTX_set_tlsext_ticket_key_evp_cb(sslctx, ticketKeyCallback);

    /*
        Disable resumption with client certificates so expiry, revocation, and chain checks run each time.
     */
    if (ME_GOAHEAD_SSL_VERIFY_PEER) {
        SSL_CTX_set_options(sslctx, SSL_OP_NO_TICKET);
        SSL_CTX_set_session_cache_mode(sslctx, SSL_SESS_CACHE_OFF);
        SSL_CTX_set_num_tickets(sslctx, 0);
    }

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
    OPENSSL_cleanse(ticketKeys, sizeof(ticketKeys));
    wfree(sslCertPath);
    sslCertPath = NULL;
    wfree(sslKeyPath);
    sslKeyPath = NULL;
}


PUBLIC bool sslHasCertificate(void)
{
    return sslctx && SSL_CTX_get0_certificate(sslctx) && SSL_CTX_get0_privatekey(sslctx);
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
    WebsSocket *sp;

    if (wp->ssl) {
        //  Tell the peer the stream is complete, unless the handshake did not finish or the connection failed
        sp = (wp->sid >= 0) ? socketPtr(wp->sid) : NULL;
        if (sp && !(sp->flags & SOCKET_EOF) && SSL_is_init_finished(wp->ssl)) {
            SSL_shutdown(wp->ssl);
            ERR_clear_error();
        }
        //  Marks the session resumable
        SSL_set_shutdown(wp->ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        SSL_free(wp->ssl);
        wp->ssl = 0;
    }
}


/*
    Return the number of bytes read. Return -1 on errors and EOF. Distinguish EOF via socketEof().
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
    } else {
        //  Report pending bytes after every read to clear the buffered-input mark when drained.
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
    if (!SSL_is_init_finished(wp->ssl)) {
        //  SSL_write would continue the handshake, reading from the peer inside a blocking flush
        sp->flags |= SOCKET_EOF;
        socketSetError(ECONNRESET);
        return -1;
    }
    totalWritten = 0;
    err = 0;
    ERR_clear_error();

    do {
        rc = SSL_write(wp->ssl, buf, (int) len);
        trace(7, "OpenSSL: written %d, requested len %zd", rc, len);
        if (rc <= 0) {
            err = SSL_get_error(wp->ssl, rc);
            if (err == SSL_ERROR_NONE || err == SSL_ERROR_WANT_WRITE) {
                break;
            }
            trace(7, "OpenSSL: error %d", err);
            /* Set a hard socket error so a stale EAGAIN cannot cause retries. */
            sp->flags |= SOCKET_EOF;
            socketSetError(ECONNRESET);
            return -1;
        } else if (maxHandshakes && sp->handshakes > maxHandshakes) {
            error("TLS renegotiation attack");
            sp->flags |= SOCKET_EOF;
            socketSetError(ECONNRESET);
            return -1;
        }
        totalWritten += rc;
        buf = (void*) ((char*) buf + rc);
        len -= rc;
        trace(7, "OpenSSL: write: len %zd, written %d, total %zd", len, rc, totalWritten);
    } while (len > 0);

    if (totalWritten == 0 && err == SSL_ERROR_WANT_WRITE) {
        socketSetError(EAGAIN);
        return -1;
    }
    return totalWritten;
}


/*
    Load the leaf and following intermediate certificates for the server chain.
 */
static int sslSetCertFile(char *certFile)
{
    X509          *cert, *extra;
    BIO           *bio;
    char          *buf;
    unsigned long err;
    int           rc;

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

    } else if ((cert = PEM_read_bio_X509(bio, NULL, noPassphrase, NULL)) == 0) {
        error("Unable to parse certificate %s", certFile);

    } else if (SSL_CTX_use_certificate(sslctx, cert) != 1) {
        error("Unable to use certificate %s", certFile);

    } else if (!SSL_CTX_check_private_key(sslctx)) {
        error("Unable to check certificate key %s", certFile);

    } else {
        rc = 0;
        SSL_CTX_clear_extra_chain_certs(sslctx);
        while ((extra = PEM_read_bio_X509(bio, NULL, noPassphrase, NULL)) != 0) {
            if (SSL_CTX_add_extra_chain_cert(sslctx, extra) != 1) {
                error("Unable to use chain certificate in %s", certFile);
                X509_free(extra);
                rc = -1;
                break;
            }
        }
        /*
            Only no-start-line at EOF is normal; report other chain parse failures.
         */
        err = ERR_peek_last_error();
        if (rc == 0 && err && !(ERR_GET_LIB(err) == ERR_LIB_PEM && ERR_GET_REASON(err) == PEM_R_NO_START_LINE)) {
            error("Unable to parse chain certificate in %s", certFile);
            rc = -1;
        }
        ERR_clear_error();
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
    Set the private key file for the SSL context. Any key type OpenSSL supports: RSA, EC or Ed25519.
 */
static int sslSetKeyFile(char *keyFile)
{
    EVP_PKEY *key;
    BIO      *bio;
    char     *buf;
    bool     encrypted;
    int      rc;

    assert(sslctx);
    assert(keyFile);

    key = 0;
    bio = 0;
    buf = 0;
    encrypted = 0;
    rc = -1;

    if (sslctx == NULL) {
        ;

    } else if ((buf = websReadWholeFile(keyFile)) == 0) {
        error("Unable to read key %s", keyFile);

    } else if ((bio = BIO_new_mem_buf(buf, -1)) == 0) {
        error("Unable to allocate memory for key %s", keyFile);

    } else if ((key = PEM_read_bio_PrivateKey(bio, NULL, noPassphrase, &encrypted)) == 0) {
        if (encrypted) {
            error("Key %s is encrypted. GoAhead requires an unencrypted private key", keyFile);
        } else {
            error("Unable to parse key %s", keyFile);
        }

    } else if (SSL_CTX_use_PrivateKey(sslctx, key) != 1) {
        error("Unable to use key %s", keyFile);

    } else {
        rc = 0;
    }
    wfree(buf);
    if (bio) {
        BIO_free(bio);
    }
    if (key) {
        EVP_PKEY_free(key);
    }
    return rc;
}


/*
    Refuse to prompt on the console for a passphrase. Record in arg, when given, that one was asked for.
 */
static int noPassphrase(char *buf, int size, int rwflag, void *arg)
{
    if (arg) {
        *(bool*) arg = 1;
    }
    return -1;
}


/*
    Make a new session ticket key and keep the current one as the previous.
 */
static int rotateTicketKeys(void)
{
    ticketKeys[1] = ticketKeys[0];
    if (RAND_bytes(ticketKeys[0].name, sizeof(ticketKeys[0].name)) != 1 ||
        RAND_bytes(ticketKeys[0].cipher, sizeof(ticketKeys[0].cipher)) != 1 ||
        RAND_bytes(ticketKeys[0].mac, sizeof(ticketKeys[0].mac)) != 1) {
        OPENSSL_cleanse(&ticketKeys[0], sizeof(ticketKeys[0]));
        return -1;
    }
    ticketKeys[0].created = time(0);
    return 0;
}


/*
    Rotate ticket keys each timeout. Accept the previous key for one more timeout and renew its tickets.
 */
static int ticketKeyCallback(SSL *ssl, uchar *name, uchar *iv, EVP_CIPHER_CTX *ctx, EVP_MAC_CTX *hctx, int enc)
{
    TicketKey  *key;
    OSSL_PARAM params[3];
    time_t     now;
    int        i;

    now = time(0);
    if (enc) {
        if (ticketKeys[0].created == 0 || now - ticketKeys[0].created >= ME_GOAHEAD_SSL_TIMEOUT) {
            if (rotateTicketKeys() < 0) {
                return -1;
            }
        }
        key = &ticketKeys[0];
        if (RAND_bytes(iv, EVP_CIPHER_get_iv_length(EVP_aes_256_cbc())) != 1) {
            return -1;
        }
        memcpy(name, key->name, sizeof(key->name));
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key->cipher, iv)) {
            return -1;
        }
    } else {
        for (i = 0, key = 0; i < 2 && !key; i++) {
            if (ticketKeys[i].created && now - ticketKeys[i].created < 2 * ME_GOAHEAD_SSL_TIMEOUT &&
                memcmp(name, ticketKeys[i].name, sizeof(ticketKeys[i].name)) == 0) {
                key = &ticketKeys[i];
            }
        }
        if (!key) {
            //  Unknown or retired key: make a full handshake
            return 0;
        }
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key->cipher, iv)) {
            return -1;
        }
    }
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY, key->mac, sizeof(key->mac));
    params[1] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, (char*) "SHA256", 0);
    params[2] = OSSL_PARAM_construct_end();
    if (!EVP_MAC_CTX_set_params(hctx, params)) {
        return -1;
    }
    return (enc || key == &ticketKeys[0]) ? 1 : 2;
}


static int verifyClientCertificate(int ok, X509_STORE_CTX *xContext)
{
    X509 *cert;
    char subject[260], issuer[260];
    int  error, depth;

    subject[0] = issuer[0] = '\0';

    if ((cert = X509_STORE_CTX_get_current_cert(xContext)) == NULL) {
        logmsg(3, "Certificate verification failed: no current certificate");
        return 0;
    }
    error = X509_STORE_CTX_get_error(xContext);
    depth = X509_STORE_CTX_get_error_depth(xContext);

    ok = 1;
    //  The names are only logged and never decide acceptance
    if (X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) - 1) == NULL) {
        subject[0] = '\0';
    }
    if (X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) - 1) == NULL) {
        issuer[0] = '\0';
    }
    /*
        Enforce the configured chain depth limit unconditionally. The underlying verifier may surface an
        over-depth chain under a variety of error codes (for example, some LibreSSL releases report the
        chain as X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY instead of X509_V_ERR_CERT_CHAIN_TOO_LONG).
        Rejecting here based on X509_STORE_CTX_get_error_depth avoids relying on backend-specific error
        mapping.
     */
    if (ME_GOAHEAD_SSL_VERIFY_DEPTH < depth) {
        logmsg(3, "Certificate chain too long (depth %d exceeds limit %d)", depth, ME_GOAHEAD_SSL_VERIFY_DEPTH);
        trace(4, "OpenSSL: Subject: %s", subject);
        trace(4, "OpenSSL: Issuer: %s", issuer);
        return 0;
    }
    /*
        SECURITY Acceptable: ME_GOAHEAD_SSL_VERIFY_PEER requests certificates. Without VERIFY_ISSUER,
        accept untrusted and forged-issuer certificates without revocation checks; only key possession is proven.
        Still reject invalid dates, malformed certificates, and excessive chain depth.
     */
    switch (error) {
    case X509_V_OK:
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
            logmsg(3, "Self-signed certificate");
            ok = 0;
        }
        break;

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
    return ok;
}



/*
    Map iana names to OpenSSL names so users can provide IANA names as well as OpenSSL cipher names
 */
/*
    Split TLS 1.3 and earlier cipher names between OpenSSL APIs.
    Apply the minimum cipher restrictions to earlier versions; fail if either list is invalid.
 */
static int setCiphers(cchar *ciphers)
{
    WebsBuf tls13, legacy;
    char    *list, *cipher, *next;
    int     rc;

    bufCreate(&tls13, 0, -1);
    bufCreate(&legacy, 0, -1);
    list = sclone(ciphers);
    for (next = list; (cipher = stok(next, ":", &next)) != 0; ) {
        if (sstarts(cipher, "TLS_")) {
            bufPut(&tls13, "%s%s", bufLen(&tls13) ? ":" : "", cipher);
        } else {
            bufPut(&legacy, "%s:", cipher);
        }
    }
    wfree(list);
    bufPut(&legacy, "%s%s", bufLen(&legacy) ? "" : "DEFAULT:", OPENSSL_CIPHER_FLOOR);

    rc = 0;
    if (bufLen(&tls13) && SSL_CTX_set_ciphersuites(sslctx, bufStart(&tls13)) != 1) {
        error("Unable to set TLS 1.3 ciphers \"%s\"", bufStart(&tls13));
        rc = -1;
    }
    if (rc == 0 && SSL_CTX_set_cipher_list(sslctx, bufStart(&legacy)) != 1) {
        error("Unable to set cipher list \"%s\"", bufStart(&legacy));
        rc = -1;
    }
    bufFree(&tls13);
    bufFree(&legacy);
    return rc;
}


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
    BIGNUM               *p, *g;

    if ((dh = DH_new()) == 0) {
        return 0;
    }
    p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
    g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
    if (!DH_set0_pqg(dh, p, NULL, g)) {
        BN_free(p);
        BN_free(g);
        DH_free(dh);
        return 0;
    }
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
