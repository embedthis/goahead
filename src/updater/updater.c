/*
    updater.c - Over-The-Air (OTA) software update library implementation

    This module provides a complete OTA update client for IoT devices. It communicates with the
    EmbedThis Builder cloud service to check for, download, verify, and apply firmware updates.

    Key Features:
    - Secure HTTPS communication with certificate and hostname verification
    - SHA-256 checksum verification of downloaded updates
    - Minimal dependencies - uses a custom minimal HTTP client for Builder API needs only
    - Supports custom device properties for update policy matching
    - Automatic update status reporting back to Builder service
    - Secure file handling with symlink attack prevention

    Architecture:
    - The update() function is the main public API entry point
    - Internal fetch*() functions provide a minimal HTTPS client (not a general-purpose HTTP library)
    - Uses OpenSSL for TLS 1.2+ and cryptographic operations
    - Designed for embedded systems with constrained resources

    Security Considerations:
    - All network communication uses HTTPS with certificate verification
    - Hostname verification via SNI and X509 verification
    - Downloaded files created with restrictive 0600 permissions
    - O_EXCL and O_NOFOLLOW flags prevent race conditions and symlink attacks
    - SHA-256 checksum verification before applying updates
    - Content-Length validation to prevent excessive memory usage

    Copyright (c) EmbedThis Software. All Rights Reserved.
 */

/********************************** Includes **********************************/

#include "updater.h"

/********************************** Locals ************************************/

#define SERVER_PORT 443
#define UBSIZE      4096
#define CA_BUNDLE   ""     // Override CA bundle file path

/**
    Fetch - Internal HTTP/HTTPS client state

    This structure maintains the state for a single HTTP request/response cycle.
    It is used exclusively by the internal fetch*() functions and is not part of the public API.

    @stability Internal
    @see fetch, fetchAlloc, fetchFree
 */
typedef struct Fetch {
    SSL_CTX *ctx;          // OpenSSL TLS context
    SSL *ssl;              // OpenSSL TLS connection handle
    Socket fd;             // TCP socket file descriptor
    size_t contentLength;  // Total response body length from Content-Length header
    size_t bodyLength;     // Length of initial body data received with headers
    char *body;            // Buffer containing initial body data (if any received with headers)
    int status;            // HTTP response status code
} Fetch;

static int verbose;        // Global flag to enable verbose tracing output (trace and errors)
static int quiet;          // Global flag to suppress all output (stdout and stderr)

/********************************** Forwards **********************************/

static int applyUpdate(cchar *path, cchar *script);
static Socket connectToHost(cchar *host);
static Fetch *fetch(cchar *method, char *url, char *headers, char *body);
static Fetch *fetchAlloc(cchar *host);
static void fetchFree(Fetch *fp);
static char *fetchString(Fetch *fp);
static int fetchFile(Fetch *fp, cchar *path);
static char *fetchHeader(cchar *response, cchar *key);
static ssize fetchRead(Fetch *fp, char *buf, size_t buflen);
static ssize fetchWrite(Fetch *fp, char *buf, size_t buflen);
static int getFileSum(cchar *path, char sum[EVP_MAX_MD_SIZE * 2 + 1]);
static char *json(cchar *jsonText, cchar *key);
static void *memdup(cvoid *ptr, size_t size);
static int parseResponseBody(Fetch *fp, char *response, ssize bytes);
static void parseUrl(char *url, char **hostOut, char **pathOut);
static int postReport(int success, cchar *host, cchar *device, cchar *update, cchar *token);
static void printSslErrors(void);
static ssize readAndValidateResponse(Fetch *fp, char *response, size_t responseSize);
static int run(cchar *script, cchar *path);
static int sendHttpRequest(Fetch *fp, cchar *method, cchar *path, cchar *host, cchar *headers, cchar *body);

/************************************ Code ************************************/
/**
    Check for and apply software updates from the EmbedThis Builder service

    This is the main entry point for the OTA update functionality. It performs the complete update workflow:
    1. Sends an update check request to the Builder service with device information
    2. If an update is available, downloads it to the specified path
    3. Verifies the download integrity using SHA-256 checksum
    4. Optionally applies the update using the provided script
    5. Reports the update result back to the Builder service

    @param host Builder cloud endpoint URL
    @param product Product ID from the Builder token list
    @param token CloudAPI access token for authentication
    @param device Unique device identifier
    @param version Current device firmware version
    @param properties Optional JSON properties for distribution policy (may be NULL)
    @param path File path to save the downloaded update
    @param script Optional script path to apply the update (may be NULL)
    @param verboseArg Non-zero to enable verbose output
    @return 0 on success, -1 on error

    SECURITY Acceptable: The developer is responsible for validating the inputs to this function.

    @ingroup Updater
 */
int update(cchar *host, cchar *product, cchar *token, cchar *device, cchar *version,
           cchar *properties, cchar *path, cchar *script, int verboseArg, int quietArg)
{
    Fetch *fp;
    char  body[UBSIZE], url[UBSIZE], headers[256], fileSum[EVP_MAX_MD_SIZE * 2 + 1];
    char  *checksum, *downloadUrl, *response, *update, *updateVersion;
    int   count, rc, status;

#if ME_WIN_LIKE
    WSADATA wsaData;
    int     wsaInitialized = 0;
#endif
    fp = NULL;
    rc = -1;
    checksum = downloadUrl = response = update = updateVersion = NULL;

    /*
        Block to make error handling easier
     */
    do {
        if (!host || !product || !token || !device || !version || !path) {
            if (!quietArg) {
                fprintf(stderr, "Bad update args\n");
            }
            break;
        }
        verbose = verboseArg;
        quiet = quietArg;

#if ME_WIN_LIKE
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            if (!quiet) {
                fprintf(stderr, "WSAStartup failed\n");
            }
            break;
        }
        wsaInitialized = 1;
#endif

        /*
            Issue update request to determine if there is an update.
            Authentication is using the CloudAPI builder token.
         */
        count = snprintf(url, sizeof(url), "%s/tok/provision/update", host);
        if (count < 0 || (size_t) count >= sizeof(url)) {
            if (!quiet) {
                fprintf(stderr, "Host URL is too long\n");
            }
            break;
        }
        /*
            SECURITY Acceptable: - The developer is responsible for validating the inputs to this function.
         */
        count = snprintf(body, sizeof(body),
                         "{\"id\":\"%s\",\"product\":\"%s\",\"version\":\"%s\"%s%s}",
                         device, product, version,
                         properties ? "," : "",
                         properties ? properties : "");
        if (count < 0 || (size_t) count >= sizeof(body)) {
            if (!quiet) {
                fprintf(stderr, "Request body is too long\n");
            }
            break;
        }
        count = snprintf(headers, sizeof(headers), "Content-Type: application/json\r\nAuthorization: %s\r\n", token);
        if (count < 0 || (size_t) count >= sizeof(headers)) {
            if (!quiet) {
                fprintf(stderr, "Headers buffer too small\n");
            }
            break;
        }
        if (verbose) {
            printf("\nCheck for update at: %s\n", url);
        }
        if ((fp = fetch("POST", url, headers, body)) == NULL) {
            break;
        }
        if ((response = fetchString(fp)) == NULL) {
            break;
        }
        fetchFree(fp);
        fp = NULL;

        /*
            If an update is available, the "url" will be defined to point to the update image
            The "update" field contains the selected update ID and is use when posting update status.
         */
        if ((downloadUrl = json(response, "url")) == NULL) {
            free(response);
            response = NULL;
            rc = 0;
            break;
        }

        checksum = json(response, "checksum");
        update = json(response, "update");
        updateVersion = json(response, "version");
        free(response);
        response = NULL;

        if (!checksum || !update || !updateVersion) {
            if (!quiet) {
                fprintf(stderr, "Incomplete update response\n");
            }
            break;
        }

        count = snprintf(headers, sizeof(headers), "Accept: */*\r\n");
        if (count < 0 || (size_t) count >= sizeof(headers)) {
            if (!quiet) {
                fprintf(stderr, "Headers buffer too small\n");
            }
            break;
        }

        if (strncmp(downloadUrl, "https://", 8) != 0) {
            if (!quiet) {
                fprintf(stderr, "Insecure download URL (HTTPS required)\n");
            }
            break;
        }
        if ((fp = fetch("GET", downloadUrl, headers, NULL)) == NULL) {
            break;
        }
        //  Fetch the update and save to the given path
        if (fetchFile(fp, path) < 0) {
            break;
        }
        fetchFree(fp);
        fp = NULL;

        if (verbose) {
            printf("Verify update checksum in %s\n", path);
        }
        if (getFileSum(path, fileSum) != 0) {
            break;
        }
        if (strcmp(fileSum, checksum) != 0) {
            if (!quiet) {
                fprintf(stderr, "Checksum does not match\n%s vs\n%s\n", fileSum, checksum);
            }
            break;
        }
        if (script) {
            status = applyUpdate(path, script);
            if (postReport(status, host, device, update, token) < 0) {
                break;
            }
        }
        rc = 0;

    } while (0);

    if (fp) {
        fetchFree(fp);
    }
    if (downloadUrl) {
        free(downloadUrl);
    }
    if (checksum) {
        free(checksum);
    }
    if (update) {
        free(update);
    }
    if (updateVersion) {
        free(updateVersion);
    }
    if (response) {
        free(response);
    }
#if ME_WIN_LIKE
    if (wsaInitialized) {
        WSACleanup();
    }
#endif
    return rc;
}

/**
    Apply the update by invoking the configured update script

    Executes the update script with the update file path as an argument. The script is responsible
    for installing the update and may exit or reboot the device as needed.

    @param path File path to the downloaded update
    @param script Path to the executable update script
    @return 0 on success, -1 on error
    @stability Internal
 */
static int applyUpdate(cchar *path, cchar *script)
{
    int status;

    if (verbose) {
        printf("Applying update: %s %s\n", script, path);
    }
    status = run(script, path);
    if (verbose) {
        printf("Update %s\n", status == 0 ? "Successful" : "Failed");
    }
    return status;
}

/**
    Execute the update script in a child process

    Uses fork()/execvp() on Unix-like systems and CreateProcess() on Windows to safely execute
    the update script without shell interpretation. The script receives the update file path as
    its only argument.

    @param script Path to the executable script
    @param path Update file path to pass as argument
    @return Exit status of the script (0 on success), -1 on error

    SECURITY Acceptable: On Unix systems, uses fork()/execvp() to avoid shell. On Windows, uses
    CreateProcess() with quoted arguments to prevent command injection. The inputs are all from
    developer controlled input and not user controlled.

    @stability Internal
 */
static int run(cchar *script, cchar *path)
{
#if ME_UNIX_LIKE
    char  *args[] = { (char*) script, (char*) path, NULL };
    int   status;
    pid_t pid;

    if ((pid = fork()) < 0) {
        if (!quiet) {
            fprintf(stderr, "Cannot fork to run command\n");
        }
        return -1;
    }
    if (pid == 0) {
        execvp(script, args);
        if (!quiet) {
            fprintf(stderr, "Cannot run command\n");
        }
        _exit(127);
    }
    if (waitpid(pid, &status, 0) == -1) {
        if (!quiet) {
            fprintf(stderr, "Cannot wait for command\n");
        }
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;

#elif ME_WIN_LIKE
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    char                cmdLine[1024];
    DWORD               exitCode;
    int                 result;
    cchar               *scriptExt;

    /*
        Build command line: "script" "path"
        Quote both arguments to handle spaces in paths
        For .sh files, prepend bash to handle shell scripts in MinGW environment
     */
    scriptExt = strrchr(script, '.');
    if (scriptExt && strcmp(scriptExt, ".sh") == 0) {
        result = snprintf(cmdLine, sizeof(cmdLine), "bash \"%s\" \"%s\"", script, path);
    } else {
        result = snprintf(cmdLine, sizeof(cmdLine), "\"%s\" \"%s\"", script, path);
    }
    if (result < 0 || (size_t) result >= sizeof(cmdLine)) {
        if (!quiet) {
            fprintf(stderr, "Command line too long\n");
        }
        return -1;
    }

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    /*
        CreateProcess with NULL for lpApplicationName uses the first token from lpCommandLine
        This avoids shell interpretation while allowing proper argument parsing
     */
    if (!CreateProcess(
            NULL,           // lpApplicationName - use command line
            cmdLine,        // lpCommandLine - contains executable and arguments
            NULL,           // lpProcessAttributes
            NULL,           // lpThreadAttributes
            FALSE,          // bInheritHandles
            0,              // dwCreationFlags - no special flags
            NULL,           // lpEnvironment - inherit parent environment
            NULL,           // lpCurrentDirectory - inherit parent directory
            &si,            // lpStartupInfo
            &pi             // lpProcessInformation
            )) {
        if (!quiet) {
            fprintf(stderr, "Cannot create process: error %lu\n", GetLastError());
        }
        return -1;
    }

    /*
        Wait for the process to complete
     */
    WaitForSingleObject(pi.hProcess, INFINITE);

    /*
        Get the exit code
     */
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        if (!quiet) {
            fprintf(stderr, "Cannot get exit code: error %lu\n", GetLastError());
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int) exitCode;

#else
    #error "ERROR: Secure process creation not implemented for this platform"
    return -1;
#endif
}

/**
    Post update status report to the Builder service

    Sends the update result (success or failure) back to the Builder service for metrics
    and device version tracking.

    @param status Update status (0 for success, non-zero for failure)
    @param host Builder cloud endpoint URL
    @param device Unique device identifier
    @param update Update ID from the Builder response
    @param token CloudAPI access token
    @return 0 on success, -1 on error
    @stability Internal
 */
static int postReport(int status, cchar *host, cchar *device, cchar *update, cchar *token)
{
    Fetch *fp;
    char  body[UBSIZE], url[256], headers[256];
    int   count;

    count = snprintf(body, sizeof(body), "{\"success\":%s,\"id\":\"%s\",\"update\":\"%s\"}",
                     status == 0 ? "true" : "false", device, update);
    if (count < 0 || (size_t) count >= sizeof(body)) {
        if (!quiet) {
            fprintf(stderr, "Report body is too long\n");
        }
        return -1;
    }
    count = snprintf(url, sizeof(url), "%s/tok/provision/updateReport", host);
    if (count < 0 || (size_t) count >= sizeof(url)) {
        if (!quiet) {
            fprintf(stderr, "Report URL is too long\n");
        }
        return -1;
    }
    count = snprintf(headers, sizeof(headers), "Content-Type: application/json\r\nAuthorization: %s\r\n", token);
    if (count < 0 || (size_t) count >= sizeof(headers)) {
        if (!quiet) {
            fprintf(stderr, "Report headers buffer too small\n");
        }
        return -1;
    }

    if ((fp = fetch("POST", url, headers, body)) == NULL) {
        if (!quiet) {
            fprintf(stderr, "Cannot post update-report\n");
        }
        return -1;
    }
    fetchFree(fp);
    return 0;
}

/**
    Parse URL to extract host and path components

    Modifies the url string in place to separate host from path.
    Strips https:// prefix if present.

    @param url URL string to parse (modified in place)
    @param hostOut Pointer to receive host component
    @param pathOut Pointer to receive path component (or empty string if no path)
    @stability Internal
 */
static void parseUrl(char *url, char **hostOut, char **pathOut)
{
    static char emptyPath[] = "";
    char        *host, *path;

    if ((host = strstr(url, "https://")) != NULL) {
        host += 8;
    } else {
        host = url;
    }
    if ((path = strchr(host, '/')) != NULL) {
        *path++ = '\0';
    } else {
        path = emptyPath;
    }
    *hostOut = host;
    *pathOut = path;
}

/**
    Create socket and establish TCP connection to host

    Resolves hostname via DNS and connects to SERVER_PORT (443).

    @param host Hostname to connect to
    @return Connected socket descriptor on success, -1 on error
    @stability Internal
 */
static Socket connectToHost(cchar *host)
{
    struct sockaddr_in server_addr;
    struct hostent     *server;
    Socket             fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (!quiet) {
            perror("Cannot open socket");
        }
        return -1;
    }
    server = gethostbyname(host);
    if (server == NULL) {
        closesocket(fd);
        if (!quiet) {
            fprintf(stderr, "Cannot find host\n");
        }
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, (size_t) server->h_length);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        if (!quiet) {
            perror("Error connecting to host");
        }
        closesocket(fd);
        return -1;
    }
    return fd;
}

/**
    Format and send HTTP request

    Constructs HTTP/1.1 request with given method, path, headers, and optional body.

    @param fp Fetch structure with active TLS connection
    @param method HTTP method (e.g., "GET", "POST")
    @param path URL path component
    @param host Hostname for Host header
    @param headers Additional HTTP headers (must end with \r\n)
    @param body Optional request body (may be NULL)
    @return 0 on success, -1 on error
    @stability Internal
 */
static int sendHttpRequest(Fetch *fp, cchar *method, cchar *path, cchar *host, cchar *headers, cchar *body)
{
    char request[UBSIZE];

    snprintf(request, sizeof(request),
             "%s /%s HTTP/1.1\r\n" \
             "Host: %s\r\n" \
             "Content-Length: %d\r\n" \
             "%s\r\n" \
             "%s",
             method, path, host, body ? (int) strlen(body) : 0, headers, body ? body : "");

    if (fetchWrite(fp, request, strlen(request)) <= 0) {
        return -1;
    }
    return 0;
}

/**
    Read and validate HTTP response

    Reads initial response data and validates HTTP/1.1 protocol and status code.

    @param fp Fetch structure with active TLS connection
    @param response Buffer to receive response data
    @param responseSize Size of response buffer
    @return Number of bytes read on success, -1 on error
    @stability Internal
 */
static ssize readAndValidateResponse(Fetch *fp, char *response, size_t responseSize)
{
    char  *status;
    ssize bytes;

    memset(response, 0, responseSize);
    if ((bytes = fetchRead(fp, response, responseSize - 1)) < 0) {
        return -1;
    }
    if (strncmp(response, "HTTP/1.1 ", 9) != 0) {
        if (!quiet) {
            fprintf(stderr, "Bad response\n%s\n", response);
        }
        return -1;
    }
    if ((status = strchr(response, ' ')) == NULL) {
        if (!quiet) {
            fprintf(stderr, "Bad response\n%s\n", response);
        }
        return -1;
    }
    fp->status = atoi(++status);
    if (fp->status != 200) {
        if (!quiet) {
            fprintf(stderr, "Bad response status %d\n%s\n", fp->status, response);
        }
        return -1;
    }
    return bytes;
}

/**
    Parse response body and Content-Length header

    Extracts Content-Length from headers and handles any body fragment
    that was received with the headers.

    @param fp Fetch structure to populate with body information
    @param response Response buffer containing headers and possible body fragment
    @param bytes Total bytes read into response buffer
    @return 0 on success, -1 on error
    @stability Internal
 */
static int parseResponseBody(Fetch *fp, char *response, ssize bytes)
{
    char   *data, *header;
    size_t headerBytes;
    int    contentLength;

    if ((data = strstr(response, "\r\n\r\n")) == NULL) {
        if (!quiet) {
            fprintf(stderr, "Bad response\n%s\n", response);
        }
        return -1;
    }
    *data = '\0';
    data += 4;

    if ((header = fetchHeader(response, "Content-Length")) == NULL) {
        if (!quiet) {
            fprintf(stderr, "Missing Content-Length\n");
        }
        return -1;
    }
    contentLength = atoi(header);
    free(header);

    if (contentLength < 0 || contentLength > 100 * 1024 * 1024) {
        if (!quiet) {
            fprintf(stderr, "Invalid Content-Length\n");
        }
        return -1;
    }
    fp->contentLength = (size_t) contentLength;

    headerBytes = (size_t) (data - response);
    if ((size_t) bytes > headerBytes) {
        fp->bodyLength = ((size_t) bytes - headerBytes);
        fp->body = memdup(data, fp->bodyLength);
        if (!fp->body) {
            if (!quiet) {
                fprintf(stderr, "Cannot allocate body buffer\n");
            }
            return -1;
        }
    } else {
        fp->bodyLength = 0;
        fp->body = NULL;
    }
    return 0;
}

/**
    Perform an HTTPS request to the specified URL

    This is a minimal HTTPS client designed specifically for the Builder API needs.
    It is NOT a general-purpose HTTP library. The function:
    - Establishes a TLS connection with certificate verification
    - Sends the HTTP request with the specified method, headers, and body
    - Reads the response headers and initial body data
    - Returns a Fetch structure for further processing

    @param method HTTP method (e.g., "GET", "POST")
    @param url Full URL including https:// scheme
    @param headers Additional HTTP headers (must end with \r\n)
    @param body Request body (may be NULL for GET requests)
    @return Fetch structure on success, NULL on error
    @stability Internal
 */
static Fetch *fetch(cchar *method, char *url, char *headers, char *body)
{
    Fetch *fp;
    char  uri[UBSIZE], response[UBSIZE];
    char  *host, *path;
    ssize bytes;

    snprintf(uri, sizeof(uri), "%s", url);
    if (verbose) {
        printf("Fetching %s\n", uri);
    }
    parseUrl(uri, &host, &path);

    if ((fp = fetchAlloc(host)) == NULL) {
        return NULL;
    }
    if (sendHttpRequest(fp, method, path, host, headers, body) < 0) {
        fetchFree(fp);
        return NULL;
    }
    if ((bytes = readAndValidateResponse(fp, response, sizeof(response))) < 0) {
        fetchFree(fp);
        return NULL;
    }
    if (parseResponseBody(fp, response, bytes) < 0) {
        fetchFree(fp);
        return NULL;
    }
    return fp;
}

/**
    Read the complete response body as a string

    Reads all remaining response data and returns it as a null-terminated string.
    Caller is responsible for freeing the returned string.

    @param fp Fetch structure from a successful fetch() call
    @return Dynamically allocated string containing the response body, or NULL on error
    @stability Internal
 */
static char *fetchString(Fetch *fp)
{
    char   *bp, *body;
    size_t bytes, len;
    ssize  readBytes;

    if (fp->contentLength == 0) {
        char *empty = strdup("");
        if (!empty) {
            if (!quiet) {
                fprintf(stderr, "Cannot allocate memory for empty string\n");
            }
        }
        return empty;
    }
    if ((body = malloc(fp->contentLength + 1)) == NULL) {
        if (!quiet) {
            fprintf(stderr, "Cannot allocate %d bytes", (int) fp->contentLength);
        }
        return NULL;
    }
    len = 0;
    bp = body;
    if (fp->body && fp->bodyLength > 0) {
        //  Use the body fragment already read with the headers
        bytes = (fp->bodyLength < fp->contentLength) ? fp->bodyLength : fp->contentLength;
        memcpy(body, fp->body, bytes);
        len = (size_t) bytes;
        bp += bytes;
        free(fp->body);
        fp->body = NULL;
    }
    while (len < fp->contentLength) {
        if ((readBytes = fetchRead(fp, bp, fp->contentLength - (size_t) (bp - body))) > 0) {
            len += (size_t) readBytes;
            bp += readBytes;
        } else {
            if (!quiet) {
                fprintf(stderr, "Connection closed or error while reading response body\n");
            }
            free(body);
            return NULL;
        }
    }
    body[fp->contentLength] = '\0';
    return body;
}

/**
    Save the response body to a file

    Downloads the response body to the specified file path with secure file creation.
    Uses O_EXCL and O_NOFOLLOW to prevent race conditions and symlink attacks.
    Files are created with 0600 permissions (owner read/write only).

    @param fp Fetch structure from a successful fetch() call
    @param path Destination file path
    @return 0 on success, -1 on error
    @stability Internal
 */
static int fetchFile(Fetch *fp, cchar *path)
{
    struct stat st;
    char        buf[UBSIZE];
    size_t      writeLen;
    ssize       bytes;
    int         fd, flags;

    if (strncmp(path, "/tmp/", 5) == 0) {
        if (!quiet) {
            fprintf(stderr, "WARNING: Saving update to /tmp is insecure due to potential symlink attacks.\n");
        }
    }
    if (verbose) {
        printf("Downloading update to %s\n", path);
    }
    unlink(path);
    flags = O_WRONLY | O_CREAT | O_EXCL | O_BINARY;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    if ((fd = open(path, flags, 0600)) < 0) {
        if (!quiet) {
            fprintf(stderr, "Cannot open image temp file\n");
        }
        return -1;
    }
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        if (!quiet) {
            fprintf(stderr, "Refusing to write to non-regular file\n");
        }
        close(fd);
        return -1;
    }
    if (fp->body) {
        //  Write the body fragment already read with the headers
        writeLen = (fp->bodyLength < fp->contentLength) ? fp->bodyLength : fp->contentLength;
        if (write(fd, fp->body, (uint) writeLen) < 0) {
            if (!quiet) {
                fprintf(stderr, "Cannot write to file");
            }
            close(fd);
            return -1;
        }
        free(fp->body);
        fp->body = NULL;
    }
    while ((bytes = fetchRead(fp, buf, sizeof(buf))) > 0) {
        if (write(fd, buf, (uint) bytes) != (uint) bytes) {
            if (!quiet) {
                fprintf(stderr, "Cannot save response");
            }
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}

/**
    Extract an HTTP header value from the response

    Parses the response headers and returns the value for the specified header name.
    Caller is responsible for freeing the returned string.

    @param response Response headers as a string
    @param key Header name (e.g., "Content-Length")
    @return Dynamically allocated header value, or NULL if not found
    @stability Internal
 */
static char *fetchHeader(cchar *response, cchar *key)
{
    char   *end, *start, kbuf[80], *value;
    size_t len;

    snprintf(kbuf, sizeof(kbuf), "%s:", key);
    value = 0;
    if ((start = strstr(response, kbuf)) != NULL) {
        if ((end = strstr(start, "\r\n")) != NULL) {
            start += strlen(kbuf);
            while (start < end && isspace(*start)) start++;
            len = (size_t) (end - start);
            value = malloc(len + 1);
            if (!value) {
                if (!quiet) {
                    fprintf(stderr, "Cannot allocate header value\n");
                }
                return NULL;
            }
            strncpy(value, start, len);
            value[len] = '\0';
        }
    }
    return value;
}

/**
    Read data from the TLS connection

    Wrapper around SSL_read() for reading response data.

    @param fp Fetch structure
    @param buf Buffer to receive data
    @param buflen Maximum bytes to read
    @return Number of bytes read, or -1 on error
    @stability Internal
 */
static ssize fetchRead(Fetch *fp, char *buf, size_t buflen)
{
    ssize rc;

    rc = SSL_read(fp->ssl, buf, (int) buflen);
    if (rc <= 0) {
        if (!quiet) {
            printSslErrors();
        }
        return -1;
    }
    return rc;
}

/**
    Write data to the TLS connection

    Wrapper around SSL_write() for sending request data.

    @param fp Fetch structure
    @param buf Buffer containing data to write
    @param buflen Number of bytes to write
    @return Number of bytes written, or -1 on error
    @stability Internal
 */
static ssize fetchWrite(Fetch *fp, char *buf, size_t buflen)
{
    ssize rc;

    rc = SSL_write(fp->ssl, buf, (int) buflen);
    if (rc <= 0) {
        if (!quiet) {
            printSslErrors();
        }
        return -1;
    }
    return rc;
}

/**
    Allocate and initialize a Fetch structure with a socket connection and TLS context

    Creates a new Fetch structure and configures OpenSSL for secure communication:
    - Enforces TLS 1.2 or higher
    - Enables certificate verification
    - Configures hostname verification via SNI
    - Establishes the TLS connection

    @param fd Connected TCP socket file descriptor
    @param host Hostname for SNI and certificate verification
    @return Initialized Fetch structure on success, NULL on error
    @stability Internal
 */
static Fetch *fetchAlloc(cchar *host)
{
    Fetch             *fp;
    X509_VERIFY_PARAM *param;
    const SSL_METHOD  *method;

    fp = malloc(sizeof(Fetch));
    if (!fp) {
        if (!quiet) {
            fprintf(stderr, "Cannot allocate Fetch structure\n");
        }
        return NULL;
    }
    memset(fp, 0, sizeof(Fetch));

    if ((fp->fd = connectToHost(host)) < 0) {
        fetchFree(fp);
        return NULL;
    }
    method = TLS_client_method();
    fp->ctx = SSL_CTX_new(method);
    if (!fp->ctx) {
        if (!quiet) {
            perror("Unable to create SSL context");
            printSslErrors();
        }
        fetchFree(fp);
        return NULL;
    }
    /* Enforce modern TLS */
    SSL_CTX_set_min_proto_version(fp->ctx, TLS1_2_VERSION);

    /*
        Verify server certificate using system default CA certificates
     */
    SSL_CTX_set_verify(fp->ctx, SSL_VERIFY_PEER, NULL);

    /*
        Override CA bundle if provided
     */
    if (CA_BUNDLE && *CA_BUNDLE) {
        if (!SSL_CTX_load_verify_locations(fp->ctx, CA_BUNDLE, NULL)) {
            if (!quiet) {
                fprintf(stderr, "Unable to set certificate locations: %s", CA_BUNDLE);
                printSslErrors();
            }
            fetchFree(fp);
            return NULL;
        }
    } else if (!SSL_CTX_set_default_verify_paths(fp->ctx)) {
        /*
            OpenSSL uses the env vars: SSL_CERT_FILE and SSL_CERT_DIR to override the default CA bundle locations.
            Windows may not have a default defined.
         */
        if (!quiet) {
            fprintf(stderr, "Cannot load system CA certificates.\n");
            printSslErrors();
        }
        fetchFree(fp);
        return NULL;
    }

    fp->ssl = SSL_new(fp->ctx);
    SSL_set_fd(fp->ssl, fp->fd);
    /*
        Send SNI - SSL_set_tlsext_host_name macro casts to void*, triggering const warning.
        This is an OpenSSL API limitation, not a security issue.
     */
    SSL_set_tlsext_host_name(fp->ssl, host);

    /*
        Verify hostname
     */
    param = SSL_get0_param(fp->ssl);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
#endif
    if (!X509_VERIFY_PARAM_set1_host(param, host, 0)) {
        if (!quiet) {
            fprintf(stderr, "Failed to set hostname for verification\n");
            printSslErrors();
        }
        fetchFree(fp);
        return NULL;
    }
    if (SSL_connect(fp->ssl) != 1) {
        if (!quiet) {
            printSslErrors();
        }
        fetchFree(fp);
        return NULL;
    }
    return fp;
}

/**
    Free a Fetch structure and release all resources

    Closes the TLS connection, frees the SSL context, closes the socket,
    and releases all allocated memory.

    @param fp Fetch structure to free (may be NULL)
    @stability Internal
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
        closesocket(fp->fd);
        fp->fd = -1;
    }
    if (fp->body) {
        free(fp->body);
        fp->body = NULL;
    }
    free(fp);
}

/**
    Extract a value from a JSON string

    This is a minimal JSON parser designed specifically for the Builder API responses.
    It is NOT a general-purpose JSON parser and only handles simple key-value extraction.
    Caller is responsible for freeing the returned string.

    @param jsonText JSON formatted string
    @param key Key name to extract
    @return Dynamically allocated value string, or NULL if key not found
    @stability Internal
 */
static char *json(cchar *jsonText, cchar *key)
{
    size_t size;
    char   *end, keybuf[80], *keyPos, *start, *value, *vbuf;
    int    quoted, count;

    count = snprintf(keybuf, sizeof(keybuf), "\"%s\":", key);
    if (count < 0 || (size_t) count >= sizeof(keybuf)) {
        if (!quiet) {
            fprintf(stderr, "Key is too long\n");
        }
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
        size = (size_t) (end - start);
        vbuf = (char*) malloc(size + 1);
        if (!vbuf) {
            if (!quiet) {
                fprintf(stderr, "Cannot allocate memory for JSON value\n");
            }
            return NULL;
        }
        strncpy(vbuf, start, size);
        vbuf[size] = '\0';
        return vbuf;
    }
    return NULL;
}

/**
    Calculate the SHA-256 checksum of a file

    Reads the file and computes its SHA-256 hash using OpenSSL EVP functions.
    The result is returned as a hexadecimal string.

    @param path File path to checksum
    @param sum Buffer to receive the hexadecimal checksum string (must be at least EVP_MAX_MD_SIZE * 2 + 1 bytes)
    @return 0 on success, -1 on error
    @stability Internal
 */
static int getFileSum(cchar *path, char sum[EVP_MAX_MD_SIZE * 2 + 1])
{
    EVP_MD_CTX *mdctx;
    FILE       *file;
    uchar      buf[UBSIZE], hash[EVP_MAX_MD_SIZE];
    uint       len, i;
    size_t     bytes;

    file = fopen(path, "rb");
    if (!file) {
        if (!quiet) {
            fprintf(stderr, "Cannot open %s\n", path);
        }
        return -1;
    }
    mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        if (!quiet) {
            fprintf(stderr, "Failed to create EVP_MD_CTX");
        }
        fclose(file);
        return -1;
    }
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        if (!quiet) {
            fprintf(stderr, "DigestInit error\n");
        }
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return -1;
    }
    while ((bytes = fread(buf, 1, sizeof(buf), file)) > 0) {
        if (EVP_DigestUpdate(mdctx, buf, bytes) != 1) {
            if (!quiet) {
                fprintf(stderr, "DigestUpdate error\n");
            }
            EVP_MD_CTX_free(mdctx);
            fclose(file);
            return -1;
        }
    }
    if (EVP_DigestFinal_ex(mdctx, hash, &len) != 1) {
        if (!quiet) {
            fprintf(stderr, "DigestFinal error\n");
        }
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return -1;
    }
    EVP_MD_CTX_free(mdctx);
    fclose(file);

    for (i = 0; i < len; i++) {
        snprintf(&sum[i * 2], 3, "%02x", hash[i]);
    }
    sum[len * 2] = '\0';
    return 0;
}

/**
    Duplicate a memory block

    Allocates a new buffer and copies the specified memory region. Adds a null terminator
    for string safety (size + 1 bytes allocated).

    @param ptr Pointer to memory to duplicate
    @param size Number of bytes to copy
    @return Pointer to duplicated memory, or NULL on error
    @stability Internal
 */
static void *memdup(cvoid *ptr, size_t size)
{
    char *newp;

    if (ptr == NULL) {
        return NULL;
    }
    newp = malloc(size + 1);
    if (!newp) {
        if (!quiet) {
            fprintf(stderr, "Cannot allocate memory for memdup\n");
        }
        return NULL;
    }
    memcpy(newp, ptr, size);
    newp[size] = '\0';
    return newp;
}

/**
    Print OpenSSL error messages safely

    This function provides a Windows-safe alternative to ERR_print_errors_fp(stderr).
    On Windows, ERR_print_errors_fp can crash when stderr is not properly initialized
    or redirected. This function uses ERR_error_string_n to get error messages and
    prints them using fprintf, which is more reliable across platforms.

    @stability Internal
 */
static void printSslErrors(void)
{
    char  buf[256];
    ulong err;

    while ((err = ERR_get_error()) != 0) {
        ERR_error_string_n(err, buf, sizeof(buf));
        fprintf(stderr, "OpenSSL error: %s\n", buf);
    }
}
