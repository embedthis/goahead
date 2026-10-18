/*
    goahead.c -- Main program for GoAhead

    Usage: goahead [options] [documents] [IP][:port] ...
        Options:
        --auth authFile        # User and role configuration
        --background           # Run as a Unix daemon (not MacOS). Keeps the working directory. Log with --log file
        --cert certFile        # Server SSL certificate (PEM)
        --debugger             # Run in debug mode
        --group groupName      # Group to run as after listening
        --home directory       # Change to directory to run
        --key keyFile          # Server SSL private key (PEM)
        --log logFile:level    # Log to file file at verbosity level
        --no-strict            # Only warn about an unsafe route table
        --proxy addresses      # Trust X-Forwarded-Proto from these proxy addresses
        --route routeFile      # Route configuration file
        --strict               # Refuse to start with an unsafe route table (default)
        --user userName        # User to run as after listening
        --verbose              # Same as --log stdout:2
        --version              # Output version information

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************* Defines ************************************/

static volatile int finished = 0;

/********************************* Forwards ***********************************/

static void initPlatform(void);
static void logHeader(void);
static void usage(void);

#if WINDOWS
static void windowsClose();
static int windowsInit();
static LRESULT CALLBACK websWindProc(HWND hwnd, UINT msg, UINT wp, LPARAM lp);
#endif

#if ME_UNIX_LIKE
static void sigHandler(int signo);
#endif

/*********************************** Code *************************************/

MAIN(goahead, int argc, char **argv, char **envp)
{
    char *argp, *home, *documents, *endpoints, *endpoint, *route, *tok, *lspec, *user, *group;
    int  argind;

#if ME_GOAHEAD_AUTH
    char *auth;
#endif

    websHardenProcess();
    initPlatform();
#if WINDOWS
    if (windowsInit() < 0) {
        return 0;
    }
#endif
    route = "route.txt";
    user = ME_GOAHEAD_USER;
    group = ME_GOAHEAD_GROUP;
#if ME_GOAHEAD_AUTH
    auth = "auth.txt";
#endif

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;

#if ME_GOAHEAD_AUTH
            /*
                Only offered when the build can act on it. A build with ME_GOAHEAD_AUTH=0 reports the
                option as unknown rather than accepting it and ignoring the file.
             */
        } else if (smatch(argp, "--auth") || smatch(argp, "-a")) {
            if (argind + 1 >= argc) usage();
            auth = argv[++argind];
#endif

#if ME_UNIX_LIKE && !MACOSX
        } else if (smatch(argp, "--background") || smatch(argp, "-b")) {
            websSetBackground(1);
#endif

#if ME_UNIX_LIKE
        } else if (smatch(argp, "--group")) {
            if (argind + 1 >= argc) usage();
            group = argv[++argind];

        } else if (smatch(argp, "--user")) {
            if (argind + 1 >= argc) usage();
            user = argv[++argind];
#endif

#if ME_COM_SSL
        } else if (smatch(argp, "--cert")) {
            if (argind + 1 >= argc) usage();
            websSetSslCertFile(argv[++argind]);

        } else if (smatch(argp, "--key")) {
            if (argind + 1 >= argc) usage();
            websSetSslKeyFile(argv[++argind]);
#endif

        } else if (smatch(argp, "--debugger") || smatch(argp, "-d") || smatch(argp, "-D")) {
            websSetDebug(1);

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

        } else if (smatch(argp, "--proxy")) {
            if (argind + 1 >= argc) usage();
            websSetTrustedProxies(argv[++argind]);

        } else if (smatch(argp, "--strict")) {
            websSetStrict(1);

        } else if (smatch(argp, "--no-strict")) {
            websSetStrict(0);

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
    if (websOpen(documents, route) < 0) {
        error("Cannot initialize server. Exiting.");
        return -1;
    }
#if ME_GOAHEAD_AUTH
    if (auth && websLoad(auth) < 0) {
        error("Cannot load %s", auth);
        return -1;
    }
#endif
    logHeader();
    if (argind < argc) {
        while (argind < argc) {
            endpoint = argv[argind++];
            if (websListen(endpoint) < 0) {
                return -1;
            }
        }
    } else {
        endpoints = sclone(ME_GOAHEAD_LISTEN);
        for (endpoint = stok(endpoints, ", \t", &tok); endpoint; endpoint = stok(NULL, ", \t,", &tok)) {
#if !ME_COM_SSL
            if (strstr(endpoint, "https")) continue;
#endif
            if (websListen(endpoint) < 0) {
                wfree(endpoints);
                return -1;
            }
        }
        wfree(endpoints);
    }
#ifdef GOAHEAD_INIT
    /*
        Define your init function by configuring the build with DFLAGS=GOAHEAD_INIT=myInitFunction
     */
    {
        extern int GOAHEAD_INIT();

        if (GOAHEAD_INIT() < 0) {
            exit(1);
        }
    }
#endif
    if (websDropPrivileges(user, group) < 0) {
        error("Cannot change to the configured user and group. Exiting.");
        return -1;
    }
#if ME_UNIX_LIKE
    if (geteuid() == 0) {
        error("Warning: running as root. Use --user and --group to run with less privilege after listening.");
    }
#endif
#if ME_UNIX_LIKE && !MACOSX
    /*
        Service events till terminated
     */
    if (websGetBackground()) {
        if (smatch(logGetPath(), "stdout") || smatch(logGetPath(), "stderr")) {
            error("--background discards log output to stdout or stderr; use --log file");
        }
        //  Keep the working directory: the documents and other relative paths resolve against it
        if (daemon(1, 0) < 0) {
            error("Cannot run as daemon");
            return -1;
        }
    }
#endif
    websServiceEvents(&finished);
    logmsg(1, "Instructed to exit");
    websClose();
#if WINDOWS
    windowsClose();
#endif
    return 0;
}


static void logHeader(void)
{
    char home[ME_GOAHEAD_LIMIT_STRING];

    if (getcwd(home, sizeof(home)) == 0) {
        scopy(home, sizeof(home), "unknown");
    }
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
            "  %s [options] [documents] [[IPaddress][:port] ...]\n\n"
            "  Options:\n"
#if ME_GOAHEAD_AUTH
            "    --auth authFile        # User and role configuration\n"
#endif
#if ME_UNIX_LIKE && !MACOSX
            "    --background           # Run as a Unix daemon\n"
#endif
#if ME_COM_SSL
            "    --cert certFile        # Server SSL certificate (PEM)\n"
#endif
            "    --debugger             # Run in debug mode\n"
#if ME_UNIX_LIKE
            "    --group groupName      # Group to run as after listening\n"
#endif
            "    --home directory       # Change to directory to run\n"
#if ME_COM_SSL
            "    --key keyFile          # Server SSL private key (PEM)\n"
#endif
            "    --log logFile:level    # Log to file file at verbosity level\n"
            "    --no-strict            # Only warn about an unsafe route table\n"
            "    --proxy addresses      # Trust X-Forwarded-Proto from these proxy addresses\n"
            "    --route routeFile      # Route configuration file\n"
            "    --strict               # Refuse to start with an unsafe route table (default)\n"
#if ME_UNIX_LIKE
            "    --user userName        # User to run as after listening\n"
#endif
            "    --verbose              # Same as --log stdout:2\n"
            "    --version              # Output version information\n\n",
            ME_TITLE, ME_NAME);
    exit(-1);
}


static void initPlatform(void)
{
#if ME_UNIX_LIKE
    struct sigaction act;

    umask(027);
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigHandler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
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


#if WINDOWS
/*
    Create a taskbar entry. Register the window class and create a window
 */
static int windowsInit()
{
    HINSTANCE inst;
    WNDCLASS  wc;                       /* Window class */
    HMENU     hSysMenu;
    HWND      hwnd;

    inst = websGetInst();
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = inst;
    wc.hIcon = NULL;
    wc.lpfnWndProc = (WNDPROC) websWindProc;
    wc.lpszMenuName = wc.lpszClassName = ME_NAME;
    if (!RegisterClass(&wc)) {
        return -1;
    }
    /*
        Create a window just so we can have a taskbar to close this web server
     */
    hwnd = CreateWindow(ME_NAME, ME_TITLE, WS_MINIMIZE | WS_POPUPWINDOW, CW_USEDEFAULT,
                        0, 0, 0, NULL, NULL, inst, NULL);
    if (hwnd == NULL) {
        return -1;
    }

    /*
        Add the about box menu item to the system menu
     */
    hSysMenu = GetSystemMenu(hwnd, FALSE);
    if (hSysMenu != NULL) {
        AppendMenu(hSysMenu, MF_SEPARATOR, 0, NULL);
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    return 0;
}


static void windowsClose()
{
    HINSTANCE inst;

    inst = websGetInst();
    UnregisterClass(ME_NAME, inst);
}


/*
    Main menu window message handler.
 */
static LRESULT CALLBACK websWindProc(HWND hwnd, UINT msg, UINT wp, LPARAM lp)
{
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        finished++;
        return 0;

    case WM_SYSCOMMAND:
        break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}


/*
    Check for Windows Messages
 */
WPARAM checkWindowsMsgLoop()
{
    MSG msg;

    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        if (!GetMessage(&msg, NULL, 0, 0) || msg.message == WM_QUIT) {
            return msg.wParam;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}


/*
    Windows message handler
 */
static LRESULT CALLBACK websAboutProc(HWND hwndDlg, uint msg, uint wp, long lp)
{
    LRESULT lResult;

    lResult = DefWindowProc(hwndDlg, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:
        break;
    case WM_DESTROY:
        break;
    case WM_COMMAND:
        break;
    }
    return lResult;
}

#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
