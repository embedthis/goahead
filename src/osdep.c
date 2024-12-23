/*
    osdep.c -- O/S dependent code

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/

#if ME_WIN_LIKE
    static HINSTANCE appInstance;
    PUBLIC void syslog(int priority, char *fmt, ...);
#endif

/************************************* Code ***********************************/

PUBLIC int websOsOpen(void)
{
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


PUBLIC void websOsClose(void)
{
#if ME_UNIX_LIKE
    closelog();
#endif
}


PUBLIC char *websTempFile(cchar *dir, cchar *prefix)
{
    static int count = 0;
    char   sep;

    sep = '/';
    if (!dir || *dir == '\0') {
#if WINCE
        dir = "/Temp";
        sep = '\\';
#elif ME_WIN_LIKE
        dir = getenv("TEMP");
        sep = '\\';
#elif VXWORKS
        dir = ".";
#else
        dir = "/tmp";
#endif
    }
    if (!prefix) {
        prefix = "tmp";
    }
    return sfmt("%s%c%s-%d.tmp", dir, sep, prefix, count++);
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
    const char  *tail;
#else
    char        *tail;
#endif
    char  *dev;

    /*
        Determine if path is relative or absolute.  If relative, prepend the current working directory to the name.
        Otherwise, use it.  Note the getcwd call below must not be getcwd or else we go into an infinite loop
    */
    if (iosDevFind(path, &tail) != NULL && path != tail) {
        return sclone(path);
    }
    dev = walloc(ME_GOAHEAD_LIMIT_FILENAME);
#if ME_ROM
    dev[0] = '\0';
#else
    getcwd(dev, ME_GOAHEAD_LIMIT_FILENAME);
#endif
    strcat(dev, "/");
    strcat(dev, path);
    return dev;
}


PUBLIC int vxchdir(char *dirname)
{
    char  *path;
    int     rc;

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


PUBLIC void syslog(int priority, char *fmt, ...)
{
    va_list     args;
    HKEY        hkey;
    void        *event;
    long        errorType;
    ulong       exists;
    char        *buf, logName[ME_GOAHEAD_LIMIT_STRING], *lines[9], *cp, *value;
    int         type;
    static int  once = 0;

    va_start(args, fmt);
    buf = sfmtv(fmt, args);
    va_end(args);

    cp = &buf[slen(buf) - 1];
    while (*cp == '\n' && cp > buf) {
        *cp-- = '\0';
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
    char  *cp;

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
    uchar       *bytes;

    bytes = (uchar*) &addr;
    sprintf(_inet_result, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    return _inet_result;
}


struct hostent* gethostbyname(char *name)
{
    static char buffer[ME_MAX_PATH];

    if(!DNSGetHostByName(name, buffer, ME_MAX_PATH)) {
        return 0;
    }
    return (struct hostent*) buffer;
}


ulong hostGetByName(char *name)
{
    struct _hostent *ent;

    ent = gethostbyname(name);
    return ent->h_addr[0];
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
