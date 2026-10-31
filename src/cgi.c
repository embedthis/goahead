/*
    cgi.c -- CGI processing

    This module implements the /cgi-bin handler. CGI processing differs from
    goforms processing in that each CGI request is executed as a separate
    process, rather than within the webserver process. For each CGI request the
    environment of the new process must be set to include all the CGI variables
    and its standard input and output must be directed to the socket.  This
    is done using temporary files.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/
#if ME_GOAHEAD_CGI

#if ME_WIN_LIKE
typedef HANDLE CgiPid;
#elif VXWORKS && (_WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 9))
/* A task ID holds a pointer on 64-bit VxWorks. (CgiPid) -1 is TASK_ID_ERROR */
typedef TASK_ID CgiPid;
#else
typedef pid_t CgiPid;
#endif

#if WINDOWS
    #define CGI_OUTPUT_GRACE 1000   /* Msec to wait for the output of an exited CGI to appear */
    #define CGI_KILL_WAIT    5000   /* Msec to wait for a terminated CGI to release its staging files */
#endif

typedef struct Cgi {            /* Struct for CGI tasks which have completed */
    Webs *wp;                   /* Connection object */
    char *stdIn;                /* File desc. for task's temp input fd */
    char *stdOut;               /* File desc. for task's temp output fd */
    char *cgiPath;              /* Path to executable process file */
    char *query;                /* Backing store for the argp strings */
    char **argp;                /* Pointer to buf containing argv tokens */
    char **envp;                /* Pointer to array of environment strings */
    CgiPid handle;              /* Process handle of the task */
    off_t fplacemark;           /* Seek location for CGI output file */
    ssize txLen;                /* Content length the CGI declared, or -1 for chunked framing */
    ssize txBytes;              /* Body bytes forwarded to the client against the declared length */
    ssize txExcess;             /* Body bytes dropped beyond the declared content length */
    bool txSuppress;            /* Response carries no body at all, whatever the CGI writes */
#if WINDOWS
    uint64 outputDeadline;      /* Tick after which an exited CGI that wrote nothing has no output coming */
    HANDLE job;                 /* Job holding the CGI and every process it starts, so an abort ends them all */
#endif
#if VXWORKS
    char *vxName;               /* Unique task name, so a reused task ID is not mistaken for this task */
    int fdin;                   /* Task stdin, owned by the server so an aborted task cannot leak it */
    int fdout;                  /* Task stdout, owned by the server */
    struct CgiModule *module;   /* Loaded module the task executes, or NULL for an entry linked into the image */
    bool missing;               /* Requested VxWorks entry point is absent */
#endif
} Cgi;

static Cgi **cgiList;           /* walloc chain list of wp's to be closed */
static int cgiMax;              /* Size of walloc list */

/*
    Prefix client environment variables with CGI_ and deny execution-control names. Windows names compare case-
    insensitively.
 */
static cchar *envBlackList[] = {
    "ALL_PROXY",
    "AUTHSTATE",
    "BASHOPTS",
    "BASH_ENV",
    "CDPATH",
    "CLASSPATH",
    "COMSPEC",
    "ENV",
    "FPATH",
    "FTP_PROXY",
    "GEM_HOME",
    "GEM_PATH",
    "GLIBC_TUNABLES",
    "GLOBIGNORE",
    "GOPATH",
    "HOME",
    "HOSTALIASES",
    "HTTP_AUTHORIZATION",
    "HTTP_PROXY",
    "HTTP_PROXY_AUTHORIZATION",
    "HTTPS_PROXY",
    /* The server decodes the transfer coding; CONTENT_LENGTH describes what reaches stdin */
    "HTTP_TRANSFER_ENCODING",
    "IFS",
    "JAVA_HOME",
    "JAVA_TOOL_OPTIONS",
    "JDK_JAVA_OPTIONS",
    "LIBPATH",
    "LOCALDOMAIN",
    "LUA_CPATH",
    "LUA_INIT",
    "LUA_PATH",
    "NLSPATH",
    "NODE_OPTIONS",
    "NODE_PATH",
    "NO_PROXY",
    "NULLCMD",
    "OPENSSL_CONF",
    "OPENSSL_ENGINES",
    "OPENSSL_MODULES",
    "PATH",
    "PATHEXT",
    "PATH_LOCALE",
    "PERL5DB",
    "PERL5LIB",
    "PERL5OPT",
    "PERL5SHELL",
    "PERLIO_DEBUG",
    "PERLLIB",
    "PHPRC",
    "PHP_INI_SCAN_DIR",
    "PS4",
    "PYTHONHOME",
    "PYTHONINSPECT",
    "PYTHONPATH",
    "PYTHONUSERBASE",
    "READNULLCMD",
    "RES_OPTIONS",
    "RUBYLIB",
    "RUBYOPT",
    "SHELLOPTS",
    "SHLIB_PATH",
    "SYSTEMROOT",
    "TCLLIBPATH",
    "TERMCAP",
    "TERMINFO",
    "TERMINFO_DIRS",
    "TEMP",
    "TERMPATH",
    "TMP",
    "TMPDIR",
    "TMPPREFIX",
    "WINDIR",
    "ZDOTDIR",
    "_JAVA_OPTIONS",
    "__COMPAT_LAYER",
    /*
        POSIX environment names are case-sensitive; proxy consumers also read lowercase names.
     */
    "all_proxy",
    "ftp_proxy",
    "http_proxy",
    "https_proxy",
    "no_proxy",
    NULL
};

/*
    Loader and interpreter startup variables that carry a version suffix, such as LUA_INIT_5_4.
 */
static cchar *envBlackPrefixes[] = {
    "LD_",
    "LDR_",
    "_RLD",
    "DYLD_",
    "LUA_INIT_",
    "LUA_PATH_",
    "LUA_CPATH_",
    NULL
};

static bool envNameMatch(cchar *name, cchar *other)
{
#if ME_WIN_LIKE
    return scaselessmatch(name, other);
#else
    return smatch(name, other);
#endif
}


static bool envNameStarts(cchar *name, cchar *prefix)
{
#if ME_WIN_LIKE
    return sncaselesscmp(name, prefix, slen(prefix)) == 0;
#else
    return sstarts(name, prefix);
#endif
}


static bool inBlackList(cchar *name)
{
    cchar **bp;

    for (bp = envBlackList; *bp; bp++) {
        if (envNameMatch(name, *bp)) {
            return 1;
        }
    }
    for (bp = envBlackPrefixes; *bp; bp++) {
        if (envNameStarts(name, *bp)) {
            return 1;
        }
    }
    return 0;
}


/*
    Reserve CGI and runtime meta-variables even when unset. Include HTTP_ case variants because Windows
    environment lookups are case-insensitive (RFC 3875 4.1.18).
 */
static cchar *metaPrefixes[] = {
    "AUTH_", "CONTENT_", "CONTEXT_", "DOCUMENT_", "GATEWAY_", "HTTP_", "HTTPS", "ORIG_", "PATH_", "PHP_",
    "QUERY_", "REDIRECT_", "REMOTE_", "REQUEST_", "SCRIPT_", "SERVER_", "SSL_", "TLS_", "UNIQUE_ID", NULL
};

static bool isMetaVarName(cchar *name)
{
    cchar **mp;

    for (mp = metaPrefixes; *mp; mp++) {
        if (envNameStarts(name, *mp)) {
            return 1;
        }
    }
    return 0;
}


/*
    Check for names that would alias server environment variables on this platform.
 */
static bool shadowsServerVar(Webs *wp, cchar *name)
{
    WebsKey *s;

    for (s = hashFirst(wp->vars); s; s = hashNext(wp->vars, s)) {
        if (s->arg == 0 && envNameMatch(s->name.value.string, name)) {
            return 1;
        }
    }
    return 0;
}

/************************************ Forwards ********************************/

static void cgiCleanup(Cgi *cgip, int cid);
static int checkCgi(Cgi *cgip);
static void killCgi(Cgi *cgip);
static CgiPid launchCgi(Cgi *cgip, char *dir);
#if VXWORKS
static void releaseVxTask(Cgi *cgip);
#endif

/************************************* Code ***********************************/
/*
    Windows CGI launch helpers. All three are platform-independent C string logic
    (no Windows types or APIs) so the production Windows path and the macOS unit
    tests share a single implementation.
 */

PUBLIC bool isCgiExeOnWindows(cchar *cgiPath)
{
    ssize n;

    if (cgiPath == NULL) {
        return 0;
    }
    n = slen(cgiPath);
    if (n < 4) {
        return 0;
    }
    return scaselessmatch(cgiPath + n - 4, ".exe");
}


PUBLIC ssize appendWinArg(char *dp, ssize destMax, cchar *arg)
{
    cchar *cp;
    char  *start, *end;
    ssize len;
    int   bs;

    if (dp == NULL) {
        return 0;
    }
    start = dp;
    end = dp + destMax;
    /*
        Check capacity before each write.
     */
    if (arg == NULL || *arg == '\0') {
        /*
            Empty arg must be represented as "" so the receiving CRT recovers a
            zero-length argv element rather than nothing.
         */
        if (end - dp < 2) {
            return -1;
        }
        *dp++ = '"';
        *dp++ = '"';
        return dp - start;
    }
    if (!strpbrk(arg, " \t\n\v\r\f\"")) {
        if ((len = slen(arg)) > end - dp) {
            return -1;
        }
        memcpy(dp, arg, (size_t) len);
        return len;
    }
    if (end - dp < 1) {
        return -1;
    }
    *dp++ = '"';
    for (cp = arg; *cp; ) {
        bs = 0;
        while (*cp == '\\') {
            bs++;
            cp++;
        }
        if (*cp == '\0') {
            /* Trailing backslash run abuts the closing quote: double it. */
            if (end - dp < 2 * bs) {
                return -1;
            }
            for (; bs > 0; bs--) {
                *dp++ = '\\';
                *dp++ = '\\';
            }
            break;
        } else if (*cp == '"') {
            /* Backslash run before embedded quote: double-and-add-one. */
            if (end - dp < 2 * bs + 2) {
                return -1;
            }
            for (; bs > 0; bs--) {
                *dp++ = '\\';
                *dp++ = '\\';
            }
            *dp++ = '\\';
            *dp++ = '"';
            cp++;
        } else {
            if (end - dp < bs + 1) {
                return -1;
            }
            for (; bs > 0; bs--) {
                *dp++ = '\\';
            }
            *dp++ = *cp++;
        }
    }
    if (end - dp < 1) {
        return -1;
    }
    *dp++ = '"';
    return dp - start;
}


PUBLIC ssize buildWinCmdLine(char **argp, char *out, ssize outMax)
{
    char  **pArgs;
    char  *p, *end;
    ssize n;

    if (argp == NULL || out == NULL) {
        return 0;
    }
    if (outMax < 1) {
        return -1;
    }
    p = out;
    //  Reserve the terminator
    end = out + outMax - 1;
    for (pArgs = argp; *pArgs; pArgs++) {
        if ((n = appendWinArg(p, end - p, *pArgs)) < 0) {
            *out = '\0';
            return -1;
        }
        p += n;
        if (pArgs[1]) {
            if (p >= end) {
                *out = '\0';
                return -1;
            }
            *p++ = ' ';
        }
    }
    *p = '\0';
    return p - out;
}


static bool validEnvName(cchar *name)
{
    cchar *cp;

    if (!name || !(isalpha((uchar) *name) || *name == '_')) {
        return 0;
    }
    for (cp = name + 1; *cp; cp++) {
        if (!(isalnum((uchar) *cp) || *cp == '_')) {
            return 0;
        }
    }
    return 1;
}


static bool hasControlChar(cchar *str)
{
    cchar *cp;

    for (cp = str; *cp; cp++) {
        if (iscntrl((uchar) *cp)) {
            return 1;
        }
    }
    return 0;
}


/*
    ISINDEX terms must start with a letter or digit and contain no controls or decoded =.
 */
static bool validIsindexArg(cchar *arg)
{
    return arg && isalnum((uchar) *arg) && !hasControlChar(arg) && !strchr(arg, '=');
}


/*
    Process a CGI request.
    Return true to indicate the request was handled, even for errors.
 */
/*
    Check the executable's extension, not the last PATH_INFO segment, against the route.
 */
static bool cgiExtensionAllowed(WebsRoute *route, cchar *path)
{
    cchar *base, *ext;
    char  *lower;
    bool  allowed;

    if (route == 0 || route->extensions < 0) {
        return 1;
    }
    base = (base = strrchr(path, '/')) != 0 ? base + 1 : path;
#if ME_WIN_LIKE
    base = strrchr(base, '\\') ? strrchr(base, '\\') + 1 : base;
#endif
    if ((ext = strrchr(base, '.')) == 0 || ext[1] == '\0') {
        return 0;
    }
    lower = slower(sclone(&ext[1]));
    allowed = hashLookup(route->extensions, lower) != 0;
    wfree(lower);
    return allowed;
}


/*
    Split ISINDEX queries only without a literal =, using & for POST and + otherwise.
    The caller owns the returned vector and *backing; element zero borrows cgiPath.
    On failure, free allocations and return NULL with the HTTP status in *status.
 */
static char **buildCgiArgs(cchar *cgiPath, cchar *query, char **backing, int *status)
{
    char **argp, *cp, *tok, *copy;
    int  n, argpsize;

    *backing = 0;
    *status = HTTP_CODE_INTERNAL_SERVER_ERROR;
    argpsize = 10;
    if ((argp = walloc(argpsize * (ssize) sizeof(char*))) == 0) {
        *status = HTTP_CODE_NOT_FOUND;
        return 0;
    }
    *argp = (char*) cgiPath;
    n = 1;
    copy = 0;

    if (query && strchr(query, '=') == NULL) {
        /*
            RFC 3875 4.4: the search string is split at each '+' and each term is then decoded, so an
            encoded space or '+' stays inside its term.
         */
        copy = sclone(query);
        for (cp = stok(copy, "+", &tok); cp != NULL; ) {
            websDecodeUrl(cp, cp, -1);
            /*
                Require word-like terms to exclude command options, response files, and redirections.
             */
            if (!validIsindexArg(cp)) {
                *status = HTTP_CODE_BAD_REQUEST;
                wfree(argp);
                wfree(copy);
                return 0;
            }
            if (n == 1) {
                /*
                    SECURITY (CWE-88): Precede ISINDEX search terms with a POSIX
                    end-of-options marker so a conformant CGI program treats them
                    as positional operands, never as options. Defence in depth on
                    top of the '-' rejection above.
                 */
                *(argp + n++) = "--";
            }
            *(argp + n++) = cp;
            trace(5, "ARG[%d] %s", n - 1, argp[n - 1]);
            if (n >= argpsize) {
                argpsize *= 2;
                if (argpsize > ME_GOAHEAD_LIMIT_CGI_ARGS) {
                    *status = HTTP_CODE_REQUEST_TOO_LARGE;
                    wfree(argp);
                    wfree(copy);
                    return 0;
                }
                argp = wrealloc(argp, argpsize * (ssize) sizeof(char*));
            }
            cp = stok(NULL, "+", &tok);
        }
    }
    *(argp + n) = NULL;
    *backing = copy;
    return argp;
}


/*
    Return a caller-owned array of allocated key=value strings, growing as needed.
 */
static char **buildCgiEnv(Webs *wp)
{
    WebsKey *s;
    char    **envp;
    cchar   *name;
    int     n, envpsize;

    envpsize = 64;
    envp = walloc(envpsize * (ssize) sizeof(char*));
    n = 0;
    if (wp->vars >= 0) {
        for (s = hashFirst(wp->vars); s != NULL; s = hashNext(wp->vars, s)) {
            if (s->content.valid && s->content.type == string) {
                name = s->name.value.string;
                /*
                    Export only portable environment names; delimiters or padding could inject assignments or alias
                    server variables.
                 */
                if (!validEnvName(name)) {
                    trace(5, "cgi: Skip request variable with a non-portable name");
                    continue;
                }
                if (inBlackList(name)) {
                    continue;
                }
                /*
                    SECURITY (CVE-2014-6271): bash imports a value that begins with "() {" as a
                    function definition. The payload is carried in the value, never the name.
                 */
                if (sstarts(s->content.value.string, "() {")) {
                    continue;
                }
                if (s->arg != 0 && *ME_GOAHEAD_CGI_VAR_PREFIX != '\0') {
                    envp[n++] = sfmt("%s%s=%s", ME_GOAHEAD_CGI_VAR_PREFIX, name, s->content.value.string);
                } else if (s->arg != 0 && (isMetaVarName(name) || shadowsServerVar(wp, name))) {
                    trace(5, "cgi: Skip request variable named like a server variable");
                    continue;
                } else {
                    envp[n++] = sfmt("%s=%s", name, s->content.value.string);
                }
                trace(5, "Env[%d] %s (value omitted)", n, s->name.value.string);
                if (n >= envpsize) {
                    envpsize *= 2;
                    envp = wrealloc(envp, envpsize * (ssize) sizeof(char*));
                }
            }
        }
    }
    *(envp + n) = NULL;
    return envp;
}


PUBLIC bool cgiHandler(Webs *wp)
{
    Cgi   *cgip;
    char  cgiPrefix[ME_GOAHEAD_LIMIT_FILENAME], *stdIn, *stdOut, cwd[ME_GOAHEAD_LIMIT_FILENAME];
    char  *cp, *cgiName, *cgiPath, **argp, **envp, *query, *dir, *documents, *exe;
    ssize len, stdinLen;
    bool  missing;
    int   status, cid;

    assert(websValid(wp));

    missing = 0;
    websSetEnv(wp);

    /* Stage buffered input when rerouting after the body has already been read. */
    if (wp->cgiStdin == NULL && bufLen(&wp->input) > 0) {
        if ((wp->cgiStdin = websTempFileOpen(NULL, "cgi", &wp->cgifd)) == 0) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create CGI file");
            return 1;
        }
        websProcessCgiData(wp);
        if (wp->finalized) {
            return 1;
        }
    }
    /*
        Use the bytes sent to stdin for CONTENT_LENGTH; multipart file extraction changes that size.
     */
    stdinLen = 0;
    if (wp->cgifd >= 0) {
        stdinLen = (ssize) lseek(wp->cgifd, 0, SEEK_END);
        close(wp->cgifd);
        wp->cgifd = -1;
    }
    websSetVarFmt(wp, "CONTENT_LENGTH", "%zd", stdinLen > 0 ? stdinLen : 0);

    /*
        Reject overflow before scanning; scopy clears the destination on failure.
     */
    if (scopy(cgiPrefix, sizeof(cgiPrefix), wp->path) < 0) {
        websError(wp, HTTP_CODE_REQUEST_URL_TOO_LARGE, "CGI path too long");
        return 1;
    }
    /*
        Resolve the program from the first segment after the route prefix, inside its directory.
     */
    for (len = slen(wp->route->prefix); len > 0 && wp->route->prefix[len - 1] == '/'; len--) { }
    if (cgiPrefix[len] != '/' || cgiPrefix[len + 1] == '\0') {
        websError(wp, HTTP_CODE_NOT_FOUND, "Missing CGI name");
        return 1;
    }
    cgiPrefix[len] = '\0';
    cgiName = &cgiPrefix[len + 1];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get working directory");
        return 1;
    }
    /*
        Make dir absolute because the program path is resolved against it and the child starts in it.
     */
    dir = wp->route->dir ? websAbsolutePath(wp->route->dir) : sclone(cwd);

    if ((cp = strchr(cgiName, '/')) != NULL) {
        //  PATH_INFO is a URI path, so it is translated under the documents directory
        documents = websAbsolutePath(websGetDocuments());
        websSetVar(wp, "PATH_INFO", cp);
        websSetVarFmt(wp, "PATH_TRANSLATED", "%s%s", documents, cp);
        wfree(documents);
        *cp = '\0';
    } else {
        websSetVar(wp, "PATH_INFO", "");
        websSetVar(wp, "PATH_TRANSLATED", "");
    }
    cgiPath = sfmt("%s%s/%s", dir, cgiPrefix, cgiName);
    websSetVarFmt(wp, "SCRIPT_NAME", "%s/%s", cgiPrefix, cgiName);

    //  Reject device names before program lookup can open them.
    if (websIsReservedDevice(cgiName)) {
        trace(2, "Refusing %s: reserved device name", cgiPath);
        websError(wp, HTTP_CODE_NOT_FOUND, "CGI program file does not exist");
        wfree(cgiPath);
        goto done;
    }

/*
    See if the file exists and is executable.  If not error out.  Don't do this step for VxWorks, since the module
    may already be part of the OS image, rather than in the file system.
 */
#if !VXWORKS
    {
        WebsStat sbuf;
        /*
            SECURITY Acceptable: TOCTOU race condition between stat/access and execution.
            Embedded system file system security is out of scope for this project.
         */
        if (stat(cgiPath, &sbuf) != 0 || !S_ISREG(sbuf.st_mode)) {
            exe = sfmt("%s.exe", cgiPath);
            if (stat(exe, &sbuf) == 0 && S_ISREG(sbuf.st_mode)) {
                wfree(cgiPath);
                cgiPath = exe;
            } else {
                trace(5, "Cannot find CGI program: %s", cgiPath);
                websError(wp, HTTP_CODE_NOT_FOUND, "CGI program file does not exist");
                wfree(exe);
                wfree(cgiPath);
                goto done;
            }
        }
#if ME_WIN_LIKE
        /*
            Windows CGI: only .exe is supported. .bat / .cmd are deliberately not
            supported because kernel32's CreateProcessInternal transparently routes
            script-extension targets through cmd.exe, exposing the spawned process
            to OS command injection from any caller that does not exhaustively escape
            for cmd.exe parsing - which is not mechanically possible in the general
            case (notably %VAR% expansion). The gate is a true suffix match, not a
            substring match, so paths like 'payload.exe.bat' cannot smuggle a .bat
            past it.
         */
        if (!isCgiExeOnWindows(cgiPath))
#else
        if (cgiPath == NULL || access(cgiPath, X_OK) != 0)
#endif
        {
            websError(wp, HTTP_CODE_NOT_FOUND, "CGI process file is not executable");
            wfree(cgiPath);
            goto done;
        }
    }
#endif /* ! VXWORKS */
    websSetVar(wp, "SCRIPT_FILENAME", cgiPath);
    if (!cgiExtensionAllowed(wp->route, cgiPath)) {
        trace(5, "CGI program %s does not have an extension the route permits", cgiPath);
        websError(wp, HTTP_CODE_NOT_FOUND, "CGI program file does not exist");
        wfree(cgiPath);
        goto done;
    }
    if ((argp = buildCgiArgs(cgiPath, wp->query, &query, &status)) == NULL) {
        websError(wp, status, status == HTTP_CODE_BAD_REQUEST ? "Bad ISINDEX argument" :
                  status == HTTP_CODE_REQUEST_TOO_LARGE ? "Too many arguments" : "Cannot allocate CGI args");
        wfree(cgiPath);
        goto done;
    }
    envp = buildCgiEnv(wp);

    if (wp->cgiStdin == NULL) {
        wp->cgiStdin = websGetCgiCommName();
    }
    stdIn = wp->cgiStdin;
    stdOut = websGetCgiCommName();

    /*
        Register before launch for shared failure cleanup. Retain query storage with argp because
        taskSpawn reads the arguments after launchCgi returns.
     */
    cid = wallocObject(&cgiList, &cgiMax, sizeof(Cgi));
    cgip = cgiList[cid];
    cgip->stdIn = stdIn;
    cgip->stdOut = stdOut;
    cgip->cgiPath = cgiPath;
    cgip->query = query;
    cgip->argp = argp;
    cgip->envp = envp;
    cgip->wp = wp;
    cgip->fplacemark = 0;
    cgip->txLen = -1;
    cgip->txBytes = 0;
    cgip->txExcess = 0;
    cgip->txSuppress = 0;
#if WINDOWS
    cgip->job = NULL;
#endif
#if VXWORKS
    cgip->fdin = cgip->fdout = -1;
    cgip->missing = 0;
#endif
    if (!stdIn || !stdOut || (cgip->handle = launchCgi(cgip, dir)) == (CgiPid) - 1) {
#if VXWORKS
        missing = cgip->missing;
#endif
        /*
            Release the registry slot before error handling can tear down the request. cgiCleanup removes staging files.
         */
        cgip->handle = 0;
        cgiCleanup(cgip, cid);
        if (missing) {
            websError(wp, HTTP_CODE_NOT_FOUND, "CGI program file does not exist");
        } else {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "failed to spawn CGI task");
        }
    }
done:
    wfree(dir);
    return 1;
}


PUBLIC int websCgiOpen(void)
{
    websDefineHandler("cgi", 0, cgiHandler, 0, 0);
    return 0;
}


/*
    Release a CGI registry slot and everything it owns. The child must already have been reaped.
    cgip->stdIn is borrowed from wp->cgiStdin and is freed by the Webs teardown, so unlink it here
    but do not free it.
 */
static void cgiCleanup(Cgi *cgip, int cid)
{
    char **ep;

#if VXWORKS
    releaseVxTask(cgip);
#endif
    if (cgip->stdIn) {
        unlink(cgip->stdIn);
    }
    if (cgip->stdOut) {
        unlink(cgip->stdOut);
    }
    cgiMax = wfreeHandle(&cgiList, cid);
    for (ep = cgip->envp; ep != NULL && *ep != NULL; ep++) {
        wfree(*ep);
    }
    wfreeset(cgip->cgiPath);
    wfreeset(cgip->stdOut);
    wfreeset(cgip->query);
    wfreeset(cgip->argp);
    wfreeset(cgip->envp);
#if WINDOWS
    if (cgip->job) {
        CloseHandle(cgip->job);
        cgip->job = NULL;
    }
#endif
    wfree(cgip);
}


/*
    Detach a request from the CGI registry. cgiList publishes a Webs pointer and the request's temp
    file names, but the registry does not own them. The request can die by timeout, client close or
    error while its child is still running, so the owner must unregister before it releases itself,
    otherwise websCgiPoll reads and writes through freed memory on its next pass. There is nobody
    left to deliver output to, so kill and reap the orphaned child rather than leaking it.
 */
PUBLIC void websCgiDetach(Webs *wp)
{
    Cgi *cgip;
    int cid;

    for (cid = 0; cid < cgiMax; cid++) {
        if ((cgip = cgiList[cid]) != NULL && cgip->wp == wp) {
            killCgi(cgip);
            cgiCleanup(cgip, cid);
            break;
        }
    }
}


PUBLIC bool websProcessCgiData(Webs *wp)
{
    ssize nbytes;

    nbytes = bufLen(&wp->input);
    trace(5, "cgi: write %zd bytes to CGI program", nbytes);
    if (write(wp->cgifd, bufStart(&wp->input), (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot write to CGI gateway");
    } else {
        trace(5, "cgi: write %zd bytes to CGI program", nbytes);
    }
    websConsumeInput(wp, nbytes);
    return 1;
}


static void writeCgiHeaders(Webs *wp, int status, ssize contentLength, cchar *location, cchar *contentType)
{
    trace(5, "cgi: Start response headers");
    websSetStatus(wp, status);
    websWriteHeaders(wp, contentLength, location);
    websWriteHeader(wp, "Pragma", "no-cache");
    // Use the program's output type, not the script path's extension.
    websWriteHeader(wp, "Content-Type", "%s", contentType ? contentType : "text/plain");
}


/* The server owns connection and framing headers. Keys are lowercase. */
static bool isHopHeader(cchar *key)
{
    static cchar *hop[] = {
        "connection", "keep-alive", "proxy-connection", "te", "trailer", "transfer-encoding", "upgrade", 0
    };
    cchar        **cp;

    for (cp = hop; *cp; cp++) {
        if (strcmp(key, *cp) == 0) {
            return 1;
        }
    }
    return 0;
}


/*
    Parse a Content-Length declared by the CGI program. The value is a claim by an untrusted child,
    not a fact, so admit it only as a plain non-negative decimal that fits an ssize. A sign, trailing
    junk or an overflowing value yields -1, which selects chunked framing so the byte count actually
    forwarded frames the response instead.
 */
static ssize parseCgiContentLength(cchar *value)
{
    cchar *cp;
    ssize len;

    if (value == NULL || *value == '\0') {
        return -1;
    }
    len = 0;
    for (cp = value; *cp; cp++) {
        if (!isdigit((uchar) *cp)) {
            return -1;
        }
        if (len > (MAXSSIZE - (*cp - '0')) / 10) {
            return -1;
        }
        len = (len * 10) + (*cp - '0');
    }
    return len;
}


/*
    Parse a status code declared by the CGI. RFC 3875 defines the value as three digits optionally
    followed by a space and a reason phrase; anything else is a malformed response, not a status.
    Return 0 for a value that cannot be a status so the caller answers on its own behalf rather than
    passing the CGI's claim through.
 */
static int parseCgiStatus(cchar *value)
{
    int code, i;

    if (value == NULL) {
        return 0;
    }
    code = 0;
    for (i = 0; i < 3; i++) {
        if (!isdigit((uchar) value[i])) {
            return 0;
        }
        code = (code * 10) + (value[i] - '0');
    }
    if (value[3] != '\0' && value[3] != ' ' && value[3] != '\t') {
        return 0;
    }
    /*
        Refuse 1XX. An interim status as the final response stalls the client waiting for a second
        response the gateway will never send. Codes above 5XX misframe nothing, so three digits is
        the only bound applied above.
     */
    if (code < 200) {
        return 0;
    }
    return code;
}


/*
    One tokenized line of the CGI response header block.
 */
typedef struct CgiHeader {
    char *key;                              /* Lower case header name. Zero once consumed as a control header */
    char *value;                            /* Trimmed header value */
} CgiHeader;


/* Split CGI headers only at LF. Strip preceding CRs, and reject CRs elsewhere in a line. */
static char *getCgiLine(char **next, int *malformed)
{
    char  *line, *nl;
    ssize len;

    line = *next;
    if (*line == '\0') {
        return NULL;
    }
    if ((nl = strchr(line, '\n')) != NULL) {
        *nl = '\0';
        *next = nl + 1;
    } else {
        *next = &line[slen(line)];
    }
    len = slen(line);
    while (len > 0 && line[len - 1] == '\r') {
        line[--len] = '\0';
    }
    if (strchr(line, '\r') != NULL) {
        trace(5, "cgi: bare CR in response header line");
        *malformed = 1;
    }
    return line;
}


/*
    Parse CGI headers from the nbytes read into buf. Return bytes consumed, zero if incomplete, or -1 to defer.
    Deferral leaves state unchanged; staged counts unread output bytes on disk.
    A NUL byte inside the header block is a malformed response.
 */
static ssize parseCgiHeaders(Cgi *cgip, char *buf, ssize nbytes, ssize staged)
{
    CgiHeader *headers;
    Webs      *wp;
    char      *end, *cp, *line, *code, *location, *contentType;
    ssize     len, contentLength, bodyLimit, availableBody;
    int       count, i, nlines, status, statusSeen, seenLength, malformed;

    wp = cgip->wp;
    status = HTTP_CODE_OK;
    statusSeen = 0;
    seenLength = 0;
    malformed = 0;
    contentLength = -1;
    contentType = 0;
    location = 0;

    /* The first blank line ends the headers, including an empty header block. Scan by length, so a NUL cannot hide it. */
    for (cp = buf; ; cp = end + 1) {
        if (cp[0] == '\n' || (cp[0] == '\r' && cp[1] == '\n')) {
            end = cp;
            len = (cp[0] == '\n') ? 1 : 2;
            break;
        }
        if ((end = memchr(cp, '\n', (size_t) (nbytes - (cp - buf)))) == NULL) {
            return 0;
        }
    }
    /*
        Detect headers before modifying the separator; headerless output is returned verbatim as the body.
        An nph status line alone also forms a header block.
     */
    if (memchr(buf, ':', (size_t) (end - buf)) == NULL && strncmp(buf, "HTTP/1.", 7) != 0) {
        /* No headers found */
        return 0;
    }
    /*
        The tokenizer below works on strings, so a NUL would truncate the block and drop the headers after it,
        the Status among them. Answer on the gateway's behalf instead.
     */
    if (memchr(buf, '\0', (size_t) (end - buf)) != NULL) {
        trace(5, "cgi: NUL byte in response header block");
        malformed = 1;
    }
    *end = '\0';
    end += len;
    cp = buf;
    if (strncmp(cp, "HTTP/1.", 7) == 0) {
        /*
            Apply an nph status line using the same validation as Status.
         */
        line = getCgiLine(&cp, &malformed);
        if ((code = strchr(line, ' ')) == NULL || (status = parseCgiStatus(code + 1)) == 0) {
            trace(5, "cgi: bad status line \"%s\"", line);
            status = HTTP_CODE_OK;
            malformed = 1;
        }
        statusSeen = 1;
    }
    /* Capture lines before the two header passes because tokenization is destructive. */
    nlines = 1;
    for (line = cp; (line = strchr(line, '\n')) != 0; line++) {
        nlines++;
    }
    headers = walloc(nlines * (ssize) sizeof(CgiHeader));
    count = 0;
    while (count < nlines && (line = getCgiLine(&cp, &malformed)) != NULL) {
        /*
            Tokenize one complete header line before splitting the name from the value. Splitting
            on ":" first leaves the remainder of the header block inside "value", so a value
            containing CR/LF carries the following headers through to the client unchecked.
         */
        if (strchr(line, ':') == NULL) {
            trace(5, "cgi: bad response http header line: \"%s\"", line);
            continue;
        }
        headers[count].key = slower(ssplit(line, ":", &headers[count].value));
        headers[count].value = strim(headers[count].value, " \t", WEBS_TRIM_BOTH);
        count++;
    }
    /*
        Collect the CGI control headers before emitting anything. RFC 3875 places no ordering
        requirement on the response headers, so a control header may follow a pass-through header and
        must still shape the status line and standard headers.
     */
    for (i = 0; i < count; i++) {
        if (strcmp(headers[i].key, "location") == 0) {
            location = headers[i].value;
            /*
                RFC 3875 6.2 requires a URI. An empty value is a malformed response, not a redirect to the
                request's own URL.
             */
            if (*location == '\0' || !websValidHeaderValue(location)) {
                trace(5, "cgi: bad Location");
                malformed = 1;
            }
        } else if (strcmp(headers[i].key, "status") == 0) {
            if ((status = parseCgiStatus(headers[i].value)) == 0) {
                trace(5, "cgi: bad Status \"%s\"", headers[i].value);
                malformed = 1;
            }
            statusSeen = 1;
        } else if (strcmp(headers[i].key, "content-type") == 0) {
            contentType = headers[i].value;

            if (*contentType == '\0' || !websValidHeaderValue(contentType)) {
                trace(5, "cgi: bad Content-Type");
                malformed = 1;
            }
        } else if (strcmp(headers[i].key, "content-length") == 0) {
            len = parseCgiContentLength(headers[i].value);
            /*
                RFC 9110 makes a message carrying two different Content-Length values unrecoverably
                invalid. Taking the last one, as this did, makes the framing depend on the order the
                CGI happened to write them. Repeating the same value is permitted and is collapsed.
             */
            if (seenLength && len != contentLength) {
                trace(5, "cgi: conflicting Content-Length \"%s\"", headers[i].value);
                malformed = 1;
            } else if (len < 0) {
                trace(5, "cgi: bad Content-Length \"%s\" - using chunked", headers[i].value);
            }
            contentLength = len;
            seenLength = 1;
        } else {
            continue;
        }
        /*
            Consumed by writeCgiHeaders below, so do not pass it through as well.
         */
        headers[i].key = 0;
    }
    /*
        Location without Status implies 302, including local paths (RFC 3875 6.2).
     */
    if (location && !statusSeen) {
        status = HTTP_CODE_MOVED_TEMPORARILY;
    }
    /*
        A response this gateway cannot interpret is answered on the gateway's own behalf. Forward
        none of it, neither body nor remaining headers, which may carry cookies or cache directives
        meant to accompany a response the client is not going to receive.
     */
    if (malformed) {
        status = HTTP_CODE_INTERNAL_SERVER_ERROR;
        contentLength = 0;
        location = 0;
        contentType = 0;
    }

    /*
        Decide whether this response may carry a body at all, before any of it is written. Body bytes
        after a bodyless status are unframed data the client reads as the next response. txSuppress
        marks honest suppression, distinct from a CGI over-writing its declared length, so the caller
        neither reports it nor closes. A HEAD keeps the framing an equivalent GET would use, a declared
        length or chunked, and websWriteEndHeaders writes no chunk trailer for a HEAD; every other
        case declares zero, never unknown, as unknown selects chunked framing whose trailer is itself
        a body byte.
     */
    bodyLimit = contentLength;
    if (websStatusHasNoBody(status) || smatch(wp->method, "HEAD") || malformed) {
        cgip->txSuppress = 1;
        bodyLimit = 0;
        if (!smatch(wp->method, "HEAD")) {
            contentLength = 0;
        }
    }
    /*
        Defer headers while a running CGI may still meet its declared length.
        Decide keep-alive before writing Connection; unknown-length responses can stream.
     */
    availableBody = staged - (end - buf);
    if (!cgip->txSuppress && contentLength >= 0) {
        if (cgip->handle && availableBody <= contentLength) {
            trace(5, "cgi: defer response headers until the declared length %zd is settled (%zd staged)",
                  contentLength, availableBody);
            wfree(headers);
            return -1;
        }
        if (availableBody != contentLength) {
            /*
                Clear keep-alive before headers when the declared length cannot be delivered.
             */
            trace(5, "cgi: declared Content-Length %zd but staged %zd bytes - closing after the response",
                  contentLength, availableBody);
            wp->flags &= ~WEBS_KEEP_ALIVE;
        }
    }
    writeCgiHeaders(wp, status, contentLength, location, contentType);
    cgip->txLen = bodyLimit;

    /*
        Now pass all other headers back to the client
     */
    for (i = 0; i < count && !malformed; i++) {
        if (headers[i].key == 0) {
            continue;
        }
        if (isHopHeader(headers[i].key)) {
            trace(5, "cgi: dropping hop-by-hop response header \"%s\"", headers[i].key);
        } else if (websWriteHeader(wp, headers[i].key, "%s", headers[i].value) < 0) {
            trace(5, "cgi: dropped response header \"%s\"", headers[i].key);
        }
    }
    wfree(headers);
    websWriteEndHeaders(wp);
    return end - buf;
}


/*
    Hand CGI output to the response buffers without blocking the event loop. Return the number of bytes accepted;
    the caller leaves the rest staged for a later poll. A closed connection accepts everything, so the staged
    output drains and the request completes.
 */
static ssize relayCgiOutput(Webs *wp, cchar *buf, ssize len)
{
    WebsBuf *op;
    ssize   accepted, room, thisWrite;

    accepted = 0;
    while (len > 0) {
        if (wp->state >= WEBS_COMPLETE) {
            return accepted + len;
        }
        op = (wp->flags & WEBS_CHUNKING) ? &wp->chunkbuf : &wp->output;
        if ((room = bufRoom(op)) <= 0) {
            websFlush(wp, 0);
            if (wp->state >= WEBS_COMPLETE) {
                return accepted + len;
            }
            if ((room = bufRoom(op)) <= 0) {
                break;
            }
        }
        thisWrite = min(room, len);
        if (websWriteBlock(wp, buf, thisWrite) != thisWrite) {
            return accepted + len;
        }
        buf += thisWrite;
        len -= thisWrite;
        accepted += thisWrite;
    }
    return accepted;
}


/*
    Can the response buffers take more output, or is the client not reading? Flush first so a draining socket
    makes room, and treat a closed connection as room so the staged output is consumed.
 */
static int relayHasRoom(Webs *wp)
{
    WebsBuf *op;

    websFlush(wp, 0);
    if (wp->state >= WEBS_COMPLETE) {
        return 1;
    }
    op = (wp->flags & WEBS_CHUNKING) ? &wp->chunkbuf : &wp->output;
    return bufRoom(op) > 0;
}


/*
    Has every staged byte of CGI output been handed to the response?
 */
static int cgiOutputDrained(Cgi *cgip)
{
    WebsStat sbuf;

    return stat(cgip->stdOut, &sbuf) != 0 || sbuf.st_size <= cgip->fplacemark;
}


PUBLIC void websCgiGatherOutput(Cgi *cgip)
{
    Webs     *wp;
    WebsStat sbuf;
    char     *buf;
    ssize    bufSize, nbytes, skip, nbody, room, surplus, accepted;
    int      fdout;

    /*
        OPT - currently polling and doing a stat each poll. Also doing open/close each chunk.
        If the CGI process writes partial headers, this repeatedly reads the data until complete
        headers are written or more than ME_GOAHEAD_LIMIT_HEADERS of data is received.
     */
    wp = cgip->wp;
    if ((wp->flags & WEBS_HEADERS_CREATED) && !relayHasRoom(wp)) {
        return;
    }
    if ((stat(cgip->stdOut, &sbuf) == 0) && (sbuf.st_size > cgip->fplacemark)) {
        if ((fdout = open(cgip->stdOut, O_RDONLY | O_BINARY | O_NOFOLLOW, 0444)) >= 0) {
            /*
                Check to see if any data is available in the output file and send its contents to the socket.
                Write the HTTP header on our first pass. The header must fit into ME_GOAHEAD_LIMIT_BUFFER.
             */
            bufSize = ME_GOAHEAD_LIMIT_HEADERS + 2;
            buf = walloc(bufSize);
            lseek(fdout, cgip->fplacemark, SEEK_SET);
            while ((nbytes = read(fdout, buf, (uint) (bufSize - 1))) > 0) {
                buf[nbytes] = 0;
                skip = 0;
                if (!(wp->flags & WEBS_HEADERS_CREATED)) {
                    skip = parseCgiHeaders(cgip, buf, nbytes, (ssize) (sbuf.st_size - cgip->fplacemark));
                    if (skip < 0) {
                        /*
                            Retry the same staged bytes once CGI length is known.
                         */
                        break;
                    }
                    if (skip == 0) {
                        if (cgip->handle && sbuf.st_size < ME_GOAHEAD_LIMIT_HEADERS) {
                            trace(5, "cgi: waiting for http headers");
                            break;
                        } else {
                            /*
                                The CGI emitted no recognizable header block. Emit a default set and
                                close it, otherwise the program output below lands inside the header
                                block and any "Name: value" line it writes becomes a real header.
                                Closing also sets WEBS_HEADERS_CREATED so the output is not re-parsed
                                as headers, and enables the promised chunked framing. Default the
                                RFC 3875 required Content-Type to text/plain so headerless output
                                cannot be sniffed as HTML.
                             */
                            trace(5, "cgi: missing http headers - create default headers");
                            writeCgiHeaders(wp, HTTP_CODE_OK, -1, 0, "text/plain");
                            websWriteEndHeaders(wp);
                        }
                    }
                }
                nbody = nbytes - skip;
                surplus = 0;
                /*
                    Surplus over the declared Content-Length is read by the client as the start of the
                    next response on a keep-alive connection. Drop it rather than emit it and take the
                    connection down. Only accumulate the count here, as this branch is re-entered on
                    every later read; the total is reported once when the child exits.
                 */
                if (cgip->txSuppress) {
                    /*
                        The response was framed as carrying no body, so dropping one is the framing
                        working, not a fault. Say nothing and leave the connection reusable.
                     */
                    nbody = 0;

                } else if (cgip->txLen >= 0) {
                    /*
                        Count against remaining length to avoid overflow; saturate the surplus count.
                     */
                    room = cgip->txLen - cgip->txBytes;
                    if (nbody > room) {
                        surplus = nbody - room;
                        nbody = room;
                    }
                }
                /*
                    Relay what the response buffers accept without blocking. The rest stays staged, with its
                    place marked, for the next poll; the event loop never waits on a client that is not reading.
                 */
                accepted = relayCgiOutput(wp, &buf[skip], nbody);
                if (cgip->txLen >= 0) {
                    cgip->txBytes += accepted;
                }
                if (accepted > 0) {
                    trace(5, "cgi: write %zd bytes to client", accepted);
                }
                if (accepted < nbody) {
                    cgip->fplacemark += (off_t) (skip + accepted);
                    break;
                }
                if (surplus > 0) {
                    cgip->txExcess = (cgip->txExcess > MAXSSIZE - surplus) ? MAXSSIZE : cgip->txExcess + surplus;
                    wp->flags &= ~WEBS_KEEP_ALIVE;
                }
                cgip->fplacemark += (off_t) nbytes;
            }
            wfree(buf);
            close(fdout);
        } else {
            trace(5, "cgi: open failed");
        }
    }
}


/*
    Any entry in the cgiList need to be checked to see if it has completed, and if so, process its output and clean up.
    Return time till next poll.
 */
int websCgiPoll(void)
{
    Webs *wp;
    Cgi  *cgip;
    int  cid;

    for (cid = 0; cid < cgiMax; cid++) {
        if ((cgip = cgiList[cid]) != NULL) {
            wp = cgip->wp;
            websCgiGatherOutput(cgip);
            if (cgip->handle && checkCgi(cgip) == 0) {
                /*
                    We get here if the CGI process has terminated. Clean up.
                 */
                cgip->handle = 0;
                websCgiGatherOutput(cgip);
            }
            /*
                Output a slow client has not yet accepted stays staged after the program exits. Complete the
                request once it has all been relayed, or when the request timeout frees the connection.
             */
            if (cgip->handle == 0 && cgiOutputDrained(cgip)) {
#if WINDOWS
                /*
                    Windows can have delayed notification through the file system after process exit. Look again
                    on later polls rather than block the event loop.
                 */
                if (cgip->fplacemark == 0) {
                    if (cgip->outputDeadline == 0) {
                        cgip->outputDeadline = GetTickCount64() + CGI_OUTPUT_GRACE;
                    }
                    if (GetTickCount64() < cgip->outputDeadline) {
                        continue;
                    }
                }
#endif
                if (cgip->fplacemark == 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "CGI generated no output");
                } else {
                    /*
                        Report a length the CGI could not honour, once per request. Truncating the
                        surplus leaves txBytes equal to txLen, so only the accumulated excess records
                        it. A shortfall is still unhandled here: the headers are on the wire and the
                        framing cannot be corrected, so close rather than keep a desynchronised
                        stream alive.
                     */
                    if (cgip->txExcess > 0) {
                        trace(2, "cgi: declared Content-Length %zd and dropped %zd surplus bytes",
                              cgip->txLen, cgip->txExcess);

                    } else if (cgip->txLen >= 0 && cgip->txBytes != cgip->txLen) {
                        trace(2, "cgi: declared Content-Length %zd but wrote %zd bytes",
                              cgip->txLen, cgip->txBytes);
                        wp->flags &= ~WEBS_KEEP_ALIVE;
                    }
                    trace(5, "cgi: Request complete - calling websDone");
                    websDone(wp);
                }
                /*
                    Remove the temporary re-direction files and free the registry slot. This must
                    happen before the Webs object is released below, as the slot borrows wp->cgiStdin.
                 */
                cgiCleanup(cgip, cid);
                /* Completion clears WEBS_KEEP_ALIVE even on reused connections; free only closed connections. */
                websPump(wp);
                if (wp->flags & WEBS_CLOSED) {
                    websFree(wp);
                    /* wp no longer valid */
                }
            }
        }
    }
    return cgiMax ? 10 : MAXINT;
}


/*
    Returns a pointer to an allocated qualified unique temporary file name. This filename must eventually be deleted
       with
    wfree().
 */
PUBLIC char *websGetCgiCommName(void)
{
    return websTempFile(NULL, "cgi");
}


#if ME_UNIX_LIKE || QNX
/* Use descriptor writes in the child to avoid flushing inherited parent stdio buffers. */
static void childError(cchar *msg)
{
    ssize len, written;

    for (len = slen(msg); len > 0; len -= written, msg += written) {
        if ((written = write(1, msg, (size_t) len)) <= 0) {
            break;
        }
    }
}


/*
    Close non-stdio descriptors before exec so CGI cannot inherit server resources.
 */
static void closeInheritedFiles(void)
{
#if FREEBSD
    closefrom(3);
#else
    long max;
    int  fd;

#if LINUX || MACOSX
    {
        struct dirent *dp;
        DIR           *dir;

    #if LINUX
        dir = opendir("/proc/self/fd");
    #else
        dir = opendir("/dev/fd");
    #endif
        if (dir != NULL) {
            while ((dp = readdir(dir)) != NULL) {
                fd = atoi(dp->d_name);
                if (fd > 2 && fd != dirfd(dir)) {
                    close(fd);
                }
            }
            closedir(dir);
            return;
        }
    }
#endif
    max = sysconf(_SC_OPEN_MAX);
    for (fd = 3; fd < max; fd++) {
        close(fd);
    }
#endif
}


/*
    Launch the CGI process and return a handle to it.
 */
static CgiPid launchCgi(Cgi *cgip, char *dir)
{
    sigset_t mask;
    char     *cgiPath;
    int      fdin, fdout, pid, sig;

    cgiPath = cgip->cgiPath;
    trace(5, "cgi: run %s", cgiPath);

    /*
        websTempFile already created both files, private and empty, so these open an existing file.
        O_NOFOLLOW keeps that true if the path is replaced between the create and here.
     */
    if ((fdin = open(cgip->stdIn, O_RDWR | O_BINARY | O_NOFOLLOW, 0600)) < 0) {
        error("Cannot open CGI stdin: %s", cgiPath);
        return -1;
    }
    if ((fdout = open(cgip->stdOut, O_RDWR | O_TRUNC | O_BINARY | O_NOFOLLOW, 0600)) < 0) {
        error("Cannot open CGI stdout: %s", cgiPath);
        close(fdin);
        return -1;
    }

    pid = fork();
    if (pid == 0) {
        /*
            Set the CGI process group in both parent and child before either can signal it.
         */
        setpgid(0, 0);
        /*
            Reset ignored signals and the mask, which exec preserves.
         */
        for (sig = 1; sig < NSIG; sig++) {
            signal(sig, SIG_DFL);
        }
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);
        /*
            Move fdout away from descriptor 0 before replacing stdin.
         */
        if (fdout == 0) {
            fdout = fcntl(fdout, F_DUPFD, 3);
        }
        /*
            Emit an error Status so startup failures cannot become successful CGI responses.
         */
        if (dup2(fdin, 0) < 0) {
            childError("Status: 500 Internal Server Error\ncontent-type: text/html\n\nDup of stdin failed\n");
            _exit(1);

        } else if (dup2(fdout, 1) < 0) {
            childError("Status: 500 Internal Server Error\ncontent-type: text/html\n\nDup of stdout failed\n");
            _exit(1);
        }
        /*
            The directory is the child's own. The server's working directory never changes.
         */
        if (chdir(dir) < 0) {
            childError("Status: 500 Internal Server Error\ncontent-type: text/html\n\n"
                       "Cannot change working directory\n");
            _exit(127);
        }
        closeInheritedFiles();
        execve(cgiPath, cgip->argp, cgip->envp);
        childError("Status: 502 Bad Gateway\ncontent-type: text/html\n\nExecution of cgi process failed\n");
        _exit(0);
    }
    /*
        Parent
     */
    if (pid > 0) {
        setpgid(pid, pid);
    }
    if (fdout >= 0) {
        close(fdout);
    }
    if (fdin >= 0) {
        close(fdin);
    }
    return pid;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(Cgi *cgip)
{
    CgiPid handle;
    int    pid;

    handle = cgip->handle;
    /*
        Treat ECHILD as termination; an embedder may have reaped the child already.
     */
    if ((pid = waitpid((CgiPid) handle, NULL, WNOHANG)) == handle) {
        trace(5, "cgi: waited for pid %d", pid);
        return 0;
    } else if (pid < 0 && errno == ECHILD) {
        trace(5, "cgi: pid %d already reaped", (int) handle);
        return 0;
    } else {
        return 1;
    }
}


/*
    SIGKILL the CGI group before reaping so an ignored signal cannot block the event loop.
    The unreaped leader prevents group-ID reuse.
 */
static void killCgi(Cgi *cgip)
{
    CgiPid handle;

    handle = cgip->handle;
    if (handle > 0) {
        trace(5, "cgi: kill orphaned pid %d", (int) handle);
        if (kill(-handle, SIGKILL) < 0) {
            kill(handle, SIGKILL);
        }
        waitpid(handle, NULL, 0);
    }
}
#endif /* LINUX || LYNX || MACOSX || QNX4 */


#if VXWORKS
#if _WRS_VXWORKS_MAJOR < 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 9)
static int findVxSym(SYMTAB_ID sid, char *name, char **pvalue)
{
    SYM_TYPE type;

    return symFindByName(sid, name, pvalue, &type);
}
#else

static int findVxSym(SYMTAB_ID sid, char *name, char **pvalue)
{
    SYMBOL_DESC symDesc;

    memset(&symDesc, 0, sizeof(SYMBOL_DESC));
    symDesc.mask = SYM_FIND_BY_NAME;
    symDesc.name = name;

    if (symFind(sid, &symDesc) == ERROR) {
        return ERROR;
    }
    if (pvalue != NULL) {
        *pvalue = (char*) symDesc.value;
    }
    return OK;
}
#endif

/*
    Keep modules loaded while tasks execute them; reload after the last task exits.
 */
typedef struct CgiModule {
    char *path;
    MODULE_ID id;
    int tasks;
} CgiModule;

static CgiModule **cgiModules;
static int       cgiModuleMax;
static int       cgiTaskSeq;

typedef int (*CgiEntry)(int argc, char **argv);

static void vxWebsCgiEntry(CgiEntry entryAddr, char **argp, char **envp, int fdin, int fdout);

/*
    taskSpawn takes its task arguments as _Vx_usr_arg_t, which holds a pointer on 64-bit VxWorks. Releases before 6.9
    take int and are 32-bit only.
 */
#if _WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 9)
    #define VX_ARG(x) ((_Vx_usr_arg_t) (x))
#else
    #define VX_ARG(x) ((int) (x))
#endif

static char *findVxEntry(char *pEntry)
{
    char *entryAddr, *pname;

    entryAddr = 0;
    if (findVxSym(sysSymTbl, pEntry, &entryAddr) == ERROR) {
        pname = sfmt("_%s", pEntry);
        findVxSym(sysSymTbl, pname, &entryAddr);
        wfree(pname);
    }
    return entryAddr;
}


static CgiModule *findVxModule(cchar *path)
{
    int mid;

    for (mid = 0; mid < cgiModuleMax; mid++) {
        if (cgiModules[mid] && smatch(cgiModules[mid]->path, path)) {
            return cgiModules[mid];
        }
    }
    return NULL;
}


static CgiModule *loadVxModule(cchar *path)
{
    CgiModule *mp;
    MODULE_ID id;
    int       fd, mid;

    if ((fd = open(path, O_RDONLY | O_BINARY, 0666)) < 0) {
        return NULL;
    }
    id = loadModule(fd, LOAD_GLOBAL_SYMBOLS);
    close(fd);
    if (id == NULL) {
        error("Cannot load the CGI module %s", path);
        return NULL;
    }
    mid = wallocObject(&cgiModules, &cgiModuleMax, sizeof(CgiModule));
    mp = cgiModules[mid];
    mp->path = sclone(path);
    mp->id = id;
    return mp;
}


static void unloadVxModule(CgiModule *mp)
{
    int mid;

    unldByModuleId(mp->id, 0);
    for (mid = 0; mid < cgiModuleMax; mid++) {
        if (cgiModules[mid] == mp) {
            cgiModuleMax = wfreeHandle(&cgiModules, mid);
            break;
        }
    }
    wfreeset(mp->path);
    wfree(mp);
}


/*
    Run basename_cgientry as a VxWorks task. The server owns stdio because taskDelete does not close it.
 */
static CgiPid launchCgi(Cgi *cgip, char *dir)
{
    CgiModule *mp;
    CgiPid    rc;
    char      *p, *basename, *pEntry, *entryAddr;
    int       priority;

    /*
        VxWorks shares one working directory across all tasks; do not change it for CGI.
     */
    (void) dir;

    /*
        Determine the basename, which is without path or the extension.
     */
    if ((p = strrchr(cgip->cgiPath, '/')) != NULL) {
        p++;
    } else {
        p = cgip->cgiPath;
    }
    basename = sclone(p);
    if ((p = strrchr(basename, '.')) != NULL) {
        *p = '\0';
    }
    /*
        Run below the server, so a busy CGI cannot starve the event loop that times it out. Numerically higher is lower
        priority.
     */
    taskPriorityGet(taskIdSelf(), &priority);
    priority = ME_GOAHEAD_CGI_PRIORITY ? ME_GOAHEAD_CGI_PRIORITY : min(priority + 1, 255);
    rc = (CgiPid) - 1;

    /*
        Reuse modules with active tasks; otherwise prefer linked entry points before loading.
     */
    pEntry = sfmt("%s_cgientry", basename);
    entryAddr = 0;
    if ((mp = findVxModule(cgip->cgiPath)) == NULL && (entryAddr = findVxEntry(pEntry)) == 0) {
        mp = loadVxModule(cgip->cgiPath);
    }
    if (mp) {
        entryAddr = findVxEntry(pEntry);
    }
    if (entryAddr == 0) {
        trace(2, "Cannot find the CGI entry point");
        cgip->missing = 1;

    } else if ((cgip->fdout = open(cgip->stdOut, O_RDWR | O_NOFOLLOW, 0600)) < 0 ||
               (cgip->fdin = open(cgip->stdIn, O_RDONLY | O_NOFOLLOW, 0600)) < 0) {
        error("Cannot open CGI stdio for %s", cgip->cgiPath);

    } else {
        cgip->vxName = sfmt("cgi-%d", ++cgiTaskSeq);
        rc = taskSpawn(cgip->vxName, priority, 0, 20000, (void*) vxWebsCgiEntry, VX_ARG(entryAddr),
                       VX_ARG(cgip->argp), VX_ARG(cgip->envp), VX_ARG(cgip->fdin), VX_ARG(cgip->fdout),
                       0, 0, 0, 0, 0);
    }
    if (mp) {
        if (rc != (CgiPid) - 1) {
            mp->tasks++;
            cgip->module = mp;
        } else if (mp->tasks == 0) {
            unloadVxModule(mp);
        }
    }
    wfree(basename);
    wfree(pEntry);
    return rc;
}


/*
    Set task stdio, argc/argv, and private environment before calling the entry point.
    envLib's delete hook frees the environment if the task is aborted.
 */
static void vxWebsCgiEntry(CgiEntry entryAddr, char **argp, char **envp, int fdin, int fdout)
{
    char   **p;
    CgiPid taskId;
    int    argc;

    taskId = taskIdSelf();
    ioTaskStdSet(taskId, 0, fdin);
    ioTaskStdSet(taskId, 1, fdout);

    for (argc = 0, p = argp; p != NULL && *p != NULL; p++, argc++) { }

    if (envPrivateCreate(taskId, -1) != OK) {
        printf("content-type: text/html\n\n" "Can not create CGI environment space\n");
        exit(0);
    }
    for (p = envp; p != NULL && *p != NULL; p++) {
        putenv(*p);
    }
    entryAddr(argc, argp);

    envPrivateDestroy(taskId);
    exit(0);
}


/*
    Check both ID and unique name under the scheduler lock to exclude task-ID reuse.
 */
static bool isCgiTask(Cgi *cgip)
{
    char *name;

    return cgip->handle && cgip->vxName && taskIdVerify(cgip->handle) == OK &&
           (name = taskName(cgip->handle)) != NULL && smatch(name, cgip->vxName);
}


static int checkCgi(Cgi *cgip)
{
    bool alive;

    taskLock();
    alive = isCgiTask(cgip);
    taskUnlock();
    return alive;
}


/*
    Check and delete atomically to avoid deleting a reused task ID.
 */
static void killCgi(Cgi *cgip)
{
    taskLock();
    if (isCgiTask(cgip)) {
        taskDelete(cgip->handle);
    }
    taskUnlock();
}


/*
    Release task stdio, name, and module reference after exit or failed launch.
 */
static void releaseVxTask(Cgi *cgip)
{
    CgiModule *mp;

    if (cgip->fdin >= 0) {
        close(cgip->fdin);
        cgip->fdin = -1;
    }
    if (cgip->fdout >= 0) {
        close(cgip->fdout);
        cgip->fdout = -1;
    }
    if ((mp = cgip->module) != NULL) {
        cgip->module = NULL;
        if (--mp->tasks == 0) {
            unloadVxModule(mp);
        }
    }
    wfreeset(cgip->vxName);
}
#endif /* VXWORKS */

#if WINDOWS
/*
    Convert a table of strings into a single block of memory. The input table consists of an array of null-terminated
    strings, terminated in a null pointer.  Returns the address of a block of memory allocated using the walloc()
    function.  The returned pointer must be deleted using wfree().  Returns NULL on error.
 */
static uchar *tableToBlock(char **table)
{
    uchar  *pBlock;         /*  Allocated block */
    char   *pEntry;         /*  Pointer into block */
    size_t sizeBlock;       /*  Size of table */
    int    index;           /*  Index into string table */

    assert(table);

    /*
        Calculate the size of the data block.  Allow for final null byte.
     */
    sizeBlock = 1;
    for (index = 0; table[index]; index++) {
        sizeBlock += strlen(table[index]) + 1;
    }
    if (sizeBlock < 2) {
        sizeBlock = 2;
    }
    /*
        Allocate the data block and fill it with the strings
     */
    pBlock = walloc(sizeBlock);

    if (pBlock != NULL) {
        pEntry = (char*) pBlock;
        for (index = 0; table[index]; index++) {
            strcpy(pEntry, table[index]);
            pEntry += strlen(pEntry) + 1;
        }
        /*
            Terminate the data block with an extra null string
         */
        *pEntry = '\0';
        pBlock[sizeBlock - 1] = '\0';
    }
    return pBlock;
}


/*
    Close a Windows handle if, and only if, it is a real handle. The two "not a handle" sentinels differ
    by origin: CreateFile returns INVALID_HANDLE_VALUE ((HANDLE) -1) on failure, while a zeroed
    STARTUPINFO or PROCESS_INFORMATION field is NULL. Passing either to CloseHandle raises
    STATUS_INVALID_HANDLE under the strict-handle-check mitigation policy, which terminates the process.
 */
static void closeWinHandle(HANDLE h)
{
    if (h != NULL && h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }
}


/*
    Windows launchCgi
    Create a temporary stdout file and launch the CGI process. Returns a handle to the spawned CGI process.
 */
static CgiPid launchCgi(Cgi *cgip, char *dir)
{
    STARTUPINFOEX                newinfo;
    SECURITY_ATTRIBUTES          security;
    PROCESS_INFORMATION          procinfo;       /*  Information about created proc   */
    LPPROC_THREAD_ATTRIBUTE_LIST attrs;
    HANDLE                       inherit[3], serverErr;
    SIZE_T                       attrSize;
    DWORD                        dwCreateFlags;
    char                         *cgiPath, *cmdLine, **pArgs;
    CgiPid                       handle;
    BOOL                         bReturn, attrsReady;
    ssize                        nLen;
    int                          i;
    uchar                        *pEnvData;

    /*
        One exit for the whole function. The command line and the environment block are freed there,
        so a failure to open either stdio file cannot orphan them. Zero both structures up front:
        the exit closes their handles unconditionally and CreateProcess does not guarantee it
        populates PROCESS_INFORMATION on failure.
     */
    handle = (CgiPid) - 1;
    cgiPath = cgip->cgiPath;
    cmdLine = 0;
    pEnvData = 0;
    attrs = 0;
    attrsReady = 0;
    memset(&newinfo, 0, sizeof(newinfo));
    memset(&procinfo, 0, sizeof(procinfo));

    /*
        Replace directory delimiters with Windows-friendly delimiters
     */
    nLen = strlen(cgiPath);
    for (i = 0; i < nLen; i++) {
        if (cgiPath[i] == '/') {
            cgiPath[i] = '\\';
        }
    }
    /*
        Calculate worst-case length of the quoted command line. Each argv element
        may grow to 2 * strlen + 3 bytes (opening / closing quote plus a doubled
        backslash run that fully precedes a quote), plus one separator per gap,
        plus one byte for the null terminator. See appendWinArg() for the exact
        quoting rules.
     */
    nLen = 1; /* null terminator */
    pArgs = cgip->argp;
    while (pArgs && *pArgs) {
        nLen += (ssize) (2 * slen(*pArgs) + 3);
        if (pArgs[1]) {
            nLen += 1; /* space between args */
        }
        pArgs++;
    }

    cmdLine = walloc(sizeof(char) * nLen);
    if (buildWinCmdLine(cgip->argp, cmdLine, nLen) < 0) {
        goto done;
    }

    /*
        Create the process start-up information
     */
    newinfo.StartupInfo.cb = sizeof(newinfo);
    newinfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    newinfo.StartupInfo.lpTitle = NULL;

    /*
        Create file handles for the spawned processes stdin and stdout files
     */
    security.nLength = sizeof(SECURITY_ATTRIBUTES);
    security.lpSecurityDescriptor = NULL;
    security.bInheritHandle = TRUE;

    /*
        Stdin file should already exist.
     */
    newinfo.StartupInfo.hStdInput = CreateFile(cgip->stdIn, GENERIC_READ, FILE_SHARE_READ, &security, OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (newinfo.StartupInfo.hStdInput == INVALID_HANDLE_VALUE) {
        error("Cannot open CGI stdin file");
        goto done;
    }

    /*
        Stdout file should already exist.
     */
    newinfo.StartupInfo.hStdOutput = CreateFile(cgip->stdOut, GENERIC_READ | GENERIC_WRITE,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
                                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (newinfo.StartupInfo.hStdOutput == INVALID_HANDLE_VALUE) {
        error("Cannot open CGI stdout file");
        goto done;
    }
    SetFilePointer(newinfo.StartupInfo.hStdOutput, 0, NULL, FILE_BEGIN);
    SetEndOfFile(newinfo.StartupInfo.hStdOutput);

    /*
        Send CGI stderr to server stderr or the null device, never to the response.
     */
    serverErr = GetStdHandle(STD_ERROR_HANDLE);
    if (serverErr == NULL || serverErr == INVALID_HANDLE_VALUE ||
        !DuplicateHandle(GetCurrentProcess(), serverErr, GetCurrentProcess(), &newinfo.StartupInfo.hStdError, 0,
                         TRUE, DUPLICATE_SAME_ACCESS)) {
        newinfo.StartupInfo.hStdError = CreateFile("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (newinfo.StartupInfo.hStdError == INVALID_HANDLE_VALUE) {
            error("Cannot open CGI stderr");
            goto done;
        }
    }

    /*
        Inherit only CGI input/output handles, excluding server resources.
     */
    inherit[0] = newinfo.StartupInfo.hStdInput;
    inherit[1] = newinfo.StartupInfo.hStdOutput;
    inherit[2] = newinfo.StartupInfo.hStdError;
    attrSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrSize);
    attrs = walloc((ssize) attrSize);
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize)) {
        error("Cannot create the CGI handle list");
        goto done;
    }
    attrsReady = 1;
    if (!UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit), NULL,
                                   NULL)) {
        error("Cannot limit the handles the CGI program inherits");
        goto done;
    }
    newinfo.lpAttributeList = attrs;

    /*
        Redirected stdio needs no console; per-request consoles consume desktop heap. Start suspended so the
        process joins its job before it can start anything.
     */
    dwCreateFlags = CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED;
    pEnvData = tableToBlock(cgip->envp);

    /*
        CreateProcess returns errors sometimes, even when the process was started correctly.  The cause is not evident.
        Detect an error by checking the value of procinfo.hProcess after the call.
     */
    /*
        lpApplicationName is set explicitly to the resolved cgiPath so program identity
        does not depend on how lpCommandLine tokenises. Combined with the extension gate
        (only .exe admitted on Windows), this guarantees no implicit cmd.exe routing.
     */
    bReturn = CreateProcess(
        cgiPath,            /*  lpApplicationName - explicit, .exe only */
        cmdLine,            /*  lpCommandLine - CRT-correctly quoted    */
        NULL,               /*  Process security attributes             */
        NULL,               /*  Thread security attributes              */
        TRUE,
        dwCreateFlags,      /*  Creation flags                          */
        pEnvData,           /*  New environment block                   */
        dir,                /*  Current directory name                  */
        &newinfo.StartupInfo, /*  STARTUPINFOEX                         */
        &procinfo);         /*  PROCESS_INFORMATION                     */

    if (bReturn != 0) {
        /*
            A job collects the CGI and its descendants, so killCgi can end all of them. Nested jobs need
            Windows 8; on an older host that already runs the server in a job the assignment fails and the
            request fails rather than run a CGI the server cannot stop.
         */
        if ((cgip->job = CreateJobObject(NULL, NULL)) == NULL ||
            !AssignProcessToJobObject(cgip->job, procinfo.hProcess)) {
            error("Cannot place the CGI program in a job");
            TerminateProcess(procinfo.hProcess, 1);
            closeWinHandle(procinfo.hProcess);
        } else {
            ResumeThread(procinfo.hThread);
            handle = procinfo.hProcess;
        }
    }

done:
    /*
        hProcess is deliberately not closed: it is the return value and the caller owns it.
     */
    closeWinHandle(procinfo.hThread);
    closeWinHandle(newinfo.StartupInfo.hStdInput);
    closeWinHandle(newinfo.StartupInfo.hStdOutput);
    closeWinHandle(newinfo.StartupInfo.hStdError);
    if (attrsReady) {
        DeleteProcThreadAttributeList(attrs);
    }
    wfree(attrs);
    wfree(pEnvData);
    wfree(cmdLine);
    return handle;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(Cgi *cgip)
{
    DWORD exitCode;
    int   nReturn;

    nReturn = GetExitCodeProcess((HANDLE) cgip->handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only
        when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE) cgip->handle);
        return 0;
    }
    return 1;
}


/*
    Terminate an orphaned CGI and everything it started, then wait for the CGI to exit so the staging
    files it holds open can be removed. TerminateProcess and TerminateJobObject only start the termination.
 */
static void killCgi(Cgi *cgip)
{
    if (cgip->handle != NULL && cgip->handle != INVALID_HANDLE_VALUE) {
        if (cgip->job == NULL || !TerminateJobObject(cgip->job, 0)) {
            TerminateProcess((HANDLE) cgip->handle, 0);
        }
        if (WaitForSingleObject((HANDLE) cgip->handle, CGI_KILL_WAIT) != WAIT_OBJECT_0) {
            error("CGI program did not exit after termination");
        }
        closeWinHandle((HANDLE) cgip->handle);
    }
}
#endif /* WIN */
#endif /* ME_GOAHEAD_CGI */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
