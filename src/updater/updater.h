/*
    updater.h - Over-The-Air (OTA) software update library for IoT devices

    This header provides the public API for the EmbedThis Updater library. The updater communicates
    with the EmbedThis Builder cloud service to check for, download, verify, and apply device firmware updates.

    Copyright (c) EmbedThis Software. All Rights Reserved.
 */

#ifndef _h_UPDATER
#define _h_UPDATER 1

#include "osdep.h"

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>

/********************************** Defines ***********************************/
#ifdef __cplusplus
extern "C" {
#endif

/**
    Check for and apply software updates from the EmbedThis Builder service

    @description This function implements a complete OTA update workflow:
        1. Queries the Builder service to check for available updates
        2. Downloads the update package if one is available
        3. Verifies the download using SHA-256 checksum
        4. Optionally applies the update by executing a custom script
        5. Reports the update status back to the Builder service

    The function uses HTTPS with certificate verification for all network communications.
    All parameters must be non-NULL except for 'properties' and 'script'.

    @param host Builder cloud endpoint URL. Must not be NULL.
    @param product Product ID from the Builder token list. Must not be NULL.
    @param token CloudAPI access token from the Builder token list for authentication. Must not be NULL.
    @param device Unique device identifier. Must not be NULL.
    @param version Current device firmware version in semantic versioning format. Must not be NULL.
    @param properties Optional JSON key-value pairs for device-specific properties used in distribution policy
        matching. Format: "\"key\":\"value\",\"key2\":\"value2\"". May be NULL.
    @param path File path where the downloaded update will be saved. The file will be created with 0600 permissions.
        Must not be NULL. The update script should remove this file after successful application.
    @param script Optional path to executable script that applies the update. The script receives the update file
        path as its only argument. If NULL, the update is downloaded and verified but not applied. May be NULL.
    @param verbose Set to non-zero to enable verbose tracing of execution to stdout, zero for normal operation.
    @param quiet Set to non-zero to suppress all stdout output, zero for normal output. Errors are still written to
       stderr.
        When both verbose and quiet are set, quiet takes precedence.

    @return Returns 0 on success. Returns -1 on error (invalid parameters, network failure, checksum mismatch,
        or script execution failure).

    @ingroup Updater
 */
PUBLIC int update(cchar *host, cchar *product, cchar *token, cchar *device, cchar *version, cchar *properties,
                  cchar *path, cchar *script, int verbose, int quiet);


#ifdef __cplusplus
}
#endif
#endif /* _h_UPDATER */