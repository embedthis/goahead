/*
    me.h -- MakeMe Configure Header for macosx-arm64-default
 */

/* Settings */
#ifndef ME_AUTHOR
    #define ME_AUTHOR "Embedthis Software"
#endif
#ifndef ME_CERTS_GENDH
    #define ME_CERTS_GENDH 1
#endif
#ifndef ME_COMPANY
    #define ME_COMPANY "embedthis"
#endif
#ifndef ME_COMPATIBLE
    #define ME_COMPATIBLE "6.0"
#endif
#ifndef ME_COMPILER_FORTIFY
    #define ME_COMPILER_FORTIFY 1
#endif
#ifndef ME_COMPILER_HAS_ATOMIC
    #define ME_COMPILER_HAS_ATOMIC 1
#endif
#ifndef ME_COMPILER_HAS_ATOMIC64
    #define ME_COMPILER_HAS_ATOMIC64 1
#endif
#ifndef ME_COMPILER_HAS_DOUBLE_BRACES
    #define ME_COMPILER_HAS_DOUBLE_BRACES 1
#endif
#ifndef ME_COMPILER_HAS_DYN_LOAD
    #define ME_COMPILER_HAS_DYN_LOAD 1
#endif
#ifndef ME_COMPILER_HAS_LIB_EDIT
    #define ME_COMPILER_HAS_LIB_EDIT 1
#endif
#ifndef ME_COMPILER_HAS_LIB_RT
    #define ME_COMPILER_HAS_LIB_RT 0
#endif
#ifndef ME_COMPILER_HAS_MMU
    #define ME_COMPILER_HAS_MMU 1
#endif
#ifndef ME_COMPILER_HAS_MTUNE
    #define ME_COMPILER_HAS_MTUNE 1
#endif
#ifndef ME_COMPILER_HAS_PAM
    #define ME_COMPILER_HAS_PAM 0
#endif
#ifndef ME_COMPILER_HAS_STACK_PROTECTOR
    #define ME_COMPILER_HAS_STACK_PROTECTOR 1
#endif
#ifndef ME_COMPILER_HAS_SYNC
    #define ME_COMPILER_HAS_SYNC 1
#endif
#ifndef ME_COMPILER_HAS_SYNC64
    #define ME_COMPILER_HAS_SYNC64 1
#endif
#ifndef ME_COMPILER_HAS_SYNC_CAS
    #define ME_COMPILER_HAS_SYNC_CAS 1
#endif
#ifndef ME_COMPILER_HAS_UNNAMED_UNIONS
    #define ME_COMPILER_HAS_UNNAMED_UNIONS 1
#endif
#ifndef ME_COMPILER_NOEXECSTACK
    #define ME_COMPILER_NOEXECSTACK 0
#endif
#ifndef ME_COMPILER_WARN64TO32
    #define ME_COMPILER_WARN64TO32 1
#endif
#ifndef ME_COMPILER_WARN_UNUSED
    #define ME_COMPILER_WARN_UNUSED 1
#endif
#ifndef ME_DEBUG
    #define ME_DEBUG 1
#endif
#ifndef ME_DEPRECATED_WARNINGS
    #define ME_DEPRECATED_WARNINGS 0
#endif
#ifndef ME_DEPTH
    #define ME_DEPTH 1
#endif
#ifndef ME_DESCRIPTION
    #define ME_DESCRIPTION "Embedthis GoAhead"
#endif
#ifndef ME_GOAHEAD_ACCESS_LOG
    #define ME_GOAHEAD_ACCESS_LOG 0
#endif
#ifndef ME_GOAHEAD_AUTH
    #define ME_GOAHEAD_AUTH 1
#endif
#ifndef ME_GOAHEAD_AUTH_STORE
    #define ME_GOAHEAD_AUTH_STORE "file"
#endif
#ifndef ME_GOAHEAD_AUTO_LOGIN
    #define ME_GOAHEAD_AUTO_LOGIN 0
#endif
#ifndef ME_GOAHEAD_CGI
    #define ME_GOAHEAD_CGI 1
#endif
#ifndef ME_GOAHEAD_CGI_VAR_PREFIX
    #define ME_GOAHEAD_CGI_VAR_PREFIX "CGI_"
#endif
#ifndef ME_GOAHEAD_CLIENT_CACHE
    #define ME_GOAHEAD_CLIENT_CACHE "css,gif,ico,jpg,js,png"
#endif
#ifndef ME_GOAHEAD_CLIENT_CACHE_LIFESPAN
    #define ME_GOAHEAD_CLIENT_CACHE_LIFESPAN 86400
#endif
#ifndef ME_GOAHEAD_DIGEST
    #define ME_GOAHEAD_DIGEST 1
#endif
#ifndef ME_GOAHEAD_DOCUMENTS
    #define ME_GOAHEAD_DOCUMENTS "web"
#endif
#ifndef ME_GOAHEAD_JAVASCRIPT
    #define ME_GOAHEAD_JAVASCRIPT 1
#endif
#ifndef ME_GOAHEAD_LEGACY
    #define ME_GOAHEAD_LEGACY 0
#endif
#ifndef ME_GOAHEAD_LIMIT_BUFFER
    #define ME_GOAHEAD_LIMIT_BUFFER 1024
#endif
#ifndef ME_GOAHEAD_LIMIT_CGI_ARGS
    #define ME_GOAHEAD_LIMIT_CGI_ARGS 4096
#endif
#ifndef ME_GOAHEAD_LIMIT_FILENAME
    #define ME_GOAHEAD_LIMIT_FILENAME 256
#endif
#ifndef ME_GOAHEAD_LIMIT_FILES
    #define ME_GOAHEAD_LIMIT_FILES 0
#endif
#ifndef ME_GOAHEAD_LIMIT_HEADER
    #define ME_GOAHEAD_LIMIT_HEADER 2048
#endif
#ifndef ME_GOAHEAD_LIMIT_HEADERS
    #define ME_GOAHEAD_LIMIT_HEADERS 4096
#endif
#ifndef ME_GOAHEAD_LIMIT_NUM_HEADERS
    #define ME_GOAHEAD_LIMIT_NUM_HEADERS 64
#endif
#ifndef ME_GOAHEAD_LIMIT_PARSE_TIMEOUT
    #define ME_GOAHEAD_LIMIT_PARSE_TIMEOUT 5
#endif
#ifndef ME_GOAHEAD_LIMIT_PASSWORD
    #define ME_GOAHEAD_LIMIT_PASSWORD 32
#endif
#ifndef ME_GOAHEAD_LIMIT_POST
    #define ME_GOAHEAD_LIMIT_POST 16384
#endif
#ifndef ME_GOAHEAD_LIMIT_PUT
    #define ME_GOAHEAD_LIMIT_PUT 204800000
#endif
#ifndef ME_GOAHEAD_LIMIT_SESSION_COUNT
    #define ME_GOAHEAD_LIMIT_SESSION_COUNT 512
#endif
#ifndef ME_GOAHEAD_LIMIT_SESSION_LIFE
    #define ME_GOAHEAD_LIMIT_SESSION_LIFE 1800
#endif
#ifndef ME_GOAHEAD_LIMIT_STRING
    #define ME_GOAHEAD_LIMIT_STRING 256
#endif
#ifndef ME_GOAHEAD_LIMIT_TIMEOUT
    #define ME_GOAHEAD_LIMIT_TIMEOUT 60
#endif
#ifndef ME_GOAHEAD_LIMIT_UPLOAD
    #define ME_GOAHEAD_LIMIT_UPLOAD 204800000
#endif
#ifndef ME_GOAHEAD_LIMIT_URI
    #define ME_GOAHEAD_LIMIT_URI 2048
#endif
#ifndef ME_GOAHEAD_LISTEN
    #define ME_GOAHEAD_LISTEN "http://*:80,https://*:443"
#endif
#ifndef ME_GOAHEAD_LOGFILE
    #define ME_GOAHEAD_LOGFILE "stderr:0"
#endif
#ifndef ME_GOAHEAD_LOGGING
    #define ME_GOAHEAD_LOGGING 1
#endif
#ifndef ME_GOAHEAD_PUT_DIR
    #define ME_GOAHEAD_PUT_DIR "."
#endif
#ifndef ME_GOAHEAD_REALM
    #define ME_GOAHEAD_REALM "example.com"
#endif
#ifndef ME_GOAHEAD_REPLACE_MALLOC
    #define ME_GOAHEAD_REPLACE_MALLOC 0
#endif
#ifndef ME_GOAHEAD_SSL_AUTHORITY
    #define ME_GOAHEAD_SSL_AUTHORITY ""
#endif
#ifndef ME_GOAHEAD_SSL_CACHE
    #define ME_GOAHEAD_SSL_CACHE 512
#endif
#ifndef ME_GOAHEAD_SSL_CERTIFICATE
    #define ME_GOAHEAD_SSL_CERTIFICATE "self.crt"
#endif
#ifndef ME_GOAHEAD_SSL_CIPHERS
    #define ME_GOAHEAD_SSL_CIPHERS ""
#endif
#ifndef ME_GOAHEAD_SSL_HANDSHAKES
    #define ME_GOAHEAD_SSL_HANDSHAKES 3
#endif
#ifndef ME_GOAHEAD_SSL_KEY
    #define ME_GOAHEAD_SSL_KEY "self.key"
#endif
#ifndef ME_GOAHEAD_SSL_LOG_LEVEL
    #define ME_GOAHEAD_SSL_LOG_LEVEL 5
#endif
#ifndef ME_GOAHEAD_SSL_REVOKE
    #define ME_GOAHEAD_SSL_REVOKE ""
#endif
#ifndef ME_GOAHEAD_SSL_TICKET
    #define ME_GOAHEAD_SSL_TICKET 1
#endif
#ifndef ME_GOAHEAD_SSL_TIMEOUT
    #define ME_GOAHEAD_SSL_TIMEOUT 86400
#endif
#ifndef ME_GOAHEAD_SSL_VERIFY_ISSUER
    #define ME_GOAHEAD_SSL_VERIFY_ISSUER 0
#endif
#ifndef ME_GOAHEAD_SSL_VERIFY_PEER
    #define ME_GOAHEAD_SSL_VERIFY_PEER 0
#endif
#ifndef ME_GOAHEAD_STEALTH
    #define ME_GOAHEAD_STEALTH 1
#endif
#ifndef ME_GOAHEAD_TRACING
    #define ME_GOAHEAD_TRACING 1
#endif
#ifndef ME_GOAHEAD_UPLOAD
    #define ME_GOAHEAD_UPLOAD 1
#endif
#ifndef ME_GOAHEAD_UPLOAD_DIR
    #define ME_GOAHEAD_UPLOAD_DIR "tmp"
#endif
#ifndef ME_GOAHEAD_XFRAME_HEADER
    #define ME_GOAHEAD_XFRAME_HEADER "SAMEORIGIN"
#endif
#ifndef ME_INTEGRATE
    #define ME_INTEGRATE 1
#endif
#ifndef ME_MANIFEST
    #define ME_MANIFEST "installs/manifest.me"
#endif
#ifndef ME_MBEDTLS_COMPACT
    #define ME_MBEDTLS_COMPACT 1
#endif
#ifndef ME_NAME
    #define ME_NAME "goahead"
#endif
#ifndef ME_OPENSSL_VERSION
    #define ME_OPENSSL_VERSION "1.0"
#endif
#ifndef ME_PARTS
    #define ME_PARTS "undefined"
#endif
#ifndef ME_PREFIXES
    #define ME_PREFIXES "install-prefixes"
#endif
#ifndef ME_ROM
    #define ME_ROM 0
#endif
#ifndef ME_ROM_TIME
    #define ME_ROM_TIME 1505449519432
#endif
#ifndef ME_TITLE
    #define ME_TITLE "Embedthis GoAhead"
#endif
#ifndef ME_VERSION
    #define ME_VERSION "6.0.5"
#endif

/* Prefixes */
#ifndef ME_ROOT_PREFIX
    #define ME_ROOT_PREFIX "/"
#endif
#ifndef ME_BASE_PREFIX
    #define ME_BASE_PREFIX "/usr/local"
#endif
#ifndef ME_DATA_PREFIX
    #define ME_DATA_PREFIX "/"
#endif
#ifndef ME_STATE_PREFIX
    #define ME_STATE_PREFIX "/var"
#endif
#ifndef ME_APP_PREFIX
    #define ME_APP_PREFIX "/usr/local/lib/goahead"
#endif
#ifndef ME_VAPP_PREFIX
    #define ME_VAPP_PREFIX "/usr/local/lib/goahead/6.0.5"
#endif
#ifndef ME_BIN_PREFIX
    #define ME_BIN_PREFIX "/usr/local/bin"
#endif
#ifndef ME_INC_PREFIX
    #define ME_INC_PREFIX "/usr/local/include"
#endif
#ifndef ME_LIB_PREFIX
    #define ME_LIB_PREFIX "/usr/local/lib"
#endif
#ifndef ME_MAN_PREFIX
    #define ME_MAN_PREFIX "/usr/local/share/man"
#endif
#ifndef ME_SBIN_PREFIX
    #define ME_SBIN_PREFIX "/usr/local/sbin"
#endif
#ifndef ME_ETC_PREFIX
    #define ME_ETC_PREFIX "/etc/goahead"
#endif
#ifndef ME_WEB_PREFIX
    #define ME_WEB_PREFIX "/var/www/goahead"
#endif
#ifndef ME_LOG_PREFIX
    #define ME_LOG_PREFIX "/var/log/goahead"
#endif
#ifndef ME_VLIB_PREFIX
    #define ME_VLIB_PREFIX "/var/lib/goahead"
#endif
#ifndef ME_SPOOL_PREFIX
    #define ME_SPOOL_PREFIX "/var/spool/goahead"
#endif
#ifndef ME_CACHE_PREFIX
    #define ME_CACHE_PREFIX "/var/spool/goahead/cache"
#endif
#ifndef ME_SRC_PREFIX
    #define ME_SRC_PREFIX "goahead-6.0.5"
#endif

/* Suffixes */
#ifndef ME_EXE
    #define ME_EXE ""
#endif
#ifndef ME_SHLIB
    #define ME_SHLIB ".dylib"
#endif
#ifndef ME_SHOBJ
    #define ME_SHOBJ ".dylib"
#endif
#ifndef ME_LIB
    #define ME_LIB ".a"
#endif
#ifndef ME_OBJ
    #define ME_OBJ ".o"
#endif

/* Profile */
#ifndef ME_CONFIG_CMD
    #define ME_CONFIG_CMD "me -d -q -platform macosx-arm64-default -configure . -gen xcode"
#endif
#ifndef ME_GOAHEAD_PRODUCT
    #define ME_GOAHEAD_PRODUCT 1
#endif
#ifndef ME_PROFILE
    #define ME_PROFILE "default"
#endif
#ifndef ME_TUNE_SIZE
    #define ME_TUNE_SIZE 1
#endif

/* Miscellaneous */
#ifndef ME_MAJOR_VERSION
    #define ME_MAJOR_VERSION 6
#endif
#ifndef ME_MINOR_VERSION
    #define ME_MINOR_VERSION 0
#endif
#ifndef ME_PATCH_VERSION
    #define ME_PATCH_VERSION 5
#endif
#ifndef ME_VNUM
    #define ME_VNUM 600000005
#endif

/* Components */
#ifndef ME_COM_CC
    #define ME_COM_CC 1
#endif
#ifndef ME_COM_LIB
    #define ME_COM_LIB 1
#endif
#ifndef ME_COM_MBEDTLS
    #define ME_COM_MBEDTLS 0
#endif
#ifndef ME_COM_OPENSSL
    #define ME_COM_OPENSSL 1
#endif
#ifndef ME_COM_OSDEP
    #define ME_COM_OSDEP 1
#endif
#ifndef ME_COM_SSL
    #define ME_COM_SSL 1
#endif
#ifndef ME_COM_VXWORKS
    #define ME_COM_VXWORKS 0
#endif
