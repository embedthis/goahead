/*
    osdep.c -- O/S dependent code

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/

#if ME_WIN_LIKE
static HINSTANCE appInstance;
PUBLIC void syslog(int priority, char *format, ...);
#endif

/*
    The resolved staging directory, computed on first use
 */
static char *tempDir;

/************************************* Code ***********************************/

PUBLIC int websOsOpen(void)
{
#if ME_UNIX_LIKE
    int fd;

    /*
        Reserve closed descriptors 0-2 with inheritable /dev/null so logs cannot reach sockets and CGI inherits safe
        stdio.
     */
    while ((fd = open("/dev/null", O_RDWR)) >= 0 && fd <= 2) { }
    if (fd > 2) {
        close(fd);
    }
#endif
#if SOLARIS
    openlog(ME_NAME, LOG_LOCAL0);
#elif ME_UNIX_LIKE
    openlog(ME_NAME, 0, LOG_LOCAL0);
#endif
#if WINDOWS || VXWORKS || TIDSP
    rand();
#else
    random();
#endif
    return 0;
}


/*
    Restrict DLL searches to system and application directories.
 */
PUBLIC void websHardenProcess(void)
{
#if ME_WIN_LIKE && !WINCE
    typedef BOOL (WINAPI *SetDefaultDllDirectoriesProc)(DWORD flags);
    SetDefaultDllDirectoriesProc setDefaultDllDirectories;
    HMODULE                      kernel;

    #ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
        #define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x200
    #endif
    #ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
        #define LOAD_LIBRARY_SEARCH_SYSTEM32        0x800
    #endif

    /* An empty directory removes the current directory from the legacy search order */
    if (!SetDllDirectoryA("")) {
        error("Cannot remove the current directory from the DLL search path");
    }
    /*
        Resolve at runtime for older headers and Windows 7 systems without KB2533623.
     */
    kernel = GetModuleHandleA("kernel32.dll");
    setDefaultDllDirectories = kernel ?
                               (SetDefaultDllDirectoriesProc) (void*) GetProcAddress(kernel,
                                                                                     "SetDefaultDllDirectories") : NULL;
    if (setDefaultDllDirectories == NULL) {
        error("Cannot restrict the DLL search path: SetDefaultDllDirectories is not available");
    } else if (!setDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_APPLICATION_DIR)) {
        error("Cannot restrict the DLL search path");
    }
#endif
}


PUBLIC void websOsClose(void)
{
    wfreeset(tempDir);
#if ME_UNIX_LIKE
    closelog();
#endif
}


PUBLIC cchar *websGetTempDir(void)
{
    cchar *dir;

#if ME_WIN_LIKE && !WINCE
    char  buf[MAX_PATH + 1];
    DWORD len;
#endif

    if (!tempDir) {
        dir = ME_GOAHEAD_TEMP_DIR;
#if ME_WIN_LIKE && !WINCE
        if (*dir == '\0') {
            len = GetTempPathA((DWORD) sizeof(buf), buf);
            if (len > 0 && len < sizeof(buf)) {
                dir = buf;
            }
        }
#endif
        tempDir = websAbsolutePath(dir);
    }
    return tempDir;
}


/*
    Create a temporary file and return its allocated path name.

    The file is created here, empty and private, before the name is returned. The name carries 64 bits
    from the system CSPRNG so a client cannot predict where another request's data is staged, and the
    create is exclusive and refuses a symlink so a local attacker can neither pre-create the path nor
    aim it at a file the server may not write. The caller opens the returned path, and must unlink the
    file and free the name when done.

    Returns NULL if no temporary file could be created. Callers must fail the request rather than
    proceed with an unnamed staging file.
 */
PUBLIC char *websTempFile(cchar *dir, cchar *prefix)
{
    cchar *hexDigits = "0123456789abcdef";
    uchar randomBytes[8];
    char  name[sizeof(randomBytes) * 2 + 1], *path;
    char  sep;
    int   fd, i, tries;

#if WINCE
    sep = '\\';
#elif ME_WIN_LIKE
    sep = '\\';
#else
    sep = '/';
#endif
    /*
        Never fall back to the working directory; it may expose staged request data.
     */
    if (!dir || *dir == '\0') {
        dir = websGetTempDir();
    }
    if (!prefix) {
        prefix = "tmp";
    }
    for (tries = 0; tries < 8; tries++) {
        if (websGetRandomBytes((char*) randomBytes, sizeof(randomBytes), 0) < 0) {
            error("Cannot read random bytes for a temp file name");
            return 0;
        }
        for (i = 0; i < (int) sizeof(randomBytes); i++) {
            name[i * 2] = hexDigits[randomBytes[i] >> 4];
            name[i * 2 + 1] = hexDigits[randomBytes[i] & 0xf];
        }
        name[sizeof(name) - 1] = '\0';
        path = sfmt("%s%c%s-%s.tmp", dir, sep, prefix, name);
        if ((fd = open(path, O_CREAT | O_EXCL | O_RDWR | O_BINARY | O_NOFOLLOW, 0600)) >= 0) {
            close(fd);
            return path;
        }
        wfree(path);
        if (errno != EEXIST) {
            /*
                A missing or unwritable temp directory. Retrying cannot help and would only spin.
             */
            break;
        }
    }
    error("Cannot create a temp file in \"%s\". Set ME_GOAHEAD_TEMP_DIR to a writable directory "
          "outside the documents directory.", dir);
    return 0;
}


#if VXWORKS
/*
    Get absolute path.  In VxWorks, functions like chdir, ioctl for mkdir and ioctl for rmdir, require an absolute path.
    This function will take the path argument and convert it to an absolute path.  It is the caller's responsibility to
    deallocate the returned string.
 */
static char *getAbsolutePath(char *path)
{
#if _WRS_VXWORKS_MAJOR >= 6
    const char *tail;
#else
    char *tail;
#endif
    char cwd[ME_GOAHEAD_LIMIT_FILENAME];

    /*
        Determine if path is relative or absolute.  If relative, prepend the current working directory to the name.
        Otherwise, use it.  Note the getcwd call below must not be getcwd or else we go into an infinite loop
     */
    if (iosDevFind(path, &tail) != NULL && path != tail) {
        return sclone(path);
    }
#if ME_ROM
    cwd[0] = '\0';
#else
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        cwd[0] = '\0';
    }
#endif
    return sfmt("%s/%s", cwd, path);
}


PUBLIC int vxchdir(char *dirname)
{
    char *path;
    int  rc;

    path = getAbsolutePath(dirname);
    #undef chdir
    rc = chdir(path);
    wfree(path);
    return rc;
}
#endif


#if ECOS
PUBLIC int send(int s, const void *buf, size_t len, int flags)
{
    return write(s, buf, len);
}


PUBLIC int recv(int s, void *buf, size_t len, int flags)
{
    return read(s, buf, len);
}
#endif


#if ME_WIN_LIKE
PUBLIC void websSetInst(HINSTANCE inst)
{
    appInstance = inst;
}


HINSTANCE websGetInst()
{
    return appInstance;
}


PUBLIC void syslog(int priority, char *format, ...)
{
    va_list    args;
    HKEY       hkey;
    void       *event;
    long       errorType;
    ulong      exists;
    char       *buf, logName[ME_GOAHEAD_LIMIT_STRING], *lines[9], *cp, *value;
    int        type;
    static int once = 0;

    va_start(args, format);
    buf = sfmtv(format, args);
    va_end(args);

    for (cp = &buf[slen(buf)]; cp > buf && cp[-1] == '\n'; cp--) {
        cp[-1] = '\0';
    }
    type = EVENTLOG_ERROR_TYPE;
    lines[0] = buf;
    lines[1] = 0;
    lines[2] = lines[3] = lines[4] = lines[5] = 0;
    lines[6] = lines[7] = lines[8] = 0;

    if (once == 0) {
        /*  Initialize the registry */
        once = 1;
        hkey = 0;
        fmt(logName, sizeof(logName), "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", ME_NAME);

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, logName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &exists) ==
            ERROR_SUCCESS) {
            value = "%SystemRoot%\\System32\\netmsg.dll";
            if (RegSetValueEx(hkey, "EventMessageFile", 0, REG_EXPAND_SZ,
                              (uchar*) value, (int) slen(value) + 1) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                wfree(buf);
                return;
            }
            errorType = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
            if (RegSetValueEx(hkey, "TypesSupported", 0, REG_DWORD, (uchar*) &errorType, sizeof(DWORD)) !=
                ERROR_SUCCESS) {
                RegCloseKey(hkey);
                wfree(buf);
                return;
            }
            RegCloseKey(hkey);
        }
    }
    event = RegisterEventSource(0, ME_NAME);
    if (event) {
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, 3299, NULL, sizeof(lines) / sizeof(char*), 0, (LPCSTR*) lines, 0);
        DeregisterEventSource(event);
    }
    wfree(buf);
}


PUBLIC void sleep(int secs)
{
    Sleep(secs / 1000);
}
#endif

/*
    "basename" returns a pointer to the last component of a pathname LINUX, LynxOS and Mac OS X have their own basename
 */
#if !ME_UNIX_LIKE
PUBLIC char *basename(char *name)
{
    char *cp;

#if ME_WIN_LIKE
    if (((cp = strrchr(name, '\\')) == NULL) && ((cp = strrchr(name, '/')) == NULL)) {
        return name;
#else
    if ((cp = strrchr(name, '/')) == NULL) {
        return name;
#endif
    } else if (*(cp + 1) == '\0' && cp == name) {
        return name;
    } else if (*(cp + 1) == '\0' && cp != name) {
        return "";
    } else {
        return ++cp;
    }
}
#endif


#if TIDSP
static char _inet_result[16];

char *inet_ntoa(struct in_addr addr)
{
    uchar *bytes;

    bytes = (uchar*) &addr;
    snprintf(_inet_result, sizeof(_inet_result), "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    return _inet_result;
}


struct hostent * gethostbyname(char *name)
{
    static char buffer[ME_MAX_PATH];

    if (!DNSGetHostByName(name, buffer, ME_MAX_PATH)) {
        return 0;
    }
    return (struct hostent*) buffer;
}


int gethostname(char *host, int bufSize)
{
    return DNSGetHostname(host, bufSize);
}


int closesocket(SOCKET s)
{
    return fdClose(s);
}


int select(int maxfds, fd_set *readFds, fd_set *writeFds, fd_set *exceptFds, struct timeval *timeVal)
{
    return fdSelect(maxfds, readFds, writeFds, exceptFds, timeVal);
}

#endif /* TIDSP */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
