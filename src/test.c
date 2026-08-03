/*
    test.c -- Unit test program for GoAhead

    Usage: goahead-test [options] [documents] [endpoints...]
        Options:
        --auth authFile        # User and role configuration
        --background           # Run as a Unix daemon (not MacOS)
        --cert certFile        # Server SSL certificate (PEM)
        --connections count    # Concurrent connection cap (test harness)
        --connections-per-ip n # Per-peer connection cap (test harness)
        --debugger             # Disable timeouts to enable debugging
        --duration secs        # Exit after the given number of seconds
        --home directory       # Change to directory to run
        --key keyFile          # Server SSL private key (PEM)
        --log logFile:level    # Log to file file at verbosity level
        --route routeFile      # Route configuration file
        --verbose              # Same as --log stdout:2
        --version              # Output version information

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"
#include    "js.h"

/********************************* Defines ************************************/

static int finished = 0;

#undef ME_GOAHEAD_LISTEN
/*
    These must match TM_HTTP, TM_HTTPS, TM_HTTPV6 and TM_HTTPSV6 in test/testme.json5

    IPv6 endpoints are on by default so test/ipv6 has a server to talk to. Binding them is not
    required to succeed: a host with no IPv6 loopback still runs the rest of the suite, and
    test/ipv6 is skipped there by clearing ME_IPV6 in test/testme.json5. Build with -DTEST_IPV6=0
    to leave the IPv6 listeners out of the binary entirely.
 */
#ifndef TEST_IPV6
    #define TEST_IPV6 1
#endif

#if TEST_IPV6
#if ME_COM_SSL
    #define ME_GOAHEAD_LISTEN "http://127.0.0.1:18080, https://127.0.0.1:14443, http://[::1]:18090, https://[::1]:14453"
#else
    #define ME_GOAHEAD_LISTEN "http://127.0.0.1:18080, http://[::1]:18090"
#endif
#else
#if ME_COM_SSL
    #define ME_GOAHEAD_LISTEN "http://127.0.0.1:18080, https://127.0.0.1:14443"
#else
    #define ME_GOAHEAD_LISTEN "http://127.0.0.1:18080"
#endif
#endif


/********************************* Forwards ***********************************/

static void initPlatform(void);
static void logHeader(void);
static void usage(void);

static bool testHandler(Webs *wp);
#if ME_GOAHEAD_CGI
static bool blacklistHandler(Webs *wp);
#endif
#if ME_GOAHEAD_JAVASCRIPT
static int aspTest(int eid, Webs *wp, int argc, char **argv);
static int bigTest(int eid, Webs *wp, int argc, char **argv);
#endif
static void actionTest(Webs *wp);
static void sessionTest(Webs *wp);
static void showTest(Webs *wp);
static void connectionsTest(Webs *wp);
#if ME_GOAHEAD_UPLOAD && !ME_ROM
static void uploadTest(Webs *wp);
#endif
#if ME_GOAHEAD_LEGACY
static int legacyTest(Webs *wp, char *prefix, char *dir, int flags);
#endif
#if ME_UNIX_LIKE
static void sigHandler(int signo);
#endif
static void exitProc(void *data, int id);

/*********************************** Code *************************************/

MAIN(goahead, int argc, char **argv, char **envp)
{
    char *argp, *auth, *home, *documents, *endpoints, *endpoint, *route, *tok, *lspec;
    int  argind, duration;

    route = "route.txt";
    auth = "auth.txt";
    duration = 0;

    /*
        Relax the POST body cap to 16 MB for the test harness. The production default
        (ME_GOAHEAD_LIMIT_POST = 16 KB) is tight for upload stress tests.
     */
    websSetPostLimit(16 * 1024 * 1024);

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;

        } else if (smatch(argp, "--auth") || smatch(argp, "-a")) {
            if (argind + 1 >= argc) usage();
            auth = argv[++argind];

#if ME_UNIX_LIKE && !MACOSX
        } else if (smatch(argp, "--background") || smatch(argp, "-b")) {
            websSetBackground(1);
#endif

#if ME_COM_SSL
        } else if (smatch(argp, "--cert")) {
            if (argind + 1 >= argc) usage();
            websSetSslCertFile(argv[++argind]);

        } else if (smatch(argp, "--key")) {
            if (argind + 1 >= argc) usage();
            websSetSslKeyFile(argv[++argind]);
#endif

        } else if (smatch(argp, "--connections")) {
            /*
                Lower the concurrent-connection cap so a test can reach it without opening a thousand
                sockets. The shipped default is derived from FD_SETSIZE and is impractical to drive.
             */
            if (argind + 1 >= argc) usage();
            websSetConnectionLimit(atoi(argv[++argind]));

        } else if (smatch(argp, "--connections-per-ip")) {
            if (argind + 1 >= argc) usage();
            websSetConnectionLimitPerIp(atoi(argv[++argind]));

        } else if (smatch(argp, "--debugger") || smatch(argp, "-d") || smatch(argp, "-D")) {
            websSetDebug(1);

        } else if (smatch(argp, "--duration")) {
            if (argind + 1 >= argc) usage();
            duration = atoi(argv[++argind]);

        } else if (smatch(argp, "--home")) {
            if (argind + 1 >= argc) usage();
            home = argv[++argind];
            if (chdir(home) < 0) {
                error("Cannot change directory to %s", home);
                exit(-1);
            }
        } else if (smatch(argp, "--log") || smatch(argp, "-l")) {
            if (argind + 1 >= argc) usage();
            logSetPath(argv[++argind]);

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            logSetPath("stdout:2");

        } else if (smatch(argp, "--route") || smatch(argp, "-r")) {
            if (argind + 1 >= argc) usage();
            route = argv[++argind];

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            printf("%s\n", ME_VERSION);
            exit(0);

        } else if (*argp == '-' && isdigit((uchar) argp[1])) {
            lspec = sfmt("stdout:%s", &argp[1]);
            logSetPath(lspec);
            wfree(lspec);

        } else {
            usage();
        }
    }
    documents = ME_GOAHEAD_DOCUMENTS;
    if (argc > argind) {
        documents = argv[argind++];
    }
    initPlatform();
    if (websOpen(documents, route) < 0) {
        error("Cannot initialize server. Exiting.");
        return -1;
    }
    logHeader();
    if (websLoad(auth) < 0) {
        error("Cannot load %s", auth);
        return -1;
    }
    if (argind < argc) {
        while (argind < argc) {
            endpoint = argv[argind++];
            if (websListen(endpoint) < 0) {
                return -1;
            }
        }
    } else {
        /*
            An IPv6 endpoint that will not bind is not fatal. Hosts without IPv6 loopback are
            common in containers, and failing here would take down the whole suite rather than the
            handful of tests that need IPv6. An IPv4 endpoint that will not bind is still fatal:
            every other test depends on it.
         */
        endpoints = sclone(ME_GOAHEAD_LISTEN);
        for (endpoint = stok(endpoints, ", \t", &tok); endpoint; endpoint = stok(NULL, ", \t,", &tok)) {
            if (websListen(endpoint) < 0) {
                if (strchr(endpoint, '[') != NULL) {
                    error("Warning: cannot listen on %s. Skipping - IPv6 tests will fail", endpoint);
                    continue;
                }
                return -1;
            }
        }
        wfree(endpoints);
    }

    websDefineHandler("test", testHandler, 0, 0, 0);
    websAddRoute("/test", "test", 0);
#if ME_GOAHEAD_CGI
    /*
        Route /blacklist-test through a handler that injects PATH/IFS/etc into
        wp->vars and forwards to /cgi-bin/cgitest. Used by test/basic/cgi.tst.ts to
        verify that the CGI env-build path filters blacklisted variables.
     */
    websDefineHandler("blacklist", blacklistHandler, 0, 0, 0);
    websAddRoute("/blacklist-test", "blacklist", 0);
#endif
#if ME_GOAHEAD_LEGACY
    websUrlHandlerDefine("/legacy/", 0, 0, legacyTest, 0);
#endif
#if ME_GOAHEAD_JAVASCRIPT
    websDefineJst("aspTest", aspTest);
    websDefineJst("bigTest", bigTest);
#endif
    websDefineAction("test", actionTest);
    websDefineAction("sessionTest", sessionTest);
    websDefineAction("showTest", showTest);
    websDefineAction("connections", connectionsTest);
#if ME_GOAHEAD_UPLOAD && !ME_ROM
    websDefineAction("uploadTest", uploadTest);
#endif

#if ME_UNIX_LIKE && !MACOSX
    /*
        Service events till terminated
     */
    if (websGetBackground()) {
        if (daemon(0, 0) < 0) {
            error("Cannot run as daemon");
            return -1;
        }
    }
#endif
    if (duration) {
        printf("Running for %d secs\n", duration);
        websStartEvent(duration * 1000, (WebsEventProc) exitProc, 0);
    }
    websServiceEvents(&finished);
    logmsg(1, "Instructed to exit\n");
    websClose();
    return 0;
}


static void exitProc(void *data, int id)
{
    websStopEvent(id);
    finished = 1;
}


static void logHeader(void)
{
    char home[ME_GOAHEAD_LIMIT_STRING];

    getcwd(home, sizeof(home));
    logmsg(2, "Configuration for %s", ME_TITLE);
    logmsg(2, "---------------------------------------------");
    logmsg(2, "Version:            %s", ME_VERSION);
    logmsg(2, "BuildType:          %s", ME_DEBUG ? "Debug" : "Release");
    logmsg(2, "CPU:                %s", ME_CPU);
    logmsg(2, "OS:                 %s", ME_OS);
    logmsg(2, "Host:               %s", websGetServer());
    logmsg(2, "Directory:          %s", home);
    logmsg(2, "Documents:          %s", websGetDocuments());
    logmsg(2, "---------------------------------------------");
}


static void usage(void)
{
    fprintf(stderr, "\n%s Usage:\n\n"
            "  %s [options] [documents] [IPaddress][:port]...\n\n"
            "  Options:\n"
            "    --auth authFile        # User and role configuration\n"
#if ME_UNIX_LIKE && !MACOSX
            "    --background           # Run as a Unix daemon\n"
#endif
#if ME_COM_SSL
            "    --cert certFile        # Server SSL certificate (PEM)\n"
#endif
            "    --connections count    # Concurrent connection cap (test harness)\n"
            "    --connections-per-ip n # Per-peer connection cap (test harness)\n"
            "    --debugger             # Run in debug mode\n"
            "    --home directory       # Change to directory to run\n"
#if ME_COM_SSL
            "    --key keyFile          # Server SSL private key (PEM)\n"
#endif
            "    --log logFile:level    # Log to file file at verbosity level\n"
            "    --route routeFile      # Route configuration file\n"
            "    --verbose              # Same as --log stderr:2\n"
            "    --version              # Output version information\n\n",
            ME_TITLE, ME_NAME);
    exit(-1);
}


void initPlatform(void)
{
#if ME_UNIX_LIKE
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);
    signal(SIGKILL, sigHandler);
    #ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    #endif
#elif ME_WIN_LIKE
    _fmode = _O_BINARY;
#endif
}


#if ME_UNIX_LIKE
static void sigHandler(int signo)
{
    finished = 1;
}
#endif


/*
    Simple handler and route test
    Note: Accesses to "/" are normally remapped automatically to /index.html
 */
static bool testHandler(Webs *wp)
{
    if (smatch(wp->path, "/")) {
        websRewriteRequest(wp, "/home.html");
        /* Fall through */
    }
    return 0;
}


#if ME_GOAHEAD_CGI
/*
    Inject blacklisted and non-blacklisted variables into wp->vars, then forward
    to the CGI program. The CGI env-build path in src/cgi.c filters entries that
    appear in envBlackList[]; the regression test asserts the filtered names do
    not appear in the cgitest env dump while the sentinel does.
 */
static bool blacklistHandler(Webs *wp)
{
    websSetVar(wp, "PATH", "/evil:/bin");
    websSetVar(wp, "IFS", "X");
    websSetVar(wp, "BASH_ENV", "/tmp/evil");
    websSetVar(wp, "LD_PRELOAD", "/tmp/evil.so");
    websSetVar(wp, "BLACKLIST_SENTINEL", "ok");
    websRewriteRequest(wp, "/cgi-bin/cgitest");
    return 0;
}
#endif


#if ME_GOAHEAD_JAVASCRIPT
/*
    Parse the form variables: name, address and echo back
 */
static int aspTest(int eid, Webs *wp, int argc, char **argv)
{
    char *name, *address;

    if (jsArgs(argc, argv, "%s %s", &name, &address) < 2) {
        websError(wp, 400, "Insufficient args\n");
        return -1;
    }
    return (int) websWrite(wp, "Name: %s, Address %s", name, address);
}


/*
    Generate a large response
 */
static int bigTest(int eid, Webs *wp, int argc, char **argv)
{
    int i;

    websSetStatus(wp, 200);
    websWriteHeaders(wp, -1, 0);
    websWriteEndHeaders(wp);
    websWrite(wp, "<html>\n");
    for (i = 0; i < 800; i++) {
        websWrite(wp, " Line: %05d %s", i, "aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>\r\n");
    }
    websWrite(wp, "</html>\n");
    websDone(wp);
    return 0;
}
#endif


/*
    Implement /action/actionTest. Parse the form variables: name, address and echo back.
 */
static void actionTest(Webs *wp)
{
    cchar *name, *address;

    name = websGetVar(wp, "name", NULL);
    address = websGetVar(wp, "address", NULL);
    websSetStatus(wp, 200);
    websWriteHeaders(wp, -1, 0);
    websWriteEndHeaders(wp);
    websWrite(wp, "<html><body><h2>name: %s, address: %s</h2></body></html>\n", name, address);
    websFlush(wp, 0);
    websDone(wp);
}


static void sessionTest(Webs *wp)
{
    cchar *number;

    if (scaselessmatch(wp->method, "POST")) {
        number = websGetVar(wp, "number", 0);
        websSetSessionVar(wp, "number", number);
    } else {
        number = websGetSessionVar(wp, "number", 0);
    }
    websSetStatus(wp, 200);
    websWriteHeaders(wp, -1, 0);
    websWriteEndHeaders(wp);
    websWrite(wp, "<html><body><p>Number %s</p></body></html>\n", number);
    websDone(wp);
}


/*
    Report the live connection count so a test can assert the cap directly instead of inferring it
    from timing. The count is server-side state and a test runs in another process, so there is no
    other way to observe it. The response is the bare number to keep parsing trivial.

    The request being serviced is itself counted, so a caller comparing against a flood it is holding
    must account for this connection.
 */
static void connectionsTest(Webs *wp)
{
    char body[64];

    /*
        Emit a known Content-Length rather than passing -1. A negative length selects chunked framing,
        and a caller scanning for the first digits after the header block then reads the chunk size
        instead of the value. Tag the value too, so the response cannot be misparsed silently.
     */
    fmt(body, sizeof(body), "connections=%d peer=%d\n", websGetConnectionCount(),
        websGetConnectionCountForIp(wp->ipaddr));
    websSetStatus(wp, 200);
    websWriteHeaders(wp, (ssize) slen(body), 0);
    websWriteHeader(wp, "Content-Type", "text/plain");
    websWriteEndHeaders(wp);
    websWriteBlock(wp, body, slen(body));
    websDone(wp);
}


static void showTest(Webs *wp)
{
    WebsKey *s;

    websSetStatus(wp, 200);
    websWriteHeaders(wp, -1, 0);
    websWriteEndHeaders(wp);
    websWrite(wp, "<html><body><pre>\n");
    for (s = hashFirst(wp->vars); s; s = hashNext(wp->vars, s)) {
        websWrite(wp, "%s=%s\n", s->name.value.string, s->content.value.string);
    }
    websWrite(wp, "</pre></body></html>\n");
    websDone(wp);
}


#if ME_GOAHEAD_UPLOAD && !ME_ROM
/*
    Dump the file upload details. Don't actually do anything with the uploaded file.
 */
static void uploadTest(Webs *wp)
{
    WebsKey    *s;
    WebsUpload *up;
    char       *upfile;

    websSetStatus(wp, 200);
    websWriteHeaders(wp, -1, 0);
    websWriteHeader(wp, "Content-Type", "text/plain");
    websWriteEndHeaders(wp);

    if (scaselessmatch(wp->method, "POST")) {
        for (s = hashFirst(wp->files); s; s = hashNext(wp->files, s)) {
            up = s->content.value.symbol;
            websWrite(wp, "FILE: %s\r\n", s->name.value.string);
            websWrite(wp, "FILENAME=%s\r\n", up->filename);
            websWrite(wp, "CLIENT=%s\r\n", up->clientFilename);
            websWrite(wp, "TYPE=%s\r\n", up->contentType);
            websWrite(wp, "SIZE=%d\r\n", (int) up->size);

            upfile = sfmt("%s/tmp/%s", websGetDocuments(), up->clientFilename);
            if (rename(up->filename, upfile) < 0) {
                error("Cannot rename uploaded file: %s to %s, errno %d", up->filename, upfile, errno);
            }
            wfree(upfile);
        }
        websWrite(wp, "\r\nVARS:\r\n");
        for (s = hashFirst(wp->vars); s; s = hashNext(wp->vars, s)) {
            websWrite(wp, "%s=%s\r\n", s->name.value.string, s->content.value.string);
        }
    }
    websDone(wp);
}
#endif


#if ME_GOAHEAD_LEGACY
/*
    Legacy handler with old parameter sequence
 */
static int legacyTest(Webs *wp, char *prefix, char *dir, int flags)
{
    websSetStatus(wp, 200);
    websWriteHeaders(wp, -1, 0);
    websWriteHeader(wp, "Content-Type", "text/plain");
    websWriteEndHeaders(wp);
    websWrite(wp, "Hello Legacy World\n");
    websDone(wp);
    return 1;
}

#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
