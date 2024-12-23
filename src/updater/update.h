/*
    update.h - Check for software upgrades
 */

#ifndef HAS_CCHAR
typedef const char cchar;
#endif

/**
    Issue an update request to the Builder to determine if there is a software update
    @description If there is an update, download to the given path and invoke the script to apply
    @param host Device cloud host address
    @param product Product ID obtained from the Builder token list
    @param token CloudAPI token obtained from the Builder token list
    @param device Unique device ID
    @param version Device firmware version
    @param properties String of additional device properties of the form: "key:value, ..."
    @param path File name to save the downloaded update. The script should remove after applying
    @param script Optional script to invoke to apply the update. The path to the update is supplied as the only argument.
    @param verbose Set to true to trace execution
 */
int update(cchar *host, cchar *product, cchar *token, cchar *device, cchar *version, cchar *properties,
           cchar *path, cchar *script, int verbose);
