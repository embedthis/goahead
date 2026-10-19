/*
    file.c -- File handler

    This module serves static file documents

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char *websIndex;                     /* Default page name */
static char *websDocuments;                 /* Default Web page directory */

/**************************** Forward Declarations ****************************/

#if !ME_ROM
static int commitPut(Webs *wp);
#if !ME_WIN_LIKE
static int copyFile(cchar *from, cchar *to);
#endif
#endif
static void fileWriteEvent(Webs *wp);
static bool isTemplate(cchar *ext);
static char *makeEtag(WebsFileInfo *info);
static bool matchEtag(cchar *list, cchar *etag);
static bool notModified(Webs *wp, WebsFileInfo *info, cchar *etag);

/*********************************** Code *************************************/
/*
    Serve static files
    Return true to indicate the request was handled, even for errors.
 */
static bool fileHandler(Webs *wp)
{
    WebsFileInfo info;
    char         *tmp, *date, *path, *encoded, *etag;
    ssize        nchars;
    int          code;

#if !ME_ROM
    bool existed;
#endif

    assert(websValid(wp));
    assert(wp->method);
    assert(wp->filename && wp->filename[0]);

#if !ME_ROM
    if (smatch(wp->method, "DELETE")) {
        if (!websPageAllowed(wp) || unlink(wp->filename) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot delete the URI");
        } else {
            /* No content */
            websResponse(wp, 204, 0);
        }
    } else if (smatch(wp->method, "PUT")) {
        //  A staged body that is not committed here is removed when the request ends
        if (!websPageAllowed(wp)) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot put the URI");
            return 1;
        }
        /*
            Do not let PUT create templates that a later request could execute.
         */
        if (isTemplate(wp->ext)) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot put a template: %s", wp->path);
            return 1;
        }
        existed = websPageStat(wp, &info) >= 0;
        if (commitPut(wp) < 0) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot save the put URI");
            return 1;
        }
        websResponse(wp, existed ? HTTP_CODE_NO_CONTENT : HTTP_CODE_CREATED, 0);

    } else
#endif /* !ME_ROM */
    {
        if (isTemplate(wp->ext)) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot serve template source for: %s", wp->path);
            return 1;
        }
        /*
            If the file is a directory, redirect using the nominated default page
         */
        if (websPageIsDirectory(wp)) {
            /*
                Copy the authorized path before trimming, then URI-encode it for Location.
             */
            path = sclone(wp->path);
            nchars = slen(path);
            if (nchars > 0 && (path[nchars - 1] == '/' || path[nchars - 1] == '\\')) {
                path[--nchars] = '\0';
            }
            encoded = websEscapeUri(path);
            tmp = sfmt("%s/%s", encoded, websIndex);
            websRedirect(wp, tmp);
            wfree(tmp);
            wfree(encoded);
            wfree(path);
            return 1;
        }
        if (websPageOpen(wp, O_RDONLY | O_BINARY, 0666) < 0) {
#if ME_DEBUG
            if (wp->referrer) {
                trace(1, "From %s", wp->referrer);
            }
#endif
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot open document for: %s", wp->path);
            return 1;
        }
        if (websPageStat(wp, &info) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot stat page for URL");
            return 1;
        }
        code = 200;
        etag = makeEtag(&info);
        if (notModified(wp, &info, etag)) {
            code = 304;
            info.size = 0;
        }
        websSetStatus(wp, code);
        wp->flags |= WEBS_CACHEABLE;
        websWriteHeaders(wp, info.size, 0);
        if (info.mtime && (date = websGetDateString(&info)) != NULL) {
            websWriteHeader(wp, "Last-Modified", "%s", date);
            wfree(date);
        }
        websWriteHeader(wp, "ETag", "%s", etag);
        wfree(etag);
        websWriteEndHeaders(wp);

        /*
            All done if the browser did a HEAD request
         */
        if (smatch(wp->method, "HEAD")) {
            websDone(wp);
            return 1;
        }
        if (info.size > 0) {
            wp->txRemaining = info.size;
            websSetBackgroundWriter(wp, fileWriteEvent);
        } else {
            websDone(wp);
        }
    }
    return 1;
}


/*
    Template pages are run by the template handler, never served as source, whatever the route table or build
 */
static bool isTemplate(cchar *ext)
{
    return smatch(ext, ".jst") || smatch(ext, ".asp");
}


/*
    Use weak timestamp tags for files; hash ROM content so equal size and time cannot hide changes.
 */
static char *makeEtag(WebsFileInfo *info)
{
    char   version[24], size[24];
    uint64 hash;
    int    i;

    if (info->hash) {
        for (hash = info->hash, i = 15; i >= 0; i--, hash >>= 4) {
            version[i] = "0123456789abcdef"[hash & 0xF];
        }
        version[16] = '\0';
    } else {
        itosbuf(version, sizeof(version), (int64) info->mtime, 16);
    }
    itosbuf(size, sizeof(size), (int64) info->size, 16);
    return sfmt("W/\"%s-%s\"", version, size);
}


/*
    Weakly compare listed opaque tags; * matches any representation.
 */
static bool matchEtag(cchar *list, cchar *etag)
{
    char *buf, *tag, *next;
    bool match;

    if (sstarts(etag, "W/")) {
        etag += 2;
    }
    buf = sclone(list);
    match = 0;
    for (tag = stok(buf, ",", &next); tag && !match; tag = stok(NULL, ",", &next)) {
        tag = strim(tag, " \t", WEBS_TRIM_BOTH);
        if (sstarts(tag, "W/")) {
            tag += 2;
        }
        match = smatch(tag, "*") || smatch(tag, etag);
    }
    wfree(buf);
    return match;
}


/*
    If-None-Match takes precedence over If-Modified-Since (RFC 9110 13.2.2).
 */
static bool notModified(Webs *wp, WebsFileInfo *info, cchar *etag)
{
    if (!smatch(wp->method, "GET") && !smatch(wp->method, "HEAD")) {
        return 0;
    }
    if (wp->ifNoneMatch) {
        return matchEtag(wp->ifNoneMatch, etag);
    }
    return wp->since && info->mtime && info->mtime <= wp->since;
}


/*
    Write directly to the socket, bypassing the output buffer. Bound each call so a fast reader
    cannot starve other connections.
 */
static void fileWriteEvent(Webs *wp)
{
    char  *buf;
    ssize size, len, wrote, slice;
    int   err;

    assert(wp);
    assert(websValid(wp));

    if ((buf = walloc(ME_GOAHEAD_LIMIT_BUFFER)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get memory");
        return;
    }
    slice = ME_GOAHEAD_LIMIT_SLICE;
    while (wp->txRemaining > 0 && slice > 0) {
        size = (ssize) min(wp->txRemaining, min(ME_GOAHEAD_LIMIT_BUFFER, slice));
        len = websPageReadData(wp, buf, size);
        if (len <= 0) {
            /*
                Close a truncated response; appending an error page would corrupt its declared body.
             */
            error("Cannot read %s after the response was committed, closing the connection", wp->filename);
            wfree(buf);
            wp->flags &= ~WEBS_KEEP_ALIVE;
            wp->state = WEBS_COMPLETE;
            websDone(wp);
            return;
        }
        if ((wrote = websWriteSocket(wp, buf, len)) < 0) {
            err = socketGetError(wp->sid);
            if (err == EWOULDBLOCK || err == EAGAIN) {
                websPageSeek(wp, -len, SEEK_CUR);
            } else {
                /* Will call websDone below */
                wp->state = WEBS_COMPLETE;
            }
            break;
        }
        wp->txRemaining -= wrote;
        slice -= wrote;
        if (wrote != len) {
            websPageSeek(wp, -(len - wrote), SEEK_CUR);
            break;
        }
    }
    wfree(buf);
    if (wp->txRemaining <= 0) {
        websDone(wp);
    }
}


#if !ME_ROM
#if !ME_WIN_LIKE
/*
    Copy privately across filesystems and remove partial copies.
 */
static int copyFile(cchar *from, cchar *to)
{
    char  buf[ME_GOAHEAD_LIMIT_BUFFER];
    ssize nbytes;
    int   in, out, rc;

    if ((in = open(from, O_RDONLY | O_BINARY)) < 0) {
        return -1;
    }
    if ((out = open(to, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_NOFOLLOW, 0600)) < 0) {
        close(in);
        return -1;
    }
    rc = 0;
    while ((nbytes = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, (size_t) nbytes) != nbytes) {
            rc = -1;
            break;
        }
    }
    if (nbytes < 0) {
        rc = -1;
    }
    close(in);
    if (close(out) < 0) {
        rc = -1;
    }
    if (rc < 0) {
        unlink(to);
    }
    return rc;
}
#endif


/*
    Commit the staged PUT before reporting success.
 */
static int commitPut(Webs *wp)
{
    if (wp->putfd < 0 || !wp->putname || !wp->filename) {
        error("Cannot complete PUT: no staged file or target");
        return -1;
    }
    close(wp->putfd);
    wp->putfd = -1;
#if ME_WIN_LIKE
    //  rename cannot replace an existing file on Windows, and cannot move one between volumes
    if (!MoveFileExA(wp->putname, wp->filename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        error("Cannot move PUT file from %s to %s", wp->putname, wp->filename);
        return -1;
    }
#else
    if (rename(wp->putname, wp->filename) < 0) {
        if (errno != EXDEV || copyFile(wp->putname, wp->filename) < 0) {
            error("Cannot rename PUT file from %s to %s, errno %d", wp->putname, wp->filename, errno);
            return -1;
        }
        unlink(wp->putname);
    }
#endif
    wfreeset(wp->putname);
    return 0;
}


PUBLIC bool websProcessPutData(Webs *wp)
{
    ssize nbytes;

    assert(wp);
    assert(wp->putfd >= 0);
    assert(wp->input.buf);

    nbytes = bufLen(&wp->input);
    //  Tested against the room left, as the running total plus a buffer can overflow
    if (nbytes > ME_GOAHEAD_LIMIT_PUT - wp->putLen) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Put file too large");

    } else if (write(wp->putfd, bufStart(&wp->input), (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot write to file");

    } else {
        wp->putLen += nbytes;
    }
    websConsumeInput(wp, nbytes);
    return 1;
}
#endif


static void fileClose(void)
{
    wfree(websIndex);
    websIndex = NULL;
    wfree(websDocuments);
    websDocuments = NULL;
}


PUBLIC void websFileOpen(void)
{
    websIndex = sclone("index.html");
    websDefineHandler("file", 0, fileHandler, fileClose, 0);
}


/*
    Get the default page for URL requests ending in "/"
 */
PUBLIC cchar *websGetIndex(void)
{
    return websIndex;
}


PUBLIC char *websGetDocuments(void)
{
    return websDocuments;
}


/*
    Set the default page for URL requests ending in "/"
 */
PUBLIC void websSetIndex(cchar *page)
{
    assert(page && *page);

    if (websIndex) {
        wfree(websIndex);
    }
    websIndex = sclone(page);
}


/*
    Set the default web directory
 */
PUBLIC void websSetDocuments(cchar *dir)
{
    /*
        Reject an empty root, which would expose the filesystem root.
     */
    if (dir == 0 || *dir == '\0') {
        error("Documents directory must not be empty");
        return;
    }
    if (websDocuments) {
        wfree(websDocuments);
    }
    websDocuments = sclone(dir);
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
