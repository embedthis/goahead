/*
    upload.c -- File upload handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

#if ME_GOAHEAD_UPLOAD
/************************************ Locals **********************************/
/*
    Upload states
 */
#define UPLOAD_REQUEST_HEADER    1   /* Request header */
#define UPLOAD_BOUNDARY          2   /* Boundary divider */
#define UPLOAD_CONTENT_HEADER    3   /* Content part header */
#define UPLOAD_CONTENT_DATA      4   /* Content encoded data */
#define UPLOAD_CONTENT_END       5   /* End of multipart message */

#define MAX_BOUNDARY             512

static char *uploadDir;

/*********************************** Forwards *********************************/

static void defineUploadVars(Webs *wp);
static char *getBoundary(Webs *wp, char *buf, ssize bufLen);
static void initUpload(Webs *wp);
static void processContentBoundary(Webs *wp, char *line);
static bool processContentData(Webs *wp);
static void processUploadHeader(Webs *wp, char *line);

/************************************* Code ***********************************/
/*
    The upload handler functions as a filter.
    Return false because the upload handler is not a terminal handler.
 */
static bool uploadHandler(Webs *wp)
{
    return 0;
}


static void initUpload(Webs *wp)
{
    char    *boundary;

    if (wp->uploadState == 0) {
        wp->uploadState = UPLOAD_BOUNDARY;
        if ((boundary = strstr(wp->contentType, "boundary=")) != 0) {
            boundary += 9;
            wfree(wp->boundary);
            wp->boundary = sfmt("--%s", boundary);
            wp->boundaryLen = strlen(wp->boundary);
        }
        if (wp->boundaryLen == 0 || *wp->boundary == '\0' || slen(wp->boundary) > MAX_BOUNDARY) {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Bad boundary");
        } else {
            websSetVar(wp, "UPLOAD_DIR", uploadDir);
            wp->files = hashCreate(11);
        }
    }
}


static void freeUploadFile(WebsUpload *up)
{
    if (up) {
        if (up->filename) {
            unlink(up->filename);
            wfree(up->filename);
        }
        wfree(up->clientFilename);
        wfree(up->contentType);
        wfree(up);
    }
}


PUBLIC void websFreeUpload(Webs *wp)
{
    WebsUpload  *up;
    WebsKey     *s;

    if (wp->files >= 0) {
        for (s = hashFirst(wp->files); s; s = hashNext(wp->files, s)) {
            up = s->content.value.symbol;
            freeUploadFile(up);
            if (up == wp->currentFile) {
                wp->currentFile = 0;
            }
        }
        hashFree(wp->files);
    }
    if (wp->currentFile) {
        freeUploadFile(wp->currentFile);
        wp->currentFile = 0;
    }
    if (wp->upfd >= 0) {
        close(wp->upfd);
        wp->upfd = -1;
    }
}


PUBLIC bool websProcessUploadData(Webs *wp)
{
    char    *line, *nextTok;
    ssize   nbytes;
    bool    canProceed;

    line = 0;
    canProceed = 1;
    while (canProceed && !wp->finalized && wp->uploadState != UPLOAD_CONTENT_END) {
        if  (wp->uploadState == UPLOAD_BOUNDARY || wp->uploadState == UPLOAD_CONTENT_HEADER) {
            /*
                Parse the next input line
             */
            line = wp->input.servp;
            if ((nextTok = memchr(line, '\n', bufLen(&wp->input))) == 0) {
                /* Incomplete line */
                canProceed = 0;
                break;
            }
            *nextTok++ = '\0';
            nbytes = nextTok - line;
            assert(nbytes > 0);
            strim(line, "\r", WEBS_TRIM_END);
        }
        switch (wp->uploadState) {
        case 0:
            initUpload(wp);
            break;

        case UPLOAD_BOUNDARY:
            processContentBoundary(wp, line);
            websConsumeInput(wp, nbytes);
            break;

        case UPLOAD_CONTENT_HEADER:
            processUploadHeader(wp, line);
            websConsumeInput(wp, nbytes);
            break;

        case UPLOAD_CONTENT_DATA:
            canProceed = processContentData(wp);
            if (bufLen(&wp->input) < wp->boundaryLen) {
                /*  Incomplete boundary - return to get more data */
                canProceed = 0;
            }
            break;

        case UPLOAD_CONTENT_END:
            break;
        }
    }
    bufCompact(&wp->input);
    return canProceed;
}


static void processContentBoundary(Webs *wp, char *line)
{
    /*
        Expecting a multipart boundary string
     */
    if (strncmp(wp->boundary, line, wp->boundaryLen) != 0) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Incomplete boundary");

    } else if (line[wp->boundaryLen] && strcmp(&line[wp->boundaryLen], "--") == 0) {
        wp->uploadState = UPLOAD_CONTENT_END;

    } else {
        wp->uploadState = UPLOAD_CONTENT_HEADER;
    }
}


static void processUploadHeader(Webs *wp, char *line)
{
    WebsUpload  *file;
    char        *key, *headerTok, *rest, *nextPair, *value;

    if (line[0] == '\0') {
        wp->uploadState = UPLOAD_CONTENT_DATA;
        return;
    }
    trace(7, "Header line: %s", line);

    headerTok = line;
    stok(line, ": ", &rest);

    if (scaselesscmp(headerTok, "Content-Disposition") == 0) {
        /*
            The content disposition header describes either a form variable or an uploaded file.

            Content-Disposition: form-data; name="field1"
            >>blank line
            Field Data
            ---boundary

            Content-Disposition: form-data; name="field1" filename="user.file"
            >>blank line
            File data
            ---boundary
         */
        key = rest;
        wfree(wp->uploadVar);
        wfree(wp->clientFilename);
        wp->uploadVar = wp->clientFilename = 0;

        while (key && stok(key, ";\r\n", &nextPair)) {

            key = strim(key, " ", WEBS_TRIM_BOTH);
            ssplit(key, "= ", &value);
            value = strim(value, "\"", WEBS_TRIM_BOTH);

            if (scaselesscmp(key, "form-data") == 0) {
                /* Nothing to do */

            } else if (scaselesscmp(key, "name") == 0) {
                wfree(wp->uploadVar);
                wp->uploadVar = sclone(value);

            } else if (scaselesscmp(key, "filename") == 0) {
                if (wp->uploadVar == 0) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing name field");
                    return;
                }
                value = websNormalizeUriPath(value);
                if (*value == '.' || !websValidUriChars(value) || strpbrk(value, "\\/:*?<>|~\"'%`^\n\r\t\f")) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Bad upload client filename");
                    wfree(value);
                    return;
                }
                wfree(wp->clientFilename);
                wp->clientFilename = value;

                /*
                    Create the file to hold the uploaded data
                 */
                wfree(wp->uploadTmp);
                if ((wp->uploadTmp = websTempFile(uploadDir, "tmp")) == 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR,
                        "Cannot create upload temp file %s. Check upload temp dir %s", wp->uploadTmp, uploadDir);
                    return;
                }
                trace(5, "File upload of: %s stored as %s", wp->clientFilename, wp->uploadTmp);

                if ((wp->upfd = open(wp->uploadTmp, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot open upload temp file %s", wp->uploadTmp);
                    return;
                }
                /*
                    Create the files[id]
                 */
                freeUploadFile(wp->currentFile);
                file = wp->currentFile = walloc(sizeof(WebsUpload));
                memset(file, 0, sizeof(WebsUpload));
                file->clientFilename = sclone(wp->clientFilename);
                file->filename = sclone(wp->uploadTmp);
            }
            key = nextPair;
        }

    } else if (scaselesscmp(headerTok, "Content-Type") == 0) {
        if (wp->clientFilename) {
            trace(5, "Set files[%s][CONTENT_TYPE] = %s", wp->uploadVar, rest);
            wp->currentFile->contentType = sclone(rest);
        }
    }
}


static void defineUploadVars(Webs *wp)
{
    WebsUpload      *file;
    char            key[64];

    file = wp->currentFile;
    fmt(key, sizeof(key), "FILE_CLIENT_FILENAME_%s", wp->uploadVar);
    websSetVar(wp, key, file->clientFilename);

    fmt(key, sizeof(key), "FILE_CONTENT_TYPE_%s", wp->uploadVar);
    websSetVar(wp, key, file->contentType);

    fmt(key, sizeof(key), "FILE_FILENAME_%s", wp->uploadVar);
    websSetVar(wp, key, file->filename);

    fmt(key, sizeof(key), "FILE_SIZE_%s", wp->uploadVar);
    websSetVarFmt(wp, key, "%d", (int) file->size);
}


static int writeToFile(Webs *wp, char *data, ssize len)
{
    WebsUpload      *file;
    ssize           rc;

    file = wp->currentFile;

    if ((file->size + len) > ME_GOAHEAD_LIMIT_UPLOAD) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Uploaded file exceeds maximum %d", (int) ME_GOAHEAD_LIMIT_UPLOAD);
        return -1;
    }
    if (len > 0) {
        /*
            File upload. Write the file data.
         */
        if ((rc = write(wp->upfd, data, (int) len)) != len) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot write to upload temp file %s, rc %d", wp->uploadTmp, rc);
            return -1;
        }
        file->size += len;
        trace(7, "uploadFilter: Wrote %d bytes to %s", len, wp->uploadTmp);
    }
    return 0;
}


static bool processContentData(Webs *wp)
{
    WebsUpload  *file;
    WebsBuf     *content;
    WebsKey     *sp;
    ssize       size, nbytes, len;
    char        *data, *bp;

    content = &wp->input;
    file = wp->currentFile;

    size = bufLen(content);
    if (size < wp->boundaryLen) {
        /*  Incomplete boundary. Return and get more data */
        return 0;
    }
    if ((bp = getBoundary(wp, content->servp, size)) == 0) {
        trace(7, "uploadFilter: Got boundary filename %x", wp->clientFilename);
        if (wp->clientFilename) {
            /*
                No signature found yet. probably more data to come. Must handle split boundaries.
                Note: the trailing boundary has a CRLF prefix (2).
             */
            data = content->servp;
            nbytes = ((int) (content->endp - data)) - (wp->boundaryLen - 1 + 2);
            if (nbytes > 0) {
                if (writeToFile(wp, content->servp, nbytes) < 0) {
                    /* Proceed to handle error */
                    return 1;
                }
                websConsumeInput(wp, nbytes);
            }
            //  Get more data
            return 0;
        }
    }
    data = content->servp;
    nbytes = (bp) ? (bp - data) : bufLen(content);

    if (nbytes > 0) {
        /*
            This is the final CRLF before the boundary
         */
        len = nbytes;
        if (len >= 2 && data[len - 2] == '\r' && data[len - 1] == '\n') {
            len -= 2;
        }
        if (wp->clientFilename) {
            /*
                Write the last bit of file data and add to the list of files and define environment variables
             */
            if (len > 0) {
                if (writeToFile(wp, data, len) < 0) {
                    /* Proceed to handle error */
                    websConsumeInput(wp, nbytes);
                    return 1;
                }
            }
            hashEnter(wp->files, wp->uploadVar, valueSymbol(file), 0);
            defineUploadVars(wp);
            wp->currentFile = 0;

        } else if (wp->uploadVar) {
            /*
                Normal string form data variables
             */
            data[len] = '\0';
            trace(5, "uploadFilter: form[%s] = %s", wp->uploadVar, data);
            websDecodeUrl(wp->uploadVar, wp->uploadVar, -1);
            websDecodeUrl(data, data, -1);
            sp = websSetVar(wp, wp->uploadVar, data);
            //  Flag as untrusted so CGI will prefix
            sp->arg = 1;
        }
        websConsumeInput(wp, nbytes);
    }
    if (wp->clientFilename) {
        /*
            Now have all the data (we've seen the boundary)
         */
        close(wp->upfd);
        wp->upfd = -1;
        wfree(wp->clientFilename);
        wp->clientFilename = 0;
        wfree(wp->uploadTmp);
        wp->uploadTmp = 0;
    }
    wp->uploadState = UPLOAD_BOUNDARY;
    return 1;
}


/*
    Find the boundary signature in memory. Returns pointer to the first match.
 */
static char *getBoundary(Webs *wp, char *buf, ssize bufLen)
{
    char    *cp, *endp;
    char    first;

    assert(buf);

    first = *wp->boundary;
    if (bufLen < wp->boundaryLen) {
        return 0;
    }
    cp = buf;
    endp = cp + (bufLen - wp->boundaryLen) + 1;
    while (cp < endp) {
        cp = (char *) memchr(cp, first, endp - cp);
        if (!cp) {
            return 0;
        }
        if (memcmp(cp, wp->boundary, wp->boundaryLen) == 0) {
            return cp;
        }
        cp++;
    }
    return 0;
}


WebsUpload *websLookupUpload(Webs *wp, cchar *key)
{
    WebsKey     *sp;

    if (wp->files >= 0) {
        if ((sp = hashLookup(wp->files, key)) == 0) {
            return 0;
        }
        return sp->content.value.symbol;
    }
    return 0;
}


WebsHash websGetUpload(Webs *wp)
{
    return wp->files;
}


PUBLIC void websUploadOpen(void)
{
    uploadDir = ME_GOAHEAD_UPLOAD_DIR;
    if (*uploadDir == '\0') {
#if ME_WIN_LIKE
        uploadDir = getenv("TEMP");
#else
        uploadDir = "/tmp";
#endif
    }
    trace(4, "Upload directory is %s", uploadDir);
    websDefineHandler("upload", 0, uploadHandler, 0, 0);
}

#endif /* ME_GOAHEAD_UPLOAD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
