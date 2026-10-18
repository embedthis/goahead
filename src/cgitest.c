/*
    cgitest.c - Test CGI program

    Copyright (c) All Rights Reserved. See details at the end of the file.

    Usage:
        cgitest [switches]
            -a                  Output the args (used for ISINDEX queries)
            -b bytes            Output content "bytes" long
            -B bytes            Output exactly "bytes" body bytes, where byte N is '0' + (N % 10).
                                Independent of -L, so a response can declare one length and write
                                another, at any size (used by the response framing tests)
            -c type             Output "type" as the Content-type header value
            -e                  Output the environment
            -E text             Write "text" and a newline to stderr before the response
            -h lines            Output header "lines" long
            -H header           Output "header" verbatim as a response header line. The value is
                                percent-descaped first, so %0d%0a delivers a raw CRLF into the
                                header block (used by the response-splitting regression tests)
            -l location         Output "location" header
            -L length           Output "length" verbatim as the Content-length header value. Not
                                derived from the body, so it may be honest, wrong or unparsable
            -n                  Non-parsed-header ouput
            -p                  Ouput the post data
            -q                  Ouput the query data
            -s status           Output "status" header
            -S                  Output the state the program started with: the SIGPIPE disposition,
                                the number of blocked signals and the number of open descriptors
                                above 2 (POSIX)
            -t timeout          Sleep "timeout" seconds before writing anything. Used to hold a
                                request open past the server's request timeout
            -w                  Output the current working directory
            -x                  Exit without writing anything, to stdout or stderr
            default             Output args, env and query

        Alternatively, pass the arguments as an environment variable HTTP_SWITCHES="-a -e -q"
 */

/********************************** Includes **********************************/

#define _CRT_SECURE_NO_WARNINGS 1
#ifndef _VSB_CONFIG_FILE
    #define _VSB_CONFIG_FILE    "vsbConfig.h"
#endif

#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if _WIN32 || WINCE
#if !WINCE
#include <direct.h>
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>

    #define access  _access
    #define close   _close
    #define fileno  _fileno
    #define fstat   _fstat
    #define getpid  _getpid
    #define open    _open
    #define putenv  _putenv
    #define read    _read
    #define stat    _stat
    #define umask   _umask
    #define unlink  _unlink
    #define write   _write
    #define strdup  _strdup
    #define lseek   _lseek
    #define getcwd  _getcwd
    #define chdir   _chdir
    #define strnset _strnset
    #define chmod   _chmod

    #define mkdir(a, b) _mkdir(a)
    #define rmdir(a)    _rmdir(a)
typedef int ssize_t;
#else
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

/*
    cgitest is a standalone CGI program and does not include osdep.h, so it carries its own copy of
    the printf format-checking macros. Keep these in step with osdep.h: PRINTF_ATTRIBUTE after the
    parameter list for GCC and Clang, PRINTF_FORMAT before the format parameter for MSVC.
 */
#ifndef PRINTF_ATTRIBUTE
    #if (__GNUC__ >= 3) || defined(__clang__) || MACOSX
        #define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format(__printf__, a1, a2)))
    #else
        #define PRINTF_ATTRIBUTE(a1, a2)
    #endif
#endif

#ifndef PRINTF_FORMAT
    #if defined(_MSC_VER) && !defined(__clang__)
        #include <sal.h>
        #define PRINTF_FORMAT _Printf_format_string_
    #else
        #define PRINTF_FORMAT
    #endif
#endif

#if _WIN32 || WINCE
    #define sleepSeconds(secs) Sleep((secs) * 1000)
#else
    #define sleepSeconds(secs) sleep(secs)
#endif

/*********************************** Locals ***********************************/

#define CMD_VXWORKS_EOF     "_ _EOF_ _"
#define CMD_VXWORKS_EOF_LEN 9
#define MAX_ARGV            64

static char *argvList[MAX_ARGV];
static int      getArgv(int *argc, char ***argv, int argcIn, char **argvIn);
static int    hasError;
static int    nonParsedHeader;
static int    numPostKeys;
static int    numQueryKeys;
static int    originalArgc;
static char   **originalArgv;
static int    outputArgs, outputEnv, outputPost, outputQuery;
static int    outputLines, outputHeaderLines, responseStatus;
static int    outputBodyBytes;
static int    outputSignals;
static int    outputCwd;
static int    outputNothing;
static char   *outputContentLength;
static char   *outputContentType;
static char   *outputHeader;
static char   *outputLocation;
static char   *outputRaw;
static char   *outputStderr;
static char   *postBuf;
static size_t postBufLen;
static char   **postKeys;
static char   *queryBuf;
static size_t queryLen;
static char   **queryKeys;
static char   *responseMsg;
static int    switchesFromHttp;
static int    timeout;

/***************************** Forward Declarations ***************************/

static void     error(PRINTF_FORMAT char *fmt, ...) PRINTF_ATTRIBUTE(1, 2);
static void     descape(char *src);
static char     hex2Char(char *s);
static int      getVars(char ***cgiKeys, char *buf, size_t len);
static int      getPostData(char **buf, size_t *len);
static int      getQueryString(char **buf, size_t *len);
static void     printEnv(char **env);
static void     printHtml(const char *s);
static void     writeHtml(const char *buf, size_t len);
static void     printVar(const char *prefix, const char *name, const char *value, const char *suffix);
static void     printQuery();
static void     printPost(char *buf, size_t len);
static void     printStartState(void);
static void     printCwd(void);
static char *safeGetenv(const char *key);
static void     finish(void);
static void     writeBody(int bytes);

/******************************************************************************/
/*
    Test program entry point
 */
#if VXWORKS
int cgitest(int argc, char **argv, char **envp)
#else
int main(int argc, char **argv, char **envp)
#endif
{
    char *cp, *method;
    int  i, j, err;

    err = 0;
    outputArgs = outputQuery = outputEnv = outputPost = 0;
    outputLines = outputHeaderLines = responseStatus = 0;
    outputBodyBytes = 0;
    outputSignals = 0;
    outputCwd = 0;
    outputNothing = 0;
    outputContentLength = 0;
    outputHeader = 0;
    outputLocation = 0;
    outputRaw = 0;
    outputStderr = 0;
    nonParsedHeader = 0;
    responseMsg = 0;
    hasError = 0;
    timeout = 0;
    queryBuf = 0;
    queryLen = 0;
    numQueryKeys = numPostKeys = 0;

    originalArgc = argc;
    originalArgv = argv;

#if _WIN32 && !WINCE
    _setmode(0, O_BINARY);
    _setmode(1, O_BINARY);
    _setmode(2, O_BINARY);
#endif
    if (strstr(argv[0], "nph-") != 0) {
        nonParsedHeader++;
    }
    if (getArgv(&argc, &argv, originalArgc, originalArgv) < 0) {
        error("Can't read CGI input");
    }
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            continue;
        }
        for (cp = &argv[i][1]; *cp; cp++) {
            switch (*cp) {
            case 'a':
                outputArgs++;
                break;

            case 'b':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    outputLines = atoi(argv[i]);
                }
                break;

            case 'B':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    outputBodyBytes = atoi(argv[i]);
                }
                break;

            case 'c':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    outputContentType = argv[i];
                }
                break;

            case 'e':
                outputEnv++;
                break;

            case 'E':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    outputStderr = argv[i];
                }
                break;

            case 'h':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    outputHeaderLines = atoi(argv[i]);
                    nonParsedHeader++;
                }
                break;

            case 'H':
                /*
                    getArgv descapes the whole switch string once and then splits it on whitespace,
                    so a caller wanting a literal CRLF in the header double-encodes it as %250d%250a.
                    Descape again here to recover the raw bytes.
                 */
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    descape(argv[i]);
                    outputHeader = argv[i];
                }
                break;

            case 'l':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    outputLocation = argv[i];
                    if (responseStatus == 0) {
                        responseStatus = 302;
                    }
                }
                break;

            case 'L':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    outputContentLength = argv[i];
                }
                break;

            case 'n':
                nonParsedHeader++;
                break;

            case 'p':
                outputPost++;
                break;

            case 'q':
                outputQuery++;
                break;

            case 'r':
                /*
                    Emit the argument as the complete response, verbatim, generating no headers or body
                    of our own. This drives the CGI response parser with output cgitest cannot otherwise
                    produce: no header block at all, or a header block whose lines are ordered
                    differently. Double-encoded like -H -- getArgv descapes once and this descapes
                    again to recover the raw bytes.
                 */
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    descape(argv[i]);
                    outputRaw = argv[i];
                }
                break;

            case 's':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    responseStatus = atoi(argv[i]);
                }
                break;

            case 'S':
                outputSignals++;
                break;

            case 't':
                if (++i >= argc) {
                    err = __LINE__;
                } else {
                    timeout = atoi(argv[i]);
                }
                break;

            case 'w':
                outputCwd++;
                break;

            case 'x':
                outputNothing++;
                break;

            default:
                /*
                    Unrecognized switches are fatal only when supplied explicitly via HTTP_SWITCHES.
                    Arriving as ISINDEX query args they are request data under test, not cgitest
                    configuration, so tolerate them and let the default output echo the args verbatim
                    to preserve the argv round-trip.
                 */
                if (switchesFromHttp) {
                    err = __LINE__;
                }
                break;
            }
        }
    }
    if (err) {
        fprintf(stderr, "usage: cgitest -aenpx [-b bytes] [-c type] [-E text] [-h lines] [-H header]\n"
                "\t[-B bytes] [-l location] [-L length] [-r response] [-s status] [-t timeout]\n"
                "\tor set the HTTP_SWITCHES environment variable\n");
        fprintf(stderr, "Error at cgitest:%d\n", __LINE__);
        exit(255);
    }
    if (outputStderr) {
        fprintf(stderr, "%s\n", outputStderr);
        fflush(stderr);
    }
    /*
        Stay silent for the requested period before emitting anything. This models a wedged CGI and
        lets a test outlive the server's request timeout while its child is still registered.
     */
    if (timeout > 0) {
        sleepSeconds(timeout);
    }
    if (outputNothing) {
        finish();
        return 0;
    }
    if (outputRaw) {
        fputs(outputRaw, stdout);
        finish();
        return 0;
    }
    if ((method = getenv("REQUEST_METHOD")) != 0 && strcmp(method, "POST") == 0) {
        if (getPostData(&postBuf, &postBufLen) < 0) {
            error("Can't read CGI input");
        }
        if (strcmp(safeGetenv("CONTENT_TYPE"), "application/x-www-form-urlencoded") == 0) {
            numPostKeys = getVars(&postKeys, postBuf, postBufLen);
        }
    }

    if (hasError) {
        if (!nonParsedHeader) {
            printf("HTTP/1.0 %d %s\r\n\r\n", responseStatus, responseMsg);
            printf("<HTML><BODY><p>Error: %d -- ", responseStatus);
            printHtml(responseMsg);
            printf("</p></BODY></HTML>\r\n");
        }
        fprintf(stderr, "cgitest: ERROR: %s\n", responseMsg);
        exit(2);
    }

    if (nonParsedHeader) {
        if (responseStatus == 0) {
            printf("HTTP/1.0 200 OK\r\n");
        } else {
            printf("HTTP/1.0 %d %s\r\n", responseStatus, responseMsg ? responseMsg: "");
        }
        printf("Connection: close\r\n");
        printf("X-CGI-CustomHeader: Any value at all\r\n");
    }
    printf("Content-type: %s\r\n", outputContentType ? outputContentType : "text/html");
    if (outputContentLength) {
        printf("Content-length: %s\r\n", outputContentLength);
    }
    if (outputHeader) {
        printf("%s\r\n", outputHeader);
    }

    if (outputHeaderLines) {
        for (i = 0; i < outputHeaderLines; i++) {
            printf("X-CGI-%d: A loooooooooooooooooooooooong string\r\n", i);
        }
    }
    if (outputLocation) {
        printf("Location: %s\r\n", outputLocation);
    }
    if (responseStatus) {
        printf("Status: %d\r\n", responseStatus);
    }
    printf("\r\n");

    if ((outputLines + outputArgs + outputEnv + outputQuery + outputPost + responseStatus +
         outputBodyBytes + outputSignals + outputCwd) == 0 && outputLocation == NULL) {
        outputArgs++;
        outputEnv++;
        outputQuery++;
        outputPost++;
    }

    if (outputBodyBytes > 0) {
        writeBody(outputBodyBytes);
    } else if (outputLines) {
        for (j = 0; j < outputLines; j++) {
            printf("%010d\n", j);
        }
    } else {
        printf("<HTML><TITLE>cgitest: Output</TITLE><BODY>\r\n");
        if (outputArgs) {
#if _WIN32
            printf("<P>CommandLine: ");
            printHtml(GetCommandLine());
            printf("</P>\r\n");
#endif
            printf("<H2>Args</H2>\r\n");
            for (i = 0; i < argc; i++) {
                printf("<P>ARG[%d]=", i);
                printHtml(argv[i]);
                printf("</P>\r\n");
            }
        }
        printEnv(envp);
        if (outputSignals) {
            printStartState();
        }
        if (outputCwd) {
            printCwd();
        }
        if (outputQuery) {
            printQuery();
        }
        if (outputPost) {
            printPost(postBuf, postBufLen);
        }
        printf("</BODY></HTML>\r\n");
    }

    finish();
    return 0;
}


/*
    Emit exactly "bytes" body bytes, where byte N of the body is '0' + (N % 10). The chunk length is
    a multiple of ten so the pattern runs unbroken across chunks, which lets a test verify not just
    how many bytes arrived but the offset at which a truncated body was cut. Written with write()
    rather than printf so the byte count is exact and does not depend on stdio buffering.
 */
static void writeBody(int bytes)
{
    char chunk[1000];
    int  i, n;

    for (i = 0; i < (int) sizeof(chunk); i++) {
        chunk[i] = (char) ('0' + (i % 10));
    }
    fflush(stdout);
    while (bytes > 0) {
        n = (bytes < (int) sizeof(chunk)) ? bytes : (int) sizeof(chunk);
        if ((int) write(1, chunk, n) != n) {
            break;
        }
        bytes -= n;
    }
}


/*
    Terminate the CGI output. VxWorks pipes need an explicit eof string, and must not call exit(0) as
    that would exit the task before the CGI handler can cleanup. The caller must return instead.
 */
static void finish(void)
{
#if VXWORKS
    write(1, CMD_VXWORKS_EOF, CMD_VXWORKS_EOF_LEN);
    write(2, CMD_VXWORKS_EOF, CMD_VXWORKS_EOF_LEN);
#endif
    free(queryKeys);
    queryKeys = NULL;
    free(postKeys);
    postKeys = NULL;
    free(postBuf);
    postBuf = NULL;
    free(responseMsg);
    responseMsg = NULL;
    fflush(stderr);
    fflush(stdout);
}


/*
    If there is a HTTP_SWITCHES argument in the query string, examine that instead of the original argv
 */
static int getArgv(int *pargc, char ***pargv, int argcIn, char **argvIn)
{
    static char sbuf[1024];
    char        *switches, *next;
    int         i;

    *pargc = 0;
    if (getQueryString(&queryBuf, &queryLen) < 0) {
        return -1;
    }
    numQueryKeys = getVars(&queryKeys, queryBuf, queryLen);

    switches = 0;
    for (i = 0; i < numQueryKeys * 2; i += 2) {
        if (strcmp(queryKeys[i], "HTTP_SWITCHES") == 0) {
            switches = queryKeys[i + 1];
            break;
        }
    }

    if (switches == 0) {
        switches = getenv("HTTP_SWITCHES");
    }
    switchesFromHttp = (switches != 0);
    if (switches) {
        strncpy(sbuf, switches, sizeof(sbuf) - 1);
        descape(sbuf);
        next = strtok(sbuf, " \t\n");
        for (i = 1; next && i < (MAX_ARGV - 1); i++) {
            argvList[i] = next;
            next = strtok(0, " \t\n");
        }
        argvList[0] = argvIn[0];
        *pargv = argvList;
        *pargc = i;

    } else {
        *pargc = argcIn;
        *pargv = argvIn;
    }
    return 0;
}


static void printEnv(char **envp)
{
    static const char *vars[] = {
        "AUTH_TYPE", "CONTENT_LENGTH", "CONTENT_TYPE", "DOCUMENT_ROOT", "GATEWAY_INTERFACE", "HTTP_ACCEPT",
        "HTTP_CONNECTION", "HTTP_HOST", "HTTP_USER_AGENT", "PATH_INFO", "PATH_TRANSLATED", "QUERY_STRING",
        "REMOTE_ADDR", "REQUEST_METHOD", "REQUEST_URI", "REMOTE_USER", "SCRIPT_NAME", "SCRIPT_FILENAME",
        "SERVER_ADDR", "SERVER_NAME", "SERVER_PORT", "SERVER_PROTOCOL", "SERVER_SOFTWARE", NULL
    };
    int               i;

    printf("<H2>Environment Variables</H2>\r\n");
    for (i = 0; vars[i]; i++) {
        printVar("<P>", vars[i], safeGetenv(vars[i]), "</P>\r\n");
    }

#if !VXWORKS
    /*
        This is not supported on VxWorks as you can't get "envp" in main()
     */
    printf("\r\n<H2>All Defined Environment Variables</H2>\r\n");
    if (envp) {
        for (i = 0; envp[i]; i++) {
            printf("<P>");
            printHtml(envp[i]);
            printf("</P>\r\n");
        }
    }
#endif
    printf("\r\n");
}


/*
    Escape < and > in HTML element content; preserve & so values read back unchanged.
 */
static void writeHtml(const char *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (buf[i] == '<') {
            fputs("&lt;", stdout);
        } else if (buf[i] == '>') {
            fputs("&gt;", stdout);
        } else {
            putchar(buf[i]);
        }
    }
}


static void printHtml(const char *s)
{
    if (s) {
        writeHtml(s, strlen(s));
    }
}


static void printVar(const char *prefix, const char *name, const char *value, const char *suffix)
{
    fputs(prefix, stdout);
    printHtml(name);
    putchar('=');
    printHtml(value);
    fputs(suffix, stdout);
}


static void printQuery()
{
    int i;

    if (numQueryKeys == 0) {
        printf("<H2>No Query String Found</H2>\r\n");
    } else {
        printf("<H2>Decoded Query String Variables</H2>\r\n");
        for (i = 0; i < (numQueryKeys * 2); i += 2) {
            printVar("<p>QVAR ", queryKeys[i], queryKeys[i + 1], "</p>\r\n");
        }
    }
    printf("\r\n");
}


static void printPost(char *buf, size_t len)
{
    int i;

    if (numPostKeys) {
        printf("<H2>Decoded Post Variables</H2>\r\n");
        for (i = 0; i < (numPostKeys * 2); i += 2) {
            printVar("<p>PVAR ", postKeys[i], postKeys[i + 1], "</p>\r\n");
        }

    } else if (buf) {
        if (len < (50 * 1000)) {
            printf("<H2>Post Data %d bytes found (data below)</H2>\r\n", (int) len);
            writeHtml(buf, len);
        } else {
            printf("<H2>Post Data %d bytes found</H2>\r\n", (int) len);
        }

    } else {
        printf("<H2>No Post Data Found</H2>\r\n");
    }
    printf("\r\n");
}


static void printCwd(void)
{
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "unknown");
    }
    printf("<p>CWD=");
    printHtml(cwd);
    printf("</p>\r\n");
}


static void printStartState(void)
{
#if _WIN32 && !WINCE
    WSADATA   wsa;
    ULONG_PTR h;
    int       inherited, type, len;

    printf("<p>SIGPIPE=unsupported</p>\r\n");
    /*
        This program creates no sockets, so any socket handle it holds was inherited. Handle values are
        multiples of four.
     */
    inherited = 0;
    if (WSAStartup(MAKEWORD(1, 1), &wsa) == 0) {
        for (h = 4; h < 65536; h += 4) {
            len = sizeof(type);
            if (getsockopt((SOCKET) h, SOL_SOCKET, SO_TYPE, (char*) &type, &len) == 0) {
                inherited++;
            }
        }
        WSACleanup();
    }
    printf("<p>INHERITED=%d</p>\r\n", inherited);
#elif WINCE || VXWORKS
    printf("<p>SIGPIPE=unsupported</p>\r\n");
#else
    struct sigaction act;
    sigset_t         mask;
    long             max;
    int              blocked, fd, inherited, sig;

    printf("<H2>Signals</H2>\r\n");
    if (sigaction(SIGPIPE, NULL, &act) == 0) {
        printf("<p>SIGPIPE=%s</p>\r\n", act.sa_handler == SIG_IGN ? "ignored" :
               act.sa_handler == SIG_DFL ? "default" : "caught");
    }
    blocked = 0;
    if (sigprocmask(SIG_BLOCK, NULL, &mask) == 0) {
        for (sig = 1; sig < NSIG; sig++) {
            if (sigismember(&mask, sig) == 1) {
                blocked++;
            }
        }
    }
    printf("<p>BLOCKED=%d</p>\r\n", blocked);

    inherited = 0;
    max = sysconf(_SC_OPEN_MAX);
    for (fd = 3; fd < max && fd < 65536; fd++) {
        if (fcntl(fd, F_GETFD) >= 0) {
            inherited++;
        }
    }
    printf("<p>INHERITED=%d</p>\r\n", inherited);
    printf("<p>UID=%d</p>\r\n<p>GID=%d</p>\r\n", (int) geteuid(), (int) getegid());
#endif
}


static int getQueryString(char **buf, size_t *buflen)
{
    *buf = getenv("QUERY_STRING");
    if (*buf == NULL) {
        *buf = "";
    }
    *buflen = strlen(*buf);
    return 0;
}


static int getPostData(char **bufp, size_t *lenp)
{
    char    *contentLength, *buf;
    ssize_t bufsize, bytes, size, limit, len;

    if ((contentLength = getenv("CONTENT_LENGTH")) != 0) {
        size = atoi(contentLength);
        limit = size;
    } else {
        size = 4096;
        limit = INT_MAX;
    }
    if ((buf = malloc(size + 1)) == 0) {
        error("Cannot allocate memory to read post data");
        return -1;
    }
    bufsize = size + 1;
    len = 0;

    while (len < limit) {
        if ((len + size + 1) > bufsize) {
            if ((buf = realloc(buf, len + size + 1)) == 0) {
                error("Cannot allocate memory to read post data");
                return -1;
            }
            bufsize = len + size + 1;
        }
        bytes = read(0, &buf[len], (int) size);
        if (bytes < 0) {
            error("Cannot read CGI input error=%d, len %d, %d", (int) errno, (int) len, (int) limit);
            return -1;
        } else if (bytes == 0) {
            /* EOF */
#if UNUSED
            /*
                If using multipart-mime, the CONTENT_LENGTH won't match the length of the data actually received
             */
            if (contentLength && len != limit) {
                error("Missing content data (Content-Length: %s)", contentLength ? contentLength : "unspecified");
            }
#endif
            break;
        }
        len += bytes;
    }
    buf[len] = 0;
    *lenp = len;
    *bufp = buf;
    return 0;
}


static int getVars(char ***cgiKeys, char *buf, size_t buflen)
{
    char **keyList, *eq, *cp, *pp, *newbuf;
    int  i, keyCount;

    *cgiKeys = NULL;
    if (buf == NULL || buflen == 0) {
        return 0;
    }
    keyCount = 1;
    for (i = 0; (size_t) i < buflen; i++) {
        if (buf[i] == '&') {
            keyCount++;
        }
    }
    /* Keep the decoded strings and their pointer table in one owned allocation. */
    keyList = malloc((size_t) keyCount * 2 * sizeof(*keyList) + buflen + 1);
    if (keyList == NULL) {
        error("Can't allocate memory");
        return 0;
    }
    newbuf = (char*) (keyList + keyCount * 2);
    memcpy(newbuf, buf, buflen);
    newbuf[buflen] = '\0';
    for (cp = newbuf; *cp; cp++) {
        if (*cp == '+') {
            *cp = ' ';
        }
    }
    i = 0;
    for (pp = strtok(newbuf, "&"); pp; pp = strtok(0, "&")) {
        if ((eq = strchr(pp, '=')) != NULL) {
            *eq++ = '\0';
            descape(eq);
        }
        descape(pp);
        keyList[i++] = pp;
        keyList[i++] = eq;
    }
    *cgiKeys = keyList;
    return i / 2;
}


static char hex2Char(char *s)
{
    int c;

    if (*s >= 'A') {
        c = toupper(*s & 0xFF) - 'A' + 10;
    } else {
        c = *s - '0';
    }
    s++;

    if (*s >= 'A') {
        c = (c * 16) + (toupper(*s & 0xFF) - 'A') + 10;
    } else {
        c = (c * 16) + (toupper(*s & 0xFF) - '0');
    }
    return (char) c;
}


static void descape(char *src)
{
    char *dest;

    dest = src;
    while (*src) {
        // Decode only complete hex escapes.
        if (*src == '%' && isxdigit((unsigned char) src[1]) && isxdigit((unsigned char) src[2])) {
            *dest++ = hex2Char(++src);
            src += 2;
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}


static char *safeGetenv(const char *key)
{
    char *cp;

    cp = getenv(key);
    if (cp == 0) {
        return "";
    }
    return cp;
}


void error(char *fmt, ...)
{
    va_list args;
    char    buf[4096];

    if (responseMsg == 0) {
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        responseStatus = 400;
        responseMsg = strdup(buf);
        va_end(args);
    }
    hasError++;
}


#if VXWORKS
/*
    VxWorks link resolution
 */
int _cleanup()
{
    return 0;
}
int _exit()
{
    return 0;
}
#endif /* VXWORKS */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
