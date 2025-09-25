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
#else
typedef pid_t CgiPid;
#endif

typedef struct Cgi {            /* Struct for CGI tasks which have completed */
    Webs *wp;                   /* Connection object */
    char *stdIn;                /* File desc. for task's temp input fd */
    char *stdOut;               /* File desc. for task's temp output fd */
    char *cgiPath;              /* Path to executable process file */
    char **argp;                /* Pointer to buf containing argv tokens */
    char **envp;                /* Pointer to array of environment strings */
    CgiPid handle;              /* Process handle of the task */
    off_t fplacemark;           /* Seek location for CGI output file */
} Cgi;

static Cgi **cgiList;           /* walloc chain list of wp's to be closed */
static int cgiMax;              /* Size of walloc list */

/*
    The real protection is via ME_GOAHEAD_CGI_VAR_PREFIX which prefixes user env vars with CGI_.
    This list is not complete and not intended to be. "Belt and suspenders".
 */
static cchar *envBlackList[] = {
    "AUTHSTATE",
    "BASHOPTS",
    "BASH_ENV",
    "CDPATH",
    "CLASSPATH",
    "ENV",
    "FPATH",
    "GLOBIGNORE",
    "GOPATH",
    "HOSTALIASES",
    "HTTP_AUTHORIZATION",
    "IFS",
    "JAVA_HOME",
    "JAVA_TOOL_OPTIONS",
    "LIBPATH",
    "LOCALDOMAIN",
    "NODE_OPTIONS",
    "NLSPATH",
    "NULLCMD",
    "PATH",
    "PATH_LOCALE",
    "PERL5DB",
    "PERL5LIB",
    "PERL5OPT",
    "PERLIO_DEBUG",
    "PERLLIB",
    "PS4",
    "PYTHONHOME",
    "PYTHONINSPECT",
    "PYTHONPATH",
    "PYTHONUSERBASE",
    "READNULLCMD",
    "REMOTE_HOST",
    "RES_OPTIONS",
    "RUBYLIB",
    "RUBYOPT",
    "SHELLOPTS",
    "SHLIB_PATH",
    "TERMCAP",
    "TERMINFO",
    "TERMINFO_DIRS",
    "TERMPATH",
    "TMPPREFIX",
    "ZDOTDIR",
    NULL
};

/************************************ Forwards ********************************/

static int checkCgi(CgiPid handle);
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut);

/************************************* Code ***********************************/
/*
    Process a form request.
    Return true to indicate the request was handled, even for errors.
 */
PUBLIC bool cgiHandler(Webs *wp)
{
    Cgi     *cgip;
    WebsKey *s;
    char    cgiPrefix[ME_GOAHEAD_LIMIT_FILENAME], *stdIn, *stdOut, cwd[ME_GOAHEAD_LIMIT_FILENAME];
    char    *cp, *cgiName, *cgiPath, **argp, **envp, **ep, *tok, *query, *dir, *extraPath, *exe, *vp;
    cchar   **bp;
    CgiPid  pHandle;
    int     n, envpsize, argpsize, cid;

    assert(websValid(wp));

    websSetEnv(wp);

    /*
        Extract the form name and then build the full path name. The form name will follow the first '/' in path.
     */
    scopy(cgiPrefix, sizeof(cgiPrefix), wp->path);
    if ((cgiName = strchr(&cgiPrefix[1], '/')) == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Missing CGI name");
        return 1;
    }
    *cgiName++ = '\0';

    getcwd(cwd, ME_GOAHEAD_LIMIT_FILENAME);
    dir = wp->route->dir ? wp->route->dir : cwd;
    chdir(dir);

    extraPath = 0;
    if ((cp = strchr(cgiName, '/')) != NULL) {
        extraPath = sclone(cp);
        *cp = '\0';
        websSetVar(wp, "PATH_INFO", extraPath);
        websSetVarFmt(wp, "PATH_TRANSLATED", "%s%s%s", dir, cgiPrefix, extraPath);
        wfree(extraPath);
    } else {
        websSetVar(wp, "PATH_INFO", "");
        websSetVar(wp, "PATH_TRANSLATED", "");
    }
    cgiPath = sfmt("%s%s/%s", dir, cgiPrefix, cgiName);
    websSetVarFmt(wp, "SCRIPT_NAME", "%s/%s", cgiPrefix, cgiName);
    websSetVar(wp, "SCRIPT_FILENAME", cgiPath);

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
        if (stat(cgiPath, &sbuf) != 0 || (sbuf.st_mode & S_IFREG) == 0) {
            exe = sfmt("%s.exe", cgiPath);
            if (stat(exe, &sbuf) == 0 && (sbuf.st_mode & S_IFREG)) {
                wfree(cgiPath);
                cgiPath = exe;
            } else {
                error("Cannot find CGI program: ", cgiPath);
                websError(wp, HTTP_CODE_NOT_FOUND | WEBS_NOLOG, "CGI program file does not exist");
                wfree(cgiPath);
                return 1;
            }
        }
#if ME_WIN_LIKE
        if (strstr(cgiPath, ".exe") == NULL && strstr(cgiPath, ".bat") == NULL)
#else
        if (access(cgiPath, X_OK) != 0)
#endif
        {
            websError(wp, HTTP_CODE_NOT_FOUND, "CGI process file is not executable");
            wfree(cgiPath);
            return 1;
        }
    }
#endif /* ! VXWORKS */
    /*
        Build command line arguments.  Only used if there is no non-encoded = character.  This is indicative of a
           ISINDEX
        query.  POST separators are & and others are +.  argp will point to a walloc'd array of pointers.  Each pointer
        will point to substring within the query string.  This array of string pointers is how the spawn or exec
           routines
        expect command line arguments to be passed.  Since we don't know ahead of time how many individual items there
           are
        in the query string, the for loop includes logic to grow the array size via wrealloc.
     */
    argpsize = 10;
    if ((argp = walloc(argpsize * sizeof(char*))) == 0) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot allocate CGI args");
        wfree(cgiPath);
        return 1;
    }
    assert(argp);
    *argp = cgiPath;
    n = 1;
    query = 0;

    if (strchr(wp->query, '=') == NULL) {
        query = sclone(wp->query);
        websDecodeUrl(query, query, strlen(query));
        for (cp = stok(query, " ", &tok); cp != NULL && argp != NULL; ) {
            *(argp + n) = cp;
            trace(5, "ARG[%d] %s", n, argp[n - 1]);
            n++;
            if (n >= argpsize) {
                argpsize *= 2;
                if (argpsize > ME_GOAHEAD_LIMIT_CGI_ARGS) {
                    websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Too many arguments");
                    wfree(cgiPath);
                    return 1;
                }
                argp = wrealloc(argp, argpsize * sizeof(char*));
            }
            cp = stok(NULL, " ", &tok);
        }
    }
    *(argp + n) = NULL;

    /*
        Add all CGI variables to the environment strings to be passed to the spawned CGI process.
        This includes a few we don't already have in the symbol table, plus all those that are in
        the vars symbol table. envp will point to a walloc'd array of pointers. Each pointer will
        point to a walloc'd string containing the keyword value pair in the form keyword=value.
        Since we don't know ahead of time how many environment strings there will be the for
        loop includes logic to grow the array size via wrealloc.
     */
    envpsize = 64;
    envp = walloc(envpsize * sizeof(char*));
    if (wp->vars) {
        for (n = 0, s = hashFirst(wp->vars); s != NULL; s = hashNext(wp->vars, s)) {
            if (s->content.valid && s->content.type == string) {
                vp = strim(s->name.value.string, " \t\r\n", WEBS_TRIM_BOTH);
                for (bp = envBlackList; *bp; bp++) {
                    if (smatch(vp, *bp)) {
                        continue;
                    }
                }
                if (sstarts(vp,
                            "LD_") ||
                    sstarts(vp, "LDR_") || sstarts(vp, "_RLD") || sstarts(vp, "DYLD_") || strstr(vp, "=()")) {
                    continue;
                }
                if (s->arg != 0 && *ME_GOAHEAD_CGI_VAR_PREFIX != '\0') {
                    envp[n++] = sfmt("%s%s=%s", ME_GOAHEAD_CGI_VAR_PREFIX, s->name.value.string,
                                     s->content.value.string);
                } else {
                    envp[n++] = sfmt("%s=%s", s->name.value.string, s->content.value.string);
                }
                trace(0, "Env[%d] %s", n, envp[n - 1]);
                if (n >= envpsize) {
                    envpsize *= 2;
                    envp = wrealloc(envp, envpsize * sizeof(char*));
                }
            }
        }
    }
    *(envp + n) = NULL;

    /*
        Create temporary file name(s) for the child's stdin and stdout. For POST data the stdin temp file (and name)
        should already exist.
     */
    if (wp->cgiStdin == NULL) {
        wp->cgiStdin = websGetCgiCommName();
    }
    stdIn = wp->cgiStdin;
    stdOut = websGetCgiCommName();
    if (wp->cgifd >= 0) {
        close(wp->cgifd);
        wp->cgifd = -1;
    }

    /*
        Now launch the process.  If not successful, do the cleanup of resources.  If successful, the cleanup will be
        done after the process completes.
     */
    if ((pHandle = launchCgi(cgiPath, argp, envp, stdIn, stdOut)) == (CgiPid) - 1) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "failed to spawn CGI task");
        for (ep = envp; *ep != NULL; ep++) {
            wfree(*ep);
        }
        wfree(cgiPath);
        wfree(argp);
        wfree(envp);
        wfree(stdOut);
        wfree(query);

    } else {
        /*
            If the spawn was successful, put this wp on a queue to be checked for completion.
         */
        cid = wallocObject(&cgiList, &cgiMax, sizeof(Cgi));
        cgip = cgiList[cid];
        cgip->handle = pHandle;
        cgip->stdIn = stdIn;
        cgip->stdOut = stdOut;
        cgip->cgiPath = cgiPath;
        cgip->argp = argp;
        cgip->envp = envp;
        cgip->wp = wp;
        cgip->fplacemark = 0;
        wfree(query);
    }
    /*
        Restore the current working directory after spawning child CGI
     */
    chdir(cwd);
    return 1;
}


PUBLIC int websCgiOpen(void)
{
    websDefineHandler("cgi", 0, cgiHandler, 0, 0);
    return 0;
}


PUBLIC bool websProcessCgiData(Webs *wp)
{
    ssize nbytes;

    nbytes = bufLen(&wp->input);
    trace(5, "cgi: write %d bytes to CGI program", nbytes);
    if (write(wp->cgifd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot write to CGI gateway");
    } else {
        trace(5, "cgi: write %d bytes to CGI program", nbytes);
    }
    websConsumeInput(wp, nbytes);
    return 1;
}


static void writeCgiHeaders(Webs *wp, int status, ssize contentLength, char *location, char *contentType)
{
    trace(5, "cgi: Start response headers");
    websSetStatus(wp, status);
    websWriteHeaders(wp, contentLength, location);
    websWriteHeader(wp, "Pragma", "no-cache");
    websWriteHeader(wp, "Cache-Control", "no-cache");
    if (contentType) {
        websWriteHeader(wp, "Content-Type", contentType);
    }
}


static ssize parseCgiHeaders(Webs *wp, char *buf)
{
    char  *end, *cp, *key, *value, *location, *contentType;
    ssize len, contentLength;
    int   status, doneHeaders;

    status = HTTP_CODE_OK;
    contentLength = -1;
    contentType = 0;
    location = 0;
    doneHeaders = 0;

    /*
        Look for end of headers
     */
    if ((end = strstr(buf, "\r\n\r\n")) == NULL) {
        if ((end = strstr(buf, "\n\n")) == NULL) {
            return 0;
        }
        len = 2;
    } else {
        len = 4;
    }
    *end = '\0';
    end += len;
    cp = buf;
    if (!strchr(cp, ':')) {
        /* No headers found */
        return 0;
    }
    if (strncmp(cp, "HTTP/1.", 7) == 0) {
        ssplit(cp, "\r\n", &cp);
    }
    for (; cp && *cp && (*cp != '\r' && *cp != '\n') && cp < end; ) {
        key = slower(ssplit(cp, ":", &value));
        if (strcmp(key, "location") == 0) {
            location = value;
        } else if (strcmp(key, "status") == 0) {
            status = atoi(value);
        } else if (strcmp(key, "content-type") == 0) {
            contentType = value;
        } else if (strcmp(key, "content-length") == 0) {
            contentLength = atoi(value);
        } else {
            /*
                Now pass all other headers back to the client
             */
            if (!doneHeaders) {
                writeCgiHeaders(wp, status, contentLength, location, contentType);
                doneHeaders = 1;
            }
            if (key && value && !strspn(key, "%<>/\\")) {
                websWriteHeader(wp, key, "%s", value);
            } else {
                trace(5, "cgi: bad response http header: \"%s\": \"%s\"", key, value);
            }
        }
        stok(value, "\r\n", &cp);
    }
    if (!doneHeaders) {
        writeCgiHeaders(wp, status, contentLength, location, contentType);
    }
    websWriteEndHeaders(wp);
    return end - buf;
}


PUBLIC void websCgiGatherOutput(Cgi *cgip)
{
    Webs     *wp;
    WebsStat sbuf;
    char     buf[ME_GOAHEAD_LIMIT_HEADERS + 2];
    ssize    nbytes, skip;
    int      fdout;

    /*
        OPT - currently polling and doing a stat each poll. Also doing open/close each chunk.
        If the CGI process writes partial headers, this repeatedly reads the data until complete
        headers are written or more than ME_GOAHEAD_LIMIT_HEADERS of data is received.
     */
    if ((stat(cgip->stdOut, &sbuf) == 0) && (sbuf.st_size > cgip->fplacemark)) {
        if ((fdout = open(cgip->stdOut, O_RDONLY | O_BINARY, 0444)) >= 0) {
            /*
                Check to see if any data is available in the output file and send its contents to the socket.
                Write the HTTP header on our first pass. The header must fit into ME_GOAHEAD_LIMIT_BUFFER.
             */
            wp = cgip->wp;
            lseek(fdout, cgip->fplacemark, SEEK_SET);
            while ((nbytes = read(fdout, buf, sizeof(buf) - 1)) > 0) {
                buf[nbytes] = 0;
                skip = 0;
                if (!(wp->flags & WEBS_HEADERS_CREATED)) {
                    if ((skip = parseCgiHeaders(wp, buf)) == 0) {
                        if (cgip->handle && sbuf.st_size < ME_GOAHEAD_LIMIT_HEADERS) {
                            trace(5, "cgi: waiting for http headers");
                            break;
                        } else {
                            trace(5, "cgi: missing http headers - create default headers");
                            writeCgiHeaders(wp, HTTP_CODE_OK, -1, 0, 0);
                        }
                    }
                }
                trace(5, "cgi: write %d bytes to client", nbytes - skip);
                websWriteBlock(wp, &buf[skip], nbytes - skip);
                cgip->fplacemark += (off_t) nbytes;
            }
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
    char **ep;
    int  cid;

    for (cid = 0; cid < cgiMax; cid++) {
        if ((cgip = cgiList[cid]) != NULL) {
            wp = cgip->wp;
            websCgiGatherOutput(cgip);
            if (checkCgi(cgip->handle) == 0) {
                /*
                    We get here if the CGI process has terminated. Clean up.
                 */
                cgip->handle = 0;
                websCgiGatherOutput(cgip);

#if WINDOWS
                /*
                     Windows can have delayed notification through the file system after process exit.
                 */
                {
                    int nTries;
                    for (nTries = 0; (cgip->fplacemark == 0) && (nTries < 100); nTries++) {
                        websCgiGatherOutput(cgip);
                        if (cgip->fplacemark == 0) {
                            Sleep(10);
                        }
                    }
                }
#endif
                if (cgip->fplacemark == 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "CGI generated no output");
                } else {
                    trace(5, "cgi: Request complete - calling websDone");
                    websDone(wp);
                }
                /*
                    Remove the temporary re-direction files
                 */
                unlink(cgip->stdIn);
                unlink(cgip->stdOut);
                /*
                    Free all the memory buffers pointed to by cgip. The stdin file name (wp->cgiStdin) gets freed as
                    part of websFree().
                 */
                cgiMax = wfreeHandle(&cgiList, cid);
                for (ep = cgip->envp; ep != NULL && *ep != NULL; ep++) {
                    wfree(*ep);
                }
                wfree(cgip->cgiPath);
                wfree(cgip->stdOut);
                wfree(cgip->argp);
                wfree(cgip->envp);
                wfree(cgip);
                websPump(wp);
                if ((wp->flags & WEBS_KEEP_ALIVE) == 0) {
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


#if WINCE
/*
     Launch the CGI process and return a handle to it.  CE note: This function is not complete.  The missing piece is
     the ability to redirect stdout.
 */
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    PROCESS_INFORMATION procinfo;       /*  Information about created proc   */
    DWORD               dwCreateFlags;
    char                *fulldir;
    BOOL                bReturn;
    int                 i, nLen;

    /*
        Replace directory delimiters with Windows-friendly delimiters
     */
    nLen = strlen(cgiPath);
    for (i = 0; i < nLen; i++) {
        if (cgiPath[i] == '/') {
            cgiPath[i] = '\\';
        }
    }
    fulldir = NULL;
    dwCreateFlags = CREATE_NEW_CONSOLE;

    /*
         CreateProcess returns errors sometimes, even when the process was started correctly.  The cause is not evident.
         For now: we detect an error by checking the value of procinfo.hProcess after the call.
     */
    procinfo.hThread = NULL;
    bReturn = CreateProcess(
        cgiPath,            /*  Name of executable module        */
        NULL,               /*  Command line string              */
        NULL,               /*  Process security attributes      */
        NULL,               /*  Thread security attributes       */
        0,                  /*  Handle inheritance flag          */
        dwCreateFlags,      /*  Creation flags                   */
        NULL,               /*  New environment block            */
        NULL,               /*  Current directory name           */
        NULL,               /*  STARTUPINFO                      */
        &procinfo);         /*  PROCESS_INFORMATION              */
    if (bReturn == 0) {
        return -1;
    } else {
        CloseHandle(procinfo.hThread);
    }
    return (int) procinfo.dwProcessId;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(CgiPid handle)
{
    int   nReturn;
    DWORD exitCode;

    nReturn = GetExitCodeProcess((HANDLE) handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE) handle);
        return 0;
    }
    return 1;
}
#endif /* WINCE */


#if ME_UNIX_LIKE || QNX
/*
    Launch the CGI process and return a handle to it.
 */
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    int fdin, fdout, pid;

    trace(5, "cgi: run %s", cgiPath);

    if ((fdin = open(stdIn, O_RDWR | O_CREAT | O_BINARY, 0666)) < 0) {
        error("Cannot open CGI stdin: ", cgiPath);
        return -1;
    }
    if ((fdout = open(stdOut, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666)) < 0) {
        error("Cannot open CGI stdout: ", cgiPath);
        return -1;
    }

    pid = fork();
    if (pid == 0) {
        /*
            Child
         */
        if (dup2(fdin, 0) < 0) {
            printf("content-type: text/html\n\nDup of stdin failed\n");
            _exit(1);

        } else if (dup2(fdout, 1) < 0) {
            printf("content-type: text/html\n\nDup of stdout failed\n");
            _exit(1);

        } else if (execve(cgiPath, argp, envp) == -1) {
            printf("content-type: text/html\n\nExecution of cgi process failed\n");
        }
        _exit(0);
    }
    /*
        Parent
     */
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
static int checkCgi(CgiPid handle)
{
    int pid;

    /*
        Check to see if the CGI child process has terminated or not yet.
     */
    if ((pid = waitpid((CgiPid) handle, NULL, WNOHANG)) == handle) {
        trace(5, "cgi: waited for pid %d", pid);
        return 0;
    } else {
        return 1;
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

static void vxWebsCgiEntry(void*entryAddr(int argc, char **argv), char **argv, char **envp, char *stdIn, char *stdOut);
/*
    Launch the CGI process and return a handle to it. Process spawning is not supported in VxWorks.  Instead, we spawn a
    "task".  A major difference is that we have to know the entry point for the taskSpawn API.  Also the module may have
    to be loaded before being executed; it may also be part of the OS image, in which case it cannot be loaded or
    unloaded.
    The following sequence is used:
    1. If the module is already loaded, unload it from memory.
    2. Search for a query string keyword=value pair in the environment variables where the keyword is cgientry.  If
        found use its value as the the entry point name.  If there is no such pair set the entry point name to the
        default: basename_cgientry, where basename is the name of the cgi file without the extension.  Use the entry
        point name in a symbol table search for that name to use as the entry point address.  If successful go to step
           5.
    3. Try to load the module into memory.  If not successful error out.
    4. If step 3 is successful repeat the entry point search from step 2. If the entry point exists, go to step 5.  If
        it does not, error out.
    5. Use taskSpawn to start a new task which uses vxWebsCgiEntry as its starting point. The five arguments to
        vxWebsCgiEntry will be the user entry point address, argp, envp, stdIn and stdOut.  vxWebsCgiEntry will convert
        argp to an argc argv pair to pass to the user entry, it will initialize the task environment with envp, it will
        open and redirect stdin and stdout to stdIn and stdOut, and then it will call the user entry.
    6.  Return the taskSpawn return value.
 */
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    char *p, *basename, *pEntry, *pname, *entryAddr, **pp;
    int  priority, rc, fd;

    /*
        Determine the basename, which is without path or the extension.
     */
    if ((int) (p = strrchr(cgiPath, '/') + 1) == 1) {
        p = cgiPath;
    }
    basename = sclone(p);
    if ((p = strrchr(basename, '.')) != NULL) {
        *p = '\0';
    }
    /*
        Unload the module, if it is already loaded.  Get the current task priority.
     */
    unld(cgiPath, 0);
    taskPriorityGet(taskIdSelf(), &priority);
    rc = fd = -1;

    /*
         Set the entry point symbol name as described above.  Look for an already loaded entry point; if it exists,
            spawn
         the task accordingly.
     */
    for (pp = envp, pEntry = NULL; pp != NULL && *pp != NULL; pp++) {
        if (strncmp(*pp, "cgientry=", 9) == 0) {
            pEntry = sclone(*pp + 9);
            break;
        }
    }
    if (pEntry == NULL) {
        pEntry = sfmt("%s_%s", basename, "cgientry");
    }
    entryAddr = 0;
    if (findVxSym(sysSymTbl, pEntry, &entryAddr) == -1) {
        pname = sfmt("_%s", pEntry);
        findVxSym(sysSymTbl, pname, &entryAddr);
        wfree(pname);
    }
    if (entryAddr != 0) {
        rc = taskSpawn(pEntry, priority, 0, 20000, (void*) vxWebsCgiEntry, (int) entryAddr, (int) argp,
                       (int) envp, (int) stdIn, (int) stdOut, 0, 0, 0, 0, 0);
        goto done;
    }

    /*
        Try to load the module.
     */
    if ((fd = open(cgiPath, O_RDONLY | O_BINARY, 0666)) < 0 ||
        loadModule(fd, LOAD_GLOBAL_SYMBOLS) == NULL) {
        goto done;
    }
    if ((findVxSym(sysSymTbl, pEntry, &entryAddr)) == -1) {
        pname = sfmt("_%s", pEntry);
        findVxSym(sysSymTbl, pname, &entryAddr);
        wfree(pname);
    }
    if (entryAddr != 0) {
        rc = taskSpawn(pEntry, priority, 0, 20000, (void*) vxWebsCgiEntry, (int) entryAddr, (int) argp,
                       (int) envp, (int) stdIn, (int) stdOut, 0, 0, 0, 0, 0);
    }
done:
    if (fd != -1) {
        close(fd);
    }
    wfree(basename);
    wfree(pEntry);
    return rc;
}


/*
    This is the CGI process wrapper.  It will open and redirect stdin and stdout to stdIn and stdOut.  It converts argv
    to an argc, argv pair to pass to the user entry. It initializes the task environment with envp strings. Then it
    will call the user entry.
 */
static void vxWebsCgiEntry(void*entryAddr(int argc, char **argv), char **argp, char **envp, char *stdIn, char *stdOut)
{
    char **p;
    int  argc, taskId, fdin, fdout;

    /*
        Open the stdIn and stdOut files and redirect stdin and stdout to them.
     */
    taskId = taskIdSelf();
    if ((fdout = open(stdOut, O_RDWR | O_CREAT, 0666)) < 0 &&
        (fdout = creat(stdOut, O_RDWR)) < 0) {
        exit(0);
    }
    ioTaskStdSet(taskId, 1, fdout);

    if ((fdin = open(stdIn, O_RDONLY | O_CREAT, 0666)) < 0 && (fdin = creat(stdIn, O_RDWR)) < 0) {
        printf("content-type: text/html\n\n" "Can not create CGI stdin to %s\n", stdIn);
        close(fdout);
        exit(0);
    }
    ioTaskStdSet(taskId, 0, fdin);

    /*
        Count the number of entries in argv
     */
    for (argc = 0, p = argp; p != NULL && *p != NULL; p++, argc++) {
    }

    /*
        Create a private environment and copy the envp strings to it.
     */
    if (envPrivateCreate(taskId, -1) != OK) {
        printf("content-type: text/html\n\n" "Can not create CGI environment space\n");
        close(fdin);
        close(fdout);
        exit(0);
    }
    for (p = envp; p != NULL && *p != NULL; p++) {
        putenv(*p);
    }

    /*
        Call the user entry.
     */
    (*entryAddr)(argc, argp);

    /*
        The user code should return here for cleanup.
     */
    envPrivateDestroy(taskId);
    close(fdin);
    close(fdout);
    exit(0);
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(CgiPid handle)
{
    STATUS stat;

    /*
        Verify the existence of a VxWorks task
     */
    stat = taskIdVerify(handle);
    if (stat == OK) {
        return 1;
    } else {
        return 0;
    }
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
    }
    return pBlock;
}


/*
    Windows launchCgi
    Create a temporary stdout file and launch the CGI process. Returns a handle to the spawned CGI process.
 */
static CgiPid launchCgi(char *cgiPath, char **argp, char **envp, char *stdIn, char *stdOut)
{
    STARTUPINFO         newinfo;
    SECURITY_ATTRIBUTES security;
    PROCESS_INFORMATION procinfo;       /*  Information about created proc   */
    DWORD               dwCreateFlags;
    char                *cmdLine, **pArgs;
    BOOL                bReturn;
    ssize               nLen;
    int                 i;
    uchar               *pEnvData;

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
        Calculate length of command line
     */
    nLen = 0;
    pArgs = argp;
    while (pArgs && *pArgs && **pArgs) {
        nLen += strlen(*pArgs) + 1;
        pArgs++;
    }

    /*
        Construct command line (safe length calculation and single-pass write)
     */
    nLen = 1; // for null terminator
    pArgs = argp;
    while (pArgs && *pArgs && **pArgs) {
        nLen += (ssize) strlen(*pArgs);
        if (pArgs[1]) {
            nLen += 1; /* space between args */
        }
        pArgs++;
    }

    cmdLine = walloc(sizeof(char) * nLen);
    assert(cmdLine);

    {
        char *p = cmdLine;
        pArgs = argp;
        while (pArgs && *pArgs && **pArgs) {
            size_t alen = strlen(*pArgs);
            memcpy(p, *pArgs, alen);
            p += alen;
            if (pArgs[1]) {
                *p++ = ' ';
            }
            pArgs++;
        }
        *p = '\0';
    }

    /*
        Create the process start-up information
     */
    memset(&newinfo, 0, sizeof(newinfo));
    newinfo.cb = sizeof(newinfo);
    newinfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    newinfo.wShowWindow = SW_HIDE;
    newinfo.lpTitle = NULL;

    /*
        Create file handles for the spawned processes stdin and stdout files
     */
    security.nLength = sizeof(SECURITY_ATTRIBUTES);
    security.lpSecurityDescriptor = NULL;
    security.bInheritHandle = TRUE;

    /*
        Stdin file should already exist.
     */
    newinfo.hStdInput = CreateFile(stdIn, GENERIC_READ, FILE_SHARE_READ, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                   NULL);
    if (newinfo.hStdOutput == (HANDLE) -1) {
        error("Cannot open CGI stdin file");
        return (CgiPid) - 1;
    }

    /*
        Stdout file is created and file pointer is reset to start.
     */
    newinfo.hStdOutput = CreateFile(stdOut, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (newinfo.hStdOutput == (HANDLE) -1) {
        error("Cannot create CGI stdout file");
        CloseHandle(newinfo.hStdInput);
        return (CgiPid) - 1;
    }
    SetFilePointer(newinfo.hStdOutput, 0, NULL, FILE_END);
    newinfo.hStdError = newinfo.hStdOutput;

    dwCreateFlags = CREATE_NEW_CONSOLE;
    pEnvData = tableToBlock(envp);

    /*
        CreateProcess returns errors sometimes, even when the process was started correctly.  The cause is not evident.
        Detect an error by checking the value of procinfo.hProcess after the call.
     */
    procinfo.hProcess = NULL;
    bReturn = CreateProcess(
        NULL,               /*  Name of executable module        */
        cmdLine,            /*  Command line string              */
        NULL,               /*  Process security attributes      */
        NULL,               /*  Thread security attributes       */
        TRUE,               /*  Handle inheritance flag          */
        dwCreateFlags,      /*  Creation flags                   */
        pEnvData,           /*  New environment block            */
        NULL,               /*  Current directory name           */
        &newinfo,           /*  STARTUPINFO                      */
        &procinfo);         /*  PROCESS_INFORMATION              */

    if (procinfo.hThread != NULL) {
        CloseHandle(procinfo.hThread);
    }
    if (newinfo.hStdInput) {
        CloseHandle(newinfo.hStdInput);
    }
    if (newinfo.hStdOutput) {
        CloseHandle(newinfo.hStdOutput);
    }
    wfree(pEnvData);
    wfree(cmdLine);

    if (bReturn == 0) {
        return (CgiPid) - 1;
    } else {
        return procinfo.hProcess;
    }
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(CgiPid handle)
{
    DWORD exitCode;
    int   nReturn;

    nReturn = GetExitCodeProcess((HANDLE) handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only
        when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE) handle);
        return 0;
    }
    return 1;
}
#endif /* WIN */
#endif /* ME_GOAHEAD_CGI */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
