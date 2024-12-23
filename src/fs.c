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

/*********************************** Code *************************************/

PUBLIC int websFsOpen(void)
{
#if ME_ROM
    WebsRomIndex    *wip;
    char            name[ME_GOAHEAD_LIMIT_FILENAME];
    ssize           len;

    romFs = hashCreate(WEBS_HASH_INIT);
    for (wip = websRomIndex; wip->path; wip++) {
        strncpy(name, wip->path, ME_GOAHEAD_LIMIT_FILENAME);
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
    WebsRomIndex    *wip;

    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
    wip->pos = 0;
    return (int) (wip - websRomIndex);
#else
    return open(path, flags, mode);
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
    WebsRomIndex    *wip;

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
    WebsStat    s;
    int         rc;
#if ME_WIN_LIKE
{
    ssize       len = slen(path) - 1;
    char        *p = sclone(path);
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
    sbuf->isDir = s.st_mode & S_IFDIR;
    return 0;
#endif
}


PUBLIC ssize websReadFile(int fd, char *buf, ssize size)
{
#if ME_ROM
    WebsRomIndex    *wip;
    ssize           len;

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
    WebsFileInfo    sbuf;
    char            *buf;
    int             fd;

    if (websStatFile(path, &sbuf) < 0) {
        return 0;
    }
    buf = walloc(sbuf.size + 1);
    if ((fd = websOpenFile(path, O_RDONLY, 0)) < 0) {
        wfree(buf);
        return 0;
    }
    websReadFile(fd, buf, sbuf.size);
    buf[sbuf.size] = '\0';
    websCloseFile(fd);
    return buf;
}


Offset websSeekFile(int fd, Offset offset, int origin)
{
#if ME_ROM
    WebsRomIndex    *wip;
    Offset          pos;

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
    return (wip->pos = pos);
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
    WebsKey     *sp;
    ssize       len;

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
