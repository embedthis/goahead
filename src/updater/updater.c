/*
    updater.c - Download and apply software updates published via EmbedThis Builder

    This uses a minimal fetch() client suitable for the needs of the update REST API only.
 */

/********************************** Includes **********************************/
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "updater.h"

/********************************** Locals ************************************/

#define DEFAULT_IMAGE_PATH "/tmp/update.bin"

#define SERVER_PORT        443
#define UBSIZE             4096

typedef struct Fetch {
    SSL_CTX *ctx;          //  TLS context
    SSL *ssl;              //  TLS config
    int fd;                //  Connection socket fd
    char *response;        //  Response headers
    char *firstBody;       //  Body read with headers
    size_t contentLength;  //  Response content length
    int status;            //  Response HTTP status
} Fetch;

static int verbose;        //  Trace execution

/********************************** Forwards **********************************/

static int applyUpdate(cchar *path, cchar *script);
static Fetch *fetch(char *method, char *url, char *headers, char *body);
static Fetch *fetchAlloc(int fd);
static void fetchFree(Fetch *fp);
static char *fetchString(Fetch *fp);
static int fetchFile(Fetch *fp, cchar *path);
static char *fetchHeader(Fetch *fp, char *key);
static size_t fetchRead(Fetch *fp, char *buf, size_t buflen);
static size_t fetchWrite(Fetch *fp, char *buf, size_t buflen);
static int getFileSum(cchar *path, char sum[EVP_MAX_MD_SIZE]);
static char *json(cchar *json, cchar *key);
static int postReport(int success, cchar *host, cchar *device, cchar *update, cchar *token);

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
 */
int update(cchar *host, cchar *product, cchar *token, cchar *device, cchar *version,
           cchar *properties, cchar *path, cchar *script, int verboseArg)
{
    Fetch *fp;
    char  body[UBSIZE], request[UBSIZE], url[UBSIZE], headers[256], fileSum[EVP_MAX_MD_SIZE];
    char  *checksum, *downloadUrl, *response, *update;
    int   status;

    if (!host || !product || !token || !device || !version || !path) {
        fprintf(stderr, "Bad update args");
        return -1;
    }
    verbose = verboseArg;

    /*
        Issue update request to determine if there is an update.
        Authentication is using the CloudAPI builder token.
     */
    snprintf(url, sizeof(url), "%s/tok/provision/update", host);
    snprintf(body, sizeof(body), "{\"id\":\"%s\",\"product\":\"%s\",\"version\":\"%s\",%s}",
             device, product, version, properties);
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\nAuthorization: %s\r\n", token);

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
    if ((downloadUrl = json(response, "url")) != NULL) {
        checksum = json(response, "checksum");
        update = json(response, "update");
        snprintf(headers, sizeof(headers), "Accept: */*\r\n");
        free(response);

        if ((fp = fetch("GET", downloadUrl, headers, NULL)) == NULL) {
            return -1;
        }
        //  Fetch the update and save to the given path
        if (fetchFile(fp, path) < 0) {
            fetchFree(fp);
            return -1;
        }
        fetchFree(fp);

        /*
            Validate the SHA-256 checksum
         */
        getFileSum(path, fileSum);
        if (strcmp(fileSum, checksum) != 0) {
            fprintf(stderr, "Checksum does not match\n%s vs\n%s\n", fileSum, checksum);
            return -1;
        }
        if (script) {
            status = applyUpdate(path, script);
            if (postReport(status, host, device, update, token) < 0) {
                return -1;
            }
        }
    } else {
        free(response);
        if (verbose) {
            printf("No update available\n");
        }
    }
    return 0;
}

/*
    Apply the update by invoking the "scripts.update" script
    This may exit or reboot if instructed by the update script
 */
static int applyUpdate(cchar *path, cchar *script)
{
    char command[UBSIZE];

    snprintf(command, sizeof(command), "%s \"%s\"", script, path);
    if (verbose) {
        printf("Running command to apply update\n%s\n", command);
    }
    return system(command);
}

/*
    Post update status back to the builder for metrics and version tracking
 */
static int postReport(int status, cchar *host, cchar *device, cchar *update, cchar *token)
{
    Fetch *fp;
    char  body[UBSIZE], url[256], headers[256];

    snprintf(body, sizeof(body), "{\"success\":%s,\"id\":\"%s\",\"update\":\"%s\"}",
             status == 0 ? "true" : "false", device, update);
    snprintf(url, sizeof(url), "%s/tok/provision/updateReport", host);
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\nAuthorization: %s\r\n", token);

    if ((fp = fetch("POST", url, headers, body)) == NULL) {
        fprintf(stderr, "Cannot post update-report\n");
        return -1;
    }
    fetchFree(fp);
    if (verbose) {
        printf("%s update status reported\n", status == 0 ? "Successful" : "Failed");
    }
    return 0;
}

/*
    Mini-fetch API. Start an HTTP action. This is NOT a generic fetch API implementation.
 */
static Fetch *fetch(char *method, char *url, char *headers, char *body)
{
    struct sockaddr_in server_addr;
    struct hostent     *server;
    Fetch              *fp;
    char               request[UBSIZE], response[UBSIZE], uri[UBSIZE];
    char               *data, *header, *host, *path, *status;
    int                fd;

    strncpy(uri, url, sizeof(uri));
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
    if ((fp = fetchAlloc(fd)) == NULL) {
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

    if (verbose) {
        printf("\nFetch Request:\n%s\n\n", request);
    }
    /*
        Write the request and wait for a response
     */
    if (fetchWrite(fp, request, strlen(request)) <= 0) {
        fetchFree(fp);
        return NULL;
    }
    memset(response, 0, UBSIZE);
    if (fetchRead(fp, response, UBSIZE - 1) < 0) {
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
    fp->response = strdup(response);
    if (verbose) {
        printf("Fetch response:\n%s\n\n", response);
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
    if ((header = fetchHeader(fp, "Content-Length")) != NULL) {
        fp->contentLength = atoi(header);
        free(header);
        if (fp->contentLength) {
            fp->firstBody = strdup(data);
        }
    }
    return fp;
}

/*
    Return a small response body as a string. Caller must free.
 */
static char *fetchString(Fetch *fp)
{
    char   *bp, *body;
    size_t bytes, len;

    if (fp->contentLength == 0) {
        return strdup("");
    }
    if ((body = malloc(fp->contentLength + 1)) == NULL) {
        fprintf(stderr, "Cannot allocate %d bytes", (int) fp->contentLength);
        return NULL;
    }
    bp = body;
    if (fp->firstBody) {
        //  Use the body fragment already read
        len = strlen(fp->firstBody);
        memcpy(body, fp->firstBody, len);
        bp += len;
    }
    while (len < fp->contentLength) {
        printf("READ %d\n", (int) (fp->contentLength - (bp - body)));
        if ((bytes = fetchRead(fp, bp, fp->contentLength - (bp - body))) > 0) {
            len += bytes;
            bp += bytes;
        }
    }
    return body;
}

/*
    Return a response body to a file.
 */
static int fetchFile(Fetch *fp, cchar *path)
{
    char   buf[UBSIZE], sum[EVP_MAX_MD_SIZE * 2 + 1], *body;
    int    bytes, fd, seenHeaders;
    size_t len;

    if (verbose) {
        printf("Downloading update image to %s\n", path);
    }
    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
        fprintf(stderr, "Cannot open image temp file");
        return -1;
    }
    if (fp->firstBody) {
        len = strlen(fp->firstBody);
        if (write(fd, fp->firstBody, len) < 0) {
            fprintf(stderr, "Cannot write to file");
            close(fd);
            return -1;
        }
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
static char *fetchHeader(Fetch *fp, char *key)
{
    char   *end, *start, kbuf[80], *value;
    size_t len;

    snprintf(kbuf, sizeof(kbuf), "%s:", key);
    value = 0;
    if ((start = strstr(fp->response, kbuf)) != NULL) {
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
static size_t fetchRead(Fetch *fp, char *buf, size_t buflen)
{
    size_t bytes;

    if ((bytes = SSL_read(fp->ssl, buf, buflen)) < 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return bytes;
}

/*
    Write request data
 */
static size_t fetchWrite(Fetch *fp, char *buf, size_t buflen)
{
    size_t bytes;

    if ((bytes = SSL_write(fp->ssl, buf, buflen)) <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return bytes;
}

/*
    Allocate a Fetch control structure. This primarily sets up an OpenSSL context.
 */
static Fetch *fetchAlloc(int fd)
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
        return NULL;
    }
    fp->ssl = SSL_new(fp->ctx);
    fp->fd = fd;
    SSL_set_fd(fp->ssl, fd);
    if (SSL_connect(fp->ssl) != 1) {
        ERR_print_errors_fp(stderr);
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
    if (fp->response) {
        free(fp->response);
        fp->response = NULL;
    }
    if (fp->firstBody) {
        free(fp->firstBody);
        fp->firstBody = NULL;
    }
    free(fp);
}

/*
    Trivial routine to lookup a key value in a json formatted string.
    This is NOT a generic JSON parser and does NOT handle use other than required by the Updater API.
    Caller must free result.
 */
static char *json(cchar *json, cchar *key)
{
    char *end, keybuf[80], *keyPos, *start;
    int  quoted;

    snprintf(keybuf, sizeof(keybuf), "%s\":", key);
    keyPos = strstr(json, keybuf);
    if (keyPos) {
        keyPos += strlen(key) + 2;  // assuming format is "key":<space>
        quoted = (*keyPos == '"') ? 1 : 0;
        start = quoted ? keyPos + 1 : keyPos;
        end = start;
        while (*end && ((quoted && *end != '"') || (!quoted && *end != ',' && *end != '}'))) {
            end++;
        }
        int  size = end - start;
        char *value = (char*) malloc(size + 1);
        if (value) {
            strncpy(value, start, size);
            value[size] = '\0';
            return value;
        }
    }
    return NULL;
}

/*
    Calculate a SHA-256 checksum for a file
 */
static int getFileSum(cchar *path, char sum[EVP_MAX_MD_SIZE])
{
    FILE          *file;
    EVP_MD_CTX    *mdctx;
    unsigned char buf[UBSIZE], hash[EVP_MAX_MD_SIZE];
    unsigned int  len;
    size_t        bytes;

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
        sprintf(&sum[i * 2], "%02x", hash[i]);
    }
    return 0;
}
