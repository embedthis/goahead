/*
    fs.c -- File System support and support for ROM-based file systems.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/******************************** Local Data **********************************/

#if ME_ROM
/*
    Symbol table for web pages and files
 */
static WebsHash romFs;
static WebsRomIndex *lookup(WebsHash fs, char *path);
#endif

#if !ME_ROM && ME_WIN_LIKE
/*
    Windows resolves these reserved device names in every directory, with or without an extension, so
    a request for /CON or /docs/com1.txt opens a device instead of a file. Reading a console or serial
    device blocks, and GoAhead runs a single event loop, so the whole server stalls. The names must be
    refused before the open because the open itself is what blocks -- there is no descriptor to test.
 */
static cchar *reservedDevices[] = {
    "con", "prn", "aux", "nul", "conin$", "conout$",
    "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
    "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
    0
};

static bool isReservedDevice(cchar *path);
#endif

/*********************************** Code *************************************/

#if !ME_ROM && ME_WIN_LIKE
/*
    Return true if any segment of the path names a reserved DOS device. Windows resolves a device from
    the segment stem -- the text before the first '.' -- and ignores trailing spaces and dots, so
    "com1", "com1.txt" and "com1 ." all reach the same device.
 */
static bool isReservedDevice(cchar *path)
{
    cchar *start, *cp;
    char  stem[ME_GOAHEAD_LIMIT_FILENAME];
    ssize len;
    int   i;

    for (start = path; start && *start; start = cp) {
        for (cp = start; *cp && *cp != '/' && *cp != '\\'; cp++) { }
        for (len = 0; &start[len] < cp && start[len] != '.'; len++) { }
        while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '.')) {
            len--;
        }
        if (0 < len && len < (ssize) sizeof(stem)) {
            memcpy(stem, start, len);
            stem[len] = '\0';
            for (i = 0; reservedDevices[i]; i++) {
                if (scaselessmatch(stem, reservedDevices[i])) {
                    return 1;
                }
            }
        }
        if (*cp) {
            cp++;
        }
    }
    return 0;
}
#endif


PUBLIC int websFsOpen(void)
{
#if ME_ROM
    WebsRomIndex *wip;
    char         name[ME_GOAHEAD_LIMIT_FILENAME];
    ssize        len;

    romFs = hashCreate(WEBS_HASH_INIT);
    for (wip = websRomIndex; wip->path; wip++) {
        strncpy(name, wip->path, ME_GOAHEAD_LIMIT_FILENAME - 1);
        name[ME_GOAHEAD_LIMIT_FILENAME - 1] = '\0';
        len = strlen(name) - 1;
        if (len > 0 && (name[len] == '/' || name[len] == '\\')) {
            name[len] = '\0';
        }
        hashEnter(romFs, name, valueSymbol(wip), 0);
    }
#endif
    return 0;
}


PUBLIC void websFsClose(void)
{
#if ME_ROM
    hashFree(romFs);
#endif
}


PUBLIC int websOpenFile(cchar *path, int flags, int mode)
{
#if ME_ROM
    WebsRomIndex *wip;

    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
    wip->pos = 0;
    return (int) (wip - websRomIndex);
#else
    WebsStat sbuf;
    int      fd;

#if ME_WIN_LIKE
    if (isReservedDevice(path)) {
        errno = ENOENT;
        return -1;
    }
#else
    /*
        O_NONBLOCK makes open() on a FIFO or device return immediately instead of blocking in the
        kernel. GoAhead has one event loop, so a blocking open stalls every connection and the timeout
        machinery with it. The flag must be on the open itself -- a prior stat() tests a different
        object than the one open() resolves.
     */
    flags |= O_NONBLOCK;
#endif
    if ((fd = open(path, flags, mode)) < 0) {
        return -1;
    }
    /*
        Only regular files are servable. Test the descriptor rather than the path so the type cannot
        change between the check and the use.
     */
    if (fstat(fd, &sbuf) < 0 || !S_ISREG(sbuf.st_mode)) {
        close(fd);
        errno = ENOENT;
        return -1;
    }
#if !ME_WIN_LIKE
    /*
        Restore blocking semantics now that the type is known. Reads and writes on a regular file
        cannot block indefinitely and the transfer paths expect blocking behavior.
     */
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
#endif
    return fd;
#endif
}


PUBLIC void websCloseFile(int fd)
{
#if !ME_ROM
    if (fd >= 0) {
        close(fd);
    }
#endif
}


PUBLIC int websStatFile(cchar *path, WebsFileInfo *sbuf)
{
#if ME_ROM
    WebsRomIndex *wip;

    assert(path && *path);

    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
    memset(sbuf, 0, sizeof(WebsFileInfo));
    sbuf->size = wip->size;
#if ME_ROM_TIME
    sbuf->mtime = ME_ROM_TIME;
#else
    sbuf->mtime = 1;
#endif
    if (wip->page == NULL) {
        sbuf->isDir = 1;
    }
    return 0;
#else
    WebsStat s;
    int      rc;
#if ME_WIN_LIKE
    if (isReservedDevice(path)) {
        errno = ENOENT;
        return -1;
    }
    {
        ssize len = slen(path) - 1;
        char  *p = sclone(path);
        if (p[len] == '/') {
            p[len] = '\0';
        } else if (p[len] == '\\') {
            p[len] = '\0';
        }
        rc = stat(p, &s);
        wfree(p);
    }
#else
    rc = stat(path, &s);
#endif
    if (rc < 0) {
        return -1;
    }
    sbuf->size = (ssize) s.st_size;
    sbuf->mtime = s.st_mtime;
    sbuf->isDir = S_ISDIR(s.st_mode);
    return 0;
#endif
}


PUBLIC ssize websReadFile(int fd, char *buf, ssize size)
{
#if ME_ROM
    WebsRomIndex *wip;
    ssize        len;

    assert(buf);
    assert(fd >= 0);

    wip = &websRomIndex[fd];

    len = min(wip->size - wip->pos, size);
    memcpy(buf, &wip->page[wip->pos], len);
    wip->pos += len;
    return len;
#else
    return read(fd, buf, (uint) size);
#endif
}


PUBLIC char *websReadWholeFile(cchar *path)
{
    WebsFileInfo sbuf;
    char         *buf;
    ssize        nbytes;
    int          fd;

    if (websStatFile(path, &sbuf) < 0) {
        return 0;
    }
    buf = walloc(sbuf.size + 1);
    /*
        O_BINARY prevents Windows text-mode CRLF->LF translation, which shrinks the read below sbuf.size
        and would otherwise leave uninitialized heap between the data and the NUL terminator. The flag is
        a no-op on Unix (defined to 0 in osdep.h).
     */
    if ((fd = websOpenFile(path, O_RDONLY | O_BINARY, 0)) < 0) {
        wfree(buf);
        return 0;
    }
    nbytes = websReadFile(fd, buf, sbuf.size);
    buf[nbytes > 0 ? nbytes : 0] = '\0';
    websCloseFile(fd);
    return buf;
}


Offset websSeekFile(int fd, Offset offset, int origin)
{
#if ME_ROM
    WebsRomIndex *wip;
    Offset       pos;

    assert(origin == SEEK_SET || origin == SEEK_CUR || origin == SEEK_END);
    assert(fd >= 0);

    wip = &websRomIndex[fd];

    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    pos = offset;
    switch (origin) {
    case SEEK_CUR:
        pos = wip->pos + offset;
        break;
    case SEEK_END:
        pos = wip->size + offset;
        break;
    default:
        break;
    }
    if (pos < 0) {
        errno = EBADF;
        return -1;
    }
    return wip->pos = pos;
#else
    return lseek(fd, (long) offset, origin);
#endif
}


PUBLIC ssize websWriteFile(int fd, cchar *buf, ssize size)
{
#if ME_ROM
    error("Cannot write to a rom file system");
    return -1;
#else
    return write(fd, buf, (uint) size);
#endif
}


#if ME_ROM
static WebsRomIndex *lookup(WebsHash fs, char *path)
{
    WebsKey *sp;
    ssize   len;

    if ((sp = hashLookup(fs, path)) == NULL) {
        if (path[0] != '/') {
            path = sfmt("/%s", path);
        } else {
            path = sclone(path);
        }
        len = slen(path) - 1;
        if (path[len] == '/') {
            path[len] = '\0';
        }
        if ((sp = hashLookup(fs, path)) == NULL) {
            wfree(path);
            return 0;
        }
        wfree(path);
    }
    return (WebsRomIndex*) sp->content.value.symbol;
}
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
