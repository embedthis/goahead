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

/*
    Each ROM open owns its read position.
 */
typedef struct RomFile {
    WebsRomIndex *wip;
    Offset pos;
} RomFile;

static RomFile **romFiles;
static int     romFileMax;

static WebsRomIndex *lookup(WebsHash fs, cchar *path);
static RomFile *getRomFile(int fd);
#endif

#if !ME_ROM && ME_WIN_LIKE
/* Refuse Windows device names before opening: opening a console or serial device can block the event loop. */
static cchar *reservedDevices[] = {
    "con", "prn", "aux", "nul", "conin$", "conout$",
    "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
    "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
    //  Superscript one to three, as UTF-8 and as the single byte of the Windows-1252 code page
    "com\xc2\xb9", "com\xc2\xb2", "com\xc2\xb3", "lpt\xc2\xb9", "lpt\xc2\xb2", "lpt\xc2\xb3",
    "com\xb9", "com\xb2", "com\xb3", "lpt\xb9", "lpt\xb2", "lpt\xb3",
    0
};
#endif

/*********************************** Code *************************************/

/*
    Match DOS device stems before dots or colons, ignoring trailing spaces and dots.
 */
PUBLIC bool websIsReservedDevice(cchar *path)
{
#if !ME_ROM && ME_WIN_LIKE
    cchar *start, *cp;
    char  stem[ME_GOAHEAD_LIMIT_FILENAME];
    ssize len;
    int   i;

    for (start = path; start && *start; start = cp) {
        for (cp = start; *cp && *cp != '/' && *cp != '\\'; cp++) { }
        for (len = 0; &start[len] < cp && start[len] != '.' && start[len] != ':'; len++) { }
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
#else
    (void) path;
#endif
    return 0;
}


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
    int          fd;

    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
    if ((fd = wallocObject(&romFiles, &romFileMax, sizeof(RomFile))) < 0) {
        return -1;
    }
    romFiles[fd]->wip = wip;
    return fd;
#else
    WebsStat sbuf;
    int      fd;

#if !ME_WIN_LIKE
    /*
        O_NONBLOCK makes open() on a FIFO or device return immediately instead of blocking in the
        kernel. GoAhead has one event loop, so a blocking open stalls every connection and the timeout
        machinery with it. The flag must be on the open itself, as a prior stat() tests a different
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
#if ME_ROM
    RomFile *rf;

    if ((rf = getRomFile(fd)) != NULL) {
        wfree(rf);
        romFileMax = wfreeHandle(&romFiles, fd);
    }
#else
    if (fd >= 0) {
        close(fd);
    }
#endif
}


PUBLIC int websStatFile(cchar *path, WebsFileInfo *sbuf)
{
#if ME_ROM
    WebsRomIndex *wip;
#else
    WebsStat s;
    int      rc;
#endif

    if (path == 0 || *path == '\0') {
        return -1;
    }
#if ME_ROM
    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
    memset(sbuf, 0, sizeof(WebsFileInfo));
    sbuf->size = wip->size;
    sbuf->mtime = wip->mtime;
    sbuf->hash = wip->hash;
    if (wip->page == NULL) {
        sbuf->isDir = 1;
    }
    return 0;
#else
#if ME_WIN_LIKE
    {
        /*
            Strip trailing separators only for directories so Windows stat agrees with open.
         */
        ssize len = slen(path) - 1;
        char  *p = sclone(path);
        bool  trailing = p[len] == '/' || p[len] == '\\';

        if (trailing) {
            p[len] = '\0';
        }
        rc = stat(p, &s);
        wfree(p);
        if (rc == 0 && trailing && !S_ISDIR(s.st_mode)) {
            errno = ENOENT;
            return -1;
        }
    }
#else
    rc = stat(path, &s);
#endif
    if (rc < 0) {
        return -1;
    }
    sbuf->size = (Offset) s.st_size;
    sbuf->mtime = s.st_mtime;
    sbuf->isDir = S_ISDIR(s.st_mode);
    sbuf->hash = 0;
    return 0;
#endif
}


/*
    Read metadata from the same open file that supplies the response bytes.
 */
PUBLIC int websStatOpenFile(int fd, WebsFileInfo *sbuf)
{
#if ME_ROM
    RomFile      *rf;
    WebsRomIndex *wip;

    assert(fd >= 0);
    if ((rf = getRomFile(fd)) == NULL || rf->wip->page == NULL) {
        return -1;
    }
    wip = rf->wip;
    memset(sbuf, 0, sizeof(WebsFileInfo));
    sbuf->size = wip->size;
    sbuf->mtime = wip->mtime;
    sbuf->hash = wip->hash;
    return 0;
#else
    WebsStat s;

    assert(fd >= 0);
    if (fd < 0 || fstat(fd, &s) < 0) {
        return -1;
    }
    sbuf->size = (Offset) s.st_size;
    sbuf->mtime = s.st_mtime;
    sbuf->isDir = S_ISDIR(s.st_mode);
    sbuf->hash = 0;
    return 0;
#endif
}


PUBLIC ssize websReadFile(int fd, char *buf, ssize size)
{
#if ME_ROM
    RomFile *rf;
    ssize   len;

    assert(buf);
    assert(fd >= 0);

    if ((rf = getRomFile(fd)) == NULL) {
        errno = EBADF;
        return -1;
    }
    if (rf->pos >= rf->wip->size) {
        return 0;
    }
    len = (ssize) min(rf->wip->size - rf->pos, size);
    memcpy(buf, &rf->wip->page[rf->pos], len);
    rf->pos += len;
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

    if (websStatFile(path, &sbuf) < 0 || sbuf.size >= MAXSSIZE) {
        return 0;
    }
    buf = walloc((ssize) sbuf.size + 1);
    /*
        O_BINARY prevents Windows text-mode CRLF->LF translation, which shrinks the read below sbuf.size
        and would otherwise leave uninitialized heap between the data and the NUL terminator. The flag is
        a no-op on Unix (defined to 0 in osdep.h).
     */
    if ((fd = websOpenFile(path, O_RDONLY | O_BINARY, 0)) < 0) {
        wfree(buf);
        return 0;
    }
    nbytes = websReadFile(fd, buf, (ssize) sbuf.size);
    buf[nbytes > 0 ? nbytes : 0] = '\0';
    websCloseFile(fd);
    return buf;
}


Offset websSeekFile(int fd, Offset offset, int origin)
{
#if ME_ROM
    RomFile *rf;
    Offset  pos;

    assert(origin == SEEK_SET || origin == SEEK_CUR || origin == SEEK_END);
    assert(fd >= 0);

    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    if ((rf = getRomFile(fd)) == NULL) {
        errno = EBADF;
        return -1;
    }
    pos = offset;
    switch (origin) {
    case SEEK_CUR:
        pos = rf->pos + offset;
        break;
    case SEEK_END:
        pos = rf->wip->size + offset;
        break;
    default:
        break;
    }
    if (pos < 0) {
        errno = EBADF;
        return -1;
    }
    return rf->pos = pos;
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
static RomFile *getRomFile(int fd)
{
    if (fd < 0 || fd >= romFileMax) {
        return NULL;
    }
    return romFiles[fd];
}


/*
    A trailing slash may name a directory, never a document.
 */
static WebsRomIndex *lookup(WebsHash fs, cchar *path)
{
    WebsRomIndex *wip;
    WebsKey      *sp;
    char         *name;
    ssize        len;
    bool         trailing;

    trailing = 0;
    if ((sp = hashLookup(fs, path)) == NULL) {
        if (path[0] != '/') {
            name = sfmt("/%s", path);
        } else {
            name = sclone(path);
        }
        len = slen(name) - 1;
        if (name[len] == '/') {
            name[len] = '\0';
            trailing = 1;
        }
        sp = hashLookup(fs, name);
        wfree(name);
        if (sp == NULL) {
            return 0;
        }
    }
    wip = (WebsRomIndex*) sp->content.value.symbol;
    if (trailing && wip->page != NULL) {
        return 0;
    }
    return wip;
}
#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
