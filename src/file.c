/*
    file.c -- File handler

    This module serves static file documents
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char   *websIndex;                   /* Default page name */
static char   *websDocuments;               /* Default Web page directory */

/**************************** Forward Declarations ****************************/

static void fileWriteEvent(Webs *wp);

/*********************************** Code *************************************/
/*
    Serve static files
    Return true to indicate the request was handled, even for errors.
 */
static bool fileHandler(Webs *wp)
{
    WebsFileInfo    info;
    char            *tmp, *date;
    ssize           nchars;
    int             code;

    assert(websValid(wp));
    assert(wp->method);
    assert(wp->filename && wp->filename[0]);

#if !ME_ROM
    if (smatch(wp->method, "DELETE")) {
        if (unlink(wp->filename) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot delete the URI");
        } else {
            /* No content */
            websResponse(wp, 204, 0);
        }
    } else if (smatch(wp->method, "PUT")) {
        /* Code is already set for us by processContent() */
        websResponse(wp, wp->code, 0);

    } else
#endif /* !ME_ROM */
    {
        /*
            If the file is a directory, redirect using the nominated default page
         */
        if (websPageIsDirectory(wp)) {
            nchars = strlen(wp->path);
            if (wp->path[nchars - 1] == '/' || wp->path[nchars - 1] == '\\') {
                wp->path[--nchars] = '\0';
            }
            tmp = sfmt("%s/%s", wp->path, websIndex);
            websRedirect(wp, tmp);
            wfree(tmp);
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
        if (wp->since && info.mtime <= wp->since) {
            code = 304;
            info.size = 0;
        }
        websSetStatus(wp, code);
        websWriteHeaders(wp, info.size, 0);
        if ((date = websGetDateString(&info)) != NULL) {
            websWriteHeader(wp, "Last-Modified", "%s", date);
            wfree(date);
        }
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
    Do output back to the browser in the background. This is a socket write handler.
    This bypasses the output buffer and writes directly to the socket.
 */
static void fileWriteEvent(Webs *wp)
{
    char    *buf;
    ssize   size, len, wrote;
    int     err;

    assert(wp);
    assert(websValid(wp));

    if ((buf = walloc(ME_GOAHEAD_LIMIT_BUFFER)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get memory");
        return;
    }
    while (wp->txRemaining > 0) {
        size = min(wp->txRemaining, ME_GOAHEAD_LIMIT_BUFFER);
        len = websPageReadData(wp, buf, size);
        if (len <= 0) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot read file content");
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
        if (wrote != len) {
            websPageSeek(wp, - (len - wrote), SEEK_CUR);
            break;
        }
    }
    wfree(buf);
    if (wp->txRemaining <= 0) {
        websDone(wp);
    }
}


#if !ME_ROM
PUBLIC bool websProcessPutData(Webs *wp)
{
    ssize   nbytes;

    assert(wp);
    assert(wp->putfd >= 0);
    assert(wp->input.buf);

    nbytes = bufLen(&wp->input);
    wp->putLen += nbytes;
    if (wp->putLen > ME_GOAHEAD_LIMIT_PUT) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Put file too large");

    } else if (write(wp->putfd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot write to file");
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
    assert(dir && *dir);
    if (websDocuments) {
        wfree(websDocuments);
    }
    websDocuments = sclone(dir);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
