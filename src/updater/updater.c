/*
    updater.c - Download and apply software updates published via EmbedThis Builder

    This uses a minimal fetch() client suitable for the needs of the update REST API only.
 */

/********************************** Includes **********************************/

#include "updater.h"

/********************************** Locals ************************************/

/*
    Adjust for path to your certificate bundle
 */
#ifndef UPDATER_CERTS
    #if defined(__APPLE__)
        #define UPDATER_CERTS "/opt/homebrew/etc/openssl@3/cert.pem"
    #elif defined(__linux__)
        #define UPDATER_CERTS "/etc/pki/tls/certs/ca-bundle.crt"
    #elif defined(__MINGW32__)
        #define UPDATER_CERTS "/etc/ssl/certs/ca-certificates.crt"
    #else
        #error "ERROR: Need to define UPDATER_CERTS to the path to your certificate bundle"
    #endif
#endif

#define SERVER_PORT 443
#define UBSIZE      4096

typedef struct Fetch {
    SSL_CTX *ctx;          //  TLS context
    SSL *ssl;              //  TLS config
    int fd;                //  Connection socket fd
    size_t contentLength;  //  Response content length
    size_t bodyLength;     //  Length of body segment
    char *body;            //  First block of body data
    int status;            //  Response HTTP status
} Fetch;

static int verbose;        //  Trace execution

/********************************** Forwards **********************************/

static int applyUpdate(cchar *path, cchar *script);
static Fetch *fetch(char *method, char *url, char *headers, char *body);
static Fetch *fetchAlloc(int fd, cchar *host);
static void fetchFree(Fetch *fp);
static char *fetchString(Fetch *fp);
static int fetchFile(Fetch *fp, cchar *path);
static char *fetchHeader(cchar *response, char *key);
static ssize_t fetchRead(Fetch *fp, char *buf, size_t buflen);
static ssize_t fetchWrite(Fetch *fp, char *buf, size_t buflen);
static int getFileSum(cchar *path, char sum[EVP_MAX_MD_SIZE * 2 + 1]);
static char *json(cchar *jsonText, cchar *key);
static void *memdup(cvoid *ptr, size_t size);
static int postReport(int success, cchar *host, cchar *device, cchar *update, cchar *token);
static int run(cchar *script, cchar *path);

/************************************ Code ************************************/
/*
    Update parameters:
    host - Device Cloud endpoint from the Builder Cloud Edit panel
    product - ProductID token from the Builder token list
    token - CloudAPI token from the Builder token list
    device - Unique device ID
    version - device firmware version
    properties - Additional device properties [key=value, ...]
    path - Path name to save the downloaded update image
    script - Script path to invoke to apply the update
    verbose - Trace execution

    SECURITY Acceptable: - The developer is responsible for validating the inputs to this function.
 */
int update(cchar *host, cchar *product, cchar *token, cchar *device, cchar *version,
           cchar *properties, cchar *path, cchar *script, int verboseArg)
{
    Fetch *fp;
    char  body[UBSIZE], request[UBSIZE], url[UBSIZE], headers[256], fileSum[EVP_MAX_MD_SIZE * 2 + 1];
    char  *checksum, *downloadUrl, *response, *update, *updateVersion;
    int   count, rc, status;

    rc = 0;

    if (!host || !product || !token || !device || !version || !path) {
        fprintf(stderr, "Bad update args");
        return -1;
    }
    verbose = verboseArg;

    /*
        Issue update request to determine if there is an update.
        Authentication is using the CloudAPI builder token.
     */
    count = snprintf(url, sizeof(url), "%s/tok/provision/update", host);
    if (count >= sizeof(url)) {
        fprintf(stderr, "Host URL is too long\n");
        return -1;
    }
    /*
        SECURITY Acceptable: - The developer is responsible for validating the inputs to this function.
     */
    count = snprintf(body, sizeof(body),
                     "{\"id\":\"%s\",\"product\":\"%s\",\"version\":\"%s\",%s}",
                     device, product, version, properties ? properties : "");
    if (count >= sizeof(body)) {
        fprintf(stderr, "Request body is too long\n");
        return -1;
    }
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\nAuthorization: %s\r\n", token);

    if (verbose) {
        printf("\nCheck for update at: %s\n", url);
    }
    if ((fp = fetch("POST", url, headers, body)) == NULL) {
        return -1;
    }
    if ((response = fetchString(fp)) == NULL || response == NULL) {
        fetchFree(fp);
        return -1;
    }
    fetchFree(fp);

    /*
        If an update is available, the "url" will be defined to point to the update image
        The "update" field contains the selected update ID and is use when posting update status.
     */
    downloadUrl = checksum = update = updateVersion = NULL;

    if ((downloadUrl = json(response, "url")) != NULL) {
        checksum = json(response, "checksum");
        update = json(response, "update");
        updateVersion = json(response, "version");

        snprintf(headers, sizeof(headers), "Accept: */*\r\n");
        free(response);

        printf("Update %s available\n", updateVersion);
        rc = 0;
        if (strncmp(downloadUrl, "https://", 8) != 0) {
            fprintf(stderr, "Insecure download URL (HTTPS required)\n");
            rc = -1;
        } else if ((fp = fetch("GET", downloadUrl, headers, NULL)) == NULL) {
            rc = -1;
        } else {
            //  Fetch the update and save to the given path
            if (fetchFile(fp, path) < 0) {
                rc = -1;
            }
            fetchFree(fp);
            if (rc == 0) {
                printf("Verify update checksum in %s\n", path);
                getFileSum(path, fileSum);
                if (strcmp(fileSum, checksum) != 0) {
                    fprintf(stderr, "Checksum does not match\n%s vs\n%s\n", fileSum, checksum);
                    rc = -1;
                } else if (script) {
                    status = applyUpdate(path, script);
                    if (postReport(status, host, device, update, token) < 0) {
                        rc = -1;
                    }
                }
            }
        }
        if (downloadUrl) free(downloadUrl);
        if (checksum) free(checksum);
        if (update) free(update);
        if (updateVersion) free(updateVersion);
    } else {
        free(response);
        printf("No update available\n");
    }
    return rc;
}

/*
    Apply the update by invoking the "scripts.update" script
    This may exit or reboot if instructed by the update script
 */
static int applyUpdate(cchar *path, cchar *script)
{
    int status;

    printf("Applying update: %s %s\n", script, path);
    status = run(script, path);
    printf("Update %s\n\n", status == 0 ? "Successful" : "Failed");
    return status;
}

/*
    SECURITY Acceptable: - The unix code above is preferred, but on systems without
    fork() and execvp() we use the system() function. The inputs are all from
    developer controlled input and not user controlled.
 */
static int run(cchar *script, cchar *path)
{
#if ME_UNIX_LIKE
    char  *args[] = { (char*) script, (char*) path, NULL };
    int   status;
    pid_t pid;

    if ((pid = fork()) < 0) {
        fprintf(stderr, "Cannot fork to run command\n");
        return -1;
    }
    if (pid == 0) {
        execvp(script, args);
        fprintf(stderr, "Cannot run command\n");
        _exit(127);
    }
    if (waitpid(pid, &status, 0) == -1) {
        fprintf(stderr, "Cannot wait for command\n");
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;

#else
    /*
        SECURITY: The system() call is dangerous and should be avoided.
        Replace this with a platform-specific implementation that does not use a shell,
        such as CreateProcess() on Windows.
     */
    # error "ERROR: Secure process creation not implemented for this platform"
    #error
    return -1;
#endif
}

/*
    Post update status back to the builder for metrics and version tracking
 */
static int postReport(int status, cchar *host, cchar *device, cchar *update, cchar *token)
{
    Fetch  *fp;
    char   body[UBSIZE], url[256], headers[256];
    size_t count;

    count = snprintf(body, sizeof(body), "{\"success\":%s,\"id\":\"%s\",\"update\":\"%s\"}",
                     status == 0 ? "true" : "false", device, update);
    if (count >= sizeof(body)) {
        fprintf(stderr, "Report body is too long\n");
        return -1;
    }
    snprintf(url, sizeof(url), "%s/tok/provision/updateReport", host);
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\nAuthorization: %s\r\n", token);

    if ((fp = fetch("POST", url, headers, body)) == NULL) {
        fprintf(stderr, "Cannot post update-report\n");
        return -1;
    }
    fetchFree(fp);
    return 0;
}

/*
    Mini-fetch API. Start an HTTP action. This is NOT a generic fetch API implementation.
 */
static Fetch *fetch(char *method, char *url, char *headers, char *body)
{
    struct sockaddr_in server_addr;
    struct hostent     *server;
    ssize_t            bytes;
    Fetch              *fp;
    char               request[UBSIZE], response[UBSIZE], uri[UBSIZE];
    char               *data, *header, *host, *path, *status;
    int                fd;

    snprintf(uri, sizeof(uri), "%s", url);
    if ((host = strstr(uri, "https://")) != NULL) {
        host += 8;
    } else {
        host = uri;
    }
    if ((path = strchr(host, '/')) != NULL) {
        *path++ = '\0';
    } else {
        path = "";
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Cannot open socket");
        return NULL;
    }
    server = gethostbyname(host);
    if (server == NULL) {
        close(fd);
        fprintf(stderr, "Cannot find host\n");
        return NULL;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to host");
        close(fd);
        return NULL;
    }
    if ((fp = fetchAlloc(fd, host)) == NULL) {
        close(fd);
        return NULL;
    }
    /*
        Format the request and calculate the body content length
     */
    snprintf(request, sizeof(request),
             "%s /%s HTTP/1.1\r\n" \
             "Host: %s\r\n" \
             "Content-Length: %d\r\n" \
             "%s\r\n" \
             "%s",
             method, path, host, body ? (int) strlen(body) : 0, headers, body ? body : "");

    /*
        Write the request and wait for a response
     */
    if (fetchWrite(fp, request, strlen(request)) <= 0) {
        fetchFree(fp);
        return NULL;
    }
    memset(response, 0, UBSIZE);
    if ((bytes = fetchRead(fp, response, UBSIZE - 1)) < 0) {
        fetchFree(fp);
        return NULL;
    }
    if (strncmp(response, "HTTP/1.1 ", 9) != 0) {
        fprintf(stderr, "Bad response\n%s\n", response);
        fetchFree(fp);
        return NULL;
    }
    if ((status = strchr(response, ' ')) == NULL) {
        fprintf(stderr, "Bad response\n%s\n", response);
        fetchFree(fp);
        return NULL;
    }
    fp->status = atoi(++status);
    if (fp->status != 200) {
        fprintf(stderr, "Bad response status %d\n%s\n", fp->status, response);
        fetchFree(fp);
        return NULL;
    }
    if ((data = strstr(response, "\r\n\r\n")) == NULL) {
        fprintf(stderr, "Bad response\n%s\n", response);
        fetchFree(fp);
        return NULL;
    }
    *data = '\0';
    data += 4;
    if ((header = fetchHeader(response, "Content-Length")) != NULL) {
        fp->contentLength = atoi(header);
        free(header);
        if (fp->contentLength < 0 || fp->contentLength > 100 * 1024 * 1024) {
            fprintf(stderr, "Invalid Content-Length\n");
            fetchFree(fp);
            return NULL;
        }
        size_t headerBytes = (size_t) (data - response);
        if ((size_t) bytes > headerBytes) {
            fp->bodyLength = (size_t) ((size_t) bytes - headerBytes);
            fp->body = memdup(data, fp->bodyLength);
        } else {
            fp->bodyLength = 0;
            fp->body = NULL;
        }
    } else {
        fprintf(stderr, "Missing Content-Length\n");
        fetchFree(fp);
        return NULL;
    }
    return fp;
}

/*
    Return a small response body as a string. Caller must free.
 */
static char *fetchString(Fetch *fp)
{
    char    *bp, *body;
    ssize_t bytes;
    size_t  len = 0;

    if (fp->contentLength == 0) {
        return strdup("");
    }
    if ((body = malloc(fp->contentLength + 1)) == NULL) {
        fprintf(stderr, "Cannot allocate %d bytes", (int) fp->contentLength);
        return NULL;
    }
    bp = body;
    if (fp->body && fp->bodyLength > 0) {
        //  Use the body fragment already read with the headers
        memcpy(body, fp->body, fp->bodyLength);
        len = fp->bodyLength;
        bp += fp->bodyLength;
        free(fp->body);
        fp->body = NULL;
    }
    while (len < fp->contentLength) {
        if ((bytes = fetchRead(fp, bp, fp->contentLength - (bp - body))) > 0) {
            len += bytes;
            bp += bytes;
        }
    }
    body[fp->contentLength] = '\0';
    return body;
}

/*
    Return a response body to a file.
 */
static int fetchFile(Fetch *fp, cchar *path)
{
    char   buf[UBSIZE], sum[EVP_MAX_MD_SIZE * 2 + 1], *body;
    int    bytes, fd, flags, seenHeaders;
    size_t len;

    if (strncmp(path, "/tmp/", 5) == 0) {
        fprintf(stderr, "WARNING: Saving update to /tmp is insecure due to potential symlink attacks.\n");
    }
    if (verbose) {
        printf("Downloading update to %s\n", path);
    }
    unlink(path);
    flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    if ((fd = open(path, flags, 0600)) < 0) {
        fprintf(stderr, "Cannot open image temp file securely. It may already exist or path is invalid.\n");
        return -1;
    }
    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Refusing to write to non-regular file\n");
        close(fd);
        return -1;
    }
    if (fp->body) {
        //  Write the body fragment already read with the headers
        if (write(fd, fp->body, fp->bodyLength) < 0) {
            fprintf(stderr, "Cannot write to file");
            close(fd);
            return -1;
        }
        free(fp->body);
        fp->body = NULL;
    }
    while ((bytes = fetchRead(fp, buf, sizeof(buf))) > 0) {
        if (write(fd, buf, bytes) != bytes) {
            fprintf(stderr, "Cannot save response");
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}

/*
    Return a response HTTP header. Caller must free.
 */
static char *fetchHeader(cchar *response, char *key)
{
    char   *end, *start, kbuf[80], *value;
    size_t len;

    snprintf(kbuf, sizeof(kbuf), "%s:", key);
    value = 0;
    if ((start = strstr(response, kbuf)) != NULL) {
        if ((end = strstr(start, "\r\n")) != NULL) {
            start += strlen(kbuf);
            while (start < end && isspace(*start)) start++;
            len = end - start;
            value = malloc(len + 1);
            strncpy(value, start, len);
            value[len] = '\0';
        }
    }
    return value;
}

/*
    Read response data
 */
static ssize_t fetchRead(Fetch *fp, char *buf, size_t buflen)
{
    ssize_t rc;

    rc = SSL_read(fp->ssl, buf, (int) buflen);
    if (rc <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return rc;
}

/*
    Write request data
 */
static ssize_t fetchWrite(Fetch *fp, char *buf, size_t buflen)
{
    ssize_t rc;

    rc = SSL_write(fp->ssl, buf, (int) buflen);
    if (rc <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return rc;
}

/*
    Allocate a Fetch control structure. This primarily sets up an OpenSSL context.
 */
static Fetch *fetchAlloc(int fd, cchar *host)
{
    Fetch            *fp;
    const SSL_METHOD *method;

    fp = malloc(sizeof(Fetch));
    memset(fp, 0, sizeof(Fetch));

    method = TLS_client_method();
    fp->ctx = SSL_CTX_new(method);
    if (!fp->ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        free(fp);
        return NULL;
    }
    /* Enforce modern TLS */
    SSL_CTX_set_min_proto_version(fp->ctx, TLS1_2_VERSION);

    /*
        Verify server certificate
     */
    SSL_CTX_set_verify(fp->ctx, SSL_VERIFY_PEER, NULL);
    if (!SSL_CTX_load_verify_locations(fp->ctx, UPDATER_CERTS, NULL)) {
        fprintf(stderr, "Failed to load CA bundle.\n");
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(fp->ctx);
        free(fp);
        return NULL;
    }
    /* Also use system default CA paths where available */
    SSL_CTX_set_default_verify_paths(fp->ctx);

    fp->ssl = SSL_new(fp->ctx);
    fp->fd = fd;
    SSL_set_fd(fp->ssl, fd);
    /* Send SNI */
    SSL_set_tlsext_host_name(fp->ssl, host);

    /*
        Verify hostname
     */
    X509_VERIFY_PARAM *param = SSL_get0_param(fp->ssl);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
#endif
    if (!X509_VERIFY_PARAM_set1_host(param, host, 0)) {
        fprintf(stderr, "Failed to set hostname for verification\n");
        ERR_print_errors_fp(stderr);
        SSL_free(fp->ssl);
        SSL_CTX_free(fp->ctx);
        free(fp);
        return NULL;
    }

    if (SSL_connect(fp->ssl) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_free(fp->ssl);
        SSL_CTX_free(fp->ctx);
        free(fp);
        return NULL;
    }
    return fp;
}

/*
    Deallocate a Fetch control structure.
 */
static void fetchFree(Fetch *fp)
{
    if (!fp) {
        return;
    }
    if (fp->ssl) {
        SSL_free(fp->ssl);
        fp->ssl = NULL;
    }
    if (fp->ctx) {
        SSL_CTX_free(fp->ctx);
        fp->ctx = NULL;
    }
    if (fp->fd >= 0) {
        close(fp->fd);
        fp->fd = -1;
    }
    if (fp->body) {
        free(fp->body);
        fp->body = NULL;
    }
    free(fp);
}

/*
    Trivial routine to lookup a key value in a json formatted string.
    This is NOT a generic JSON parser and does NOT handle use other than required by the Updater API.
    Caller must free result.
 */
static char *json(cchar *jsonText, cchar *key)
{
    char *end, keybuf[80], *keyPos, *start, *value;
    int  quoted, count;

    count = snprintf(keybuf, sizeof(keybuf), "\"%s\":", key);
    if (count >= sizeof(keybuf)) {
        fprintf(stderr, "Key is too long\n");
        return NULL;
    }
    keyPos = strstr(jsonText, keybuf);
    if (keyPos) {
        value = keyPos + strlen(keybuf);  // assuming format is "key":<space>
        quoted = (*value == '"') ? 1 : 0;
        start = quoted ? value + 1 : value;
        end = start;
        while (*end && ((quoted && *end != '"') || (!quoted && *end != ',' && *end != '}'))) {
            end++;
        }
        int  size = end - start;
        char *vbuf = (char*) malloc(size + 1);
        if (vbuf) {
            strncpy(vbuf, start, size);
            vbuf[size] = '\0';
            return vbuf;
        }
    }
    return NULL;
}

/*
    Calculate a SHA-256 checksum for a file
 */
static int getFileSum(cchar *path, char sum[EVP_MAX_MD_SIZE * 2 + 1])
{
    EVP_MD_CTX *mdctx;
    FILE       *file;
    uchar      buf[UBSIZE], hash[EVP_MAX_MD_SIZE];
    uint       len;
    size_t     bytes;

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Cannot open %s\n", path);
        return -1;
    }
    mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        fprintf(stderr, "Failed to create EVP_MD_CTX");
        fclose(file);
        return -1;
    }
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        fprintf(stderr, "DigestInit error\n");
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return -1;
    }
    while ((bytes = fread(buf, 1, sizeof(buf), file)) > 0) {
        if (EVP_DigestUpdate(mdctx, buf, bytes) != 1) {
            fprintf(stderr, "DigestUpdate error\n");
            EVP_MD_CTX_free(mdctx);
            fclose(file);
            return -1;
        }
    }
    if (EVP_DigestFinal_ex(mdctx, hash, &len) != 1) {
        fprintf(stderr, "DigestFinal error\n");
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return -1;
    }
    EVP_MD_CTX_free(mdctx);
    fclose(file);

    for (int i = 0; i < len; i++) {
        snprintf(&sum[i * 2], 3, "%02x", hash[i]);
    }
    sum[len * 2] = '\0';
    return 0;
}

static void *memdup(cvoid *ptr, size_t size)
{
    char *newp;

    if (ptr == NULL) {
        return NULL;
    }
    if ((newp = malloc(size + 1)) != 0) {
        memcpy(newp, ptr, size);
        newp[size] = '\0';
    }
    return newp;
}
