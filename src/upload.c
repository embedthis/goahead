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
#define UPLOAD_REQUEST_HEADER 1      /* Request header */
#define UPLOAD_BOUNDARY       2      /* Boundary divider */
#define UPLOAD_CONTENT_HEADER 3      /* Content part header */
#define UPLOAD_CONTENT_DATA   4      /* Content encoded data */
#define UPLOAD_CONTENT_END    5      /* End of multipart message */

#define MAX_BOUNDARY          70     /* RFC 2046 5.1.1 */
#define MAX_PADDING           256    /* Transport padding accepted after a delimiter */

#define BOUNDARY_MORE         -1     /* The data ends inside a possible delimiter */

static cchar *uploadDir;

/*
    The upload variable prefixes, indexed by UPLOAD_VAR_*. Both defineUploadVars and websIsUploadVar use them.
 */
#define UPLOAD_VAR_CLIENT_FILENAME 0
#define UPLOAD_VAR_CONTENT_TYPE    1
#define UPLOAD_VAR_FILENAME        2
#define UPLOAD_VAR_SIZE            3

static cchar *uploadVars[] = {
    "FILE_CLIENT_FILENAME_", "FILE_CONTENT_TYPE_", "FILE_FILENAME_", "FILE_SIZE_", 0
};

/*********************************** Forwards *********************************/

static void defineUploadVars(Webs *wp);
static int  delimiterSuffix(cchar *cp, cchar *end);
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


/*
    Test a boundary against RFC 2046 5.1.1: 1 to 70 bchars, not ending in a space
 */
static bool validBoundary(cchar *boundary)
{
    ssize len;

    len = slen(boundary);
    if (len == 0 || len > MAX_BOUNDARY || boundary[len - 1] == ' ') {
        return 0;
    }
    for (; *boundary; boundary++) {
        if (!isalnum((uchar) *boundary) && !strchr("'()+_,-./:=? ", *boundary)) {
            return 0;
        }
    }
    return 1;
}


static void initUpload(Webs *wp)
{
    char *boundary;

    if (wp->uploadState == 0) {
        wp->uploadState = UPLOAD_BOUNDARY;
        boundary = websParseContentType(wp->contentType, "boundary");
        if (boundary && validBoundary(boundary)) {
            wfreeset(wp->boundary);
            /*
                A delimiter is CRLF "--" boundary (RFC 2046 5.1.1). The first delimiter has no preceding CRLF and is
                matched against wp->boundary + 2.
             */
            wp->boundary = sfmt("\r\n--%s", boundary);
            wp->boundaryLen = (ssize) strlen(wp->boundary);
        }
        wfree(boundary);
        if (wp->boundaryLen == 0) {
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
            wfreeset(up->filename);
        }
        wfreeset(up->clientFilename);
        wfreeset(up->contentType);
        wfree(up);
    }
}


/*
    Release the text fields held for the body. On a complete body they have already been stored as request variables.
 */
static void freeUploadFields(Webs *wp)
{
    int i;

    for (i = 0; i < wp->uploadFieldCount * 2; i++) {
        wfreeset(wp->uploadFields[i]);
    }
    wfreeset(wp->uploadFields);
    wp->uploadFieldCount = 0;
}


PUBLIC void websFreeUpload(Webs *wp)
{
    WebsUpload *up;
    WebsKey    *s;

    freeUploadFields(wp);
    //  Windows requires closing the upload before removing it.
    if (wp->upfd >= 0) {
        close(wp->upfd);
        wp->upfd = -1;
    }
    if (wp->files >= 0) {
        for (s = hashFirst(wp->files); s; s = hashNext(wp->files, s)) {
            up = s->content.value.symbol;
            freeUploadFile(up);
            if (up == wp->currentFile) {
                wp->currentFile = 0;
            }
        }
        hashFree(wp->files);
        wp->files = -1;
    }
    if (wp->currentFile) {
        freeUploadFile(wp->currentFile);
        wp->currentFile = 0;
    }
}


PUBLIC bool websProcessUploadData(Webs *wp)
{
    char  *line, *nextTok;
    ssize nbytes, lineLen;
    bool  canProceed;

    line = 0;
    nbytes = 0;
    canProceed = 1;
    while (canProceed && !wp->finalized && wp->uploadState != UPLOAD_CONTENT_END) {
        if (wp->uploadState == UPLOAD_BOUNDARY || wp->uploadState == UPLOAD_CONTENT_HEADER) {
            /*
                Parse the next input line
             */
            line = bufStart(&wp->input);
            //  Resume at the previous read boundary to keep line scanning linear.
            if ((nextTok = memchr(line + wp->lineScan, '\n', (size_t) (bufLen(&wp->input) - wp->lineScan))) != 0) {
                wp->lineScan = 0;
                lineLen = nextTok - line;
                *nextTok++ = '\0';
                nbytes = nextTok - line;
            } else if (wp->eof && bufLen(&wp->input) > 0) {
                //  No more input: the rest is the last line, such as a close delimiter without a final CRLF
                wp->lineScan = 0;
                nbytes = lineLen = bufLen(&wp->input);
            } else {
                /* Incomplete line */
                wp->lineScan = bufLen(&wp->input);
                canProceed = 0;
                break;
            }
            assert(nbytes > 0);
            /*
                Reject embedded NULs to prevent disagreement with byte-counting multipart parsers.
             */
            if (memchr(line, '\0', (size_t) lineLen) != 0) {
                websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Control character in part header");
                websConsumeInput(wp, nbytes);
                break;
            }
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
    if (wp->eof && !wp->finalized && wp->uploadState != UPLOAD_CONTENT_END) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing closing boundary");
        return 1;
    }
    bufCompact(&wp->input);
    return canProceed;
}


/*
    Expecting a delimiter line: "--" boundary, then transport padding, or "--" to close the body
 */
static void processContentBoundary(Webs *wp, char *line)
{
    cchar *suffix;
    ssize len;

    len = wp->boundaryLen - 2;
    if (strncmp(wp->boundary + 2, line, (size_t) len) != 0) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Incomplete boundary");
        return;
    }
    suffix = &line[len];
    if (suffix[0] == '-' && suffix[1] == '-') {
        wp->uploadState = UPLOAD_CONTENT_END;
        /*
            Store the text fields now the body is complete, so a repeated name is joined once rather than
            re-copied for every part that repeats it.
         */
        websSetRequestVarPairs(wp, (cchar**) wp->uploadFields, wp->uploadFieldCount, WEBS_VAR_BODY);
        freeUploadFields(wp);

    } else if ((ssize) strspn(suffix, " \t") == slen(suffix)) {
        if (++wp->uploadParts > ME_GOAHEAD_LIMIT_PARAMS) {
            websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Too many upload parts");
            return;
        }
        //  Each part describes itself (RFC 7578 4.2): nothing carries over from the previous part
        wfreeset(wp->uploadVar);
        wfreeset(wp->clientFilename);
        wp->uploadState = UPLOAD_CONTENT_HEADER;

    } else {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Bad boundary");
    }
}


/*
    Accept only an empty or plain filename without prohibited dots, separators, characters, or device names.
    Do not normalize client filenames.
 */
static bool validClientFilename(cchar *name)
{
    ssize len;

    len = slen(name);
    if (len == 0) {
        return 1;
    }
    if (*name == '.' || name[len - 1] == '.' || name[len - 1] == ' ') {
        return 0;
    }
    if (!websValidUriChars(name) || strpbrk(name, "\\/:*?<>|~\"'%`^\n\r\t\f")) {
        return 0;
    }
    return !websIsReservedDevice(name);
}


/*
    Parse the next parameter of a Content-Disposition value in place: "; name=value" or "; name="quoted"", where a
    quoted value may hold ";" and quoted-pair escapes (RFC 7578 4.2, RFC 2045 5.1). A bare token such as
    "form-data" has an empty value. Return 1 with the name and value set, 0 at the end, or -1 for a malformed
    parameter.
 */
static int nextParam(char **cpp, char **name, char **value)
{
    char *cp, *vp;
    int  delim;

    cp = *cpp;
    cp += strspn(cp, "; \t");
    if (*cp == '\0') {
        *cpp = cp;
        return 0;
    }
    *name = cp;
    cp += strcspn(cp, "=; \t");
    //  Terminate the name, remembering which delimiter ended it
    if ((delim = *cp) != '\0') {
        *cp++ = '\0';
    }
    cp += strspn(cp, " \t");
    if (delim != '=' && delim != ';' && *cp == '=') {
        delim = '=';
        cp += 1 + strspn(cp + 1, " \t");
    }
    *value = "";
    if (delim == '=') {
        if (*cp == '"') {
            for (vp = *value = ++cp; *cp && *cp != '"'; cp++) {
                if (*cp == '\\' && cp[1]) {
                    cp++;
                }
                *vp++ = *cp;
            }
            if (*cp != '"') {
                return -1;
            }
            cp++;
            *vp = '\0';
        } else {
            *value = cp;
            cp += strcspn(cp, "; \t");
        }
        if ((delim = *cp) != '\0') {
            if (delim != ';' && delim != ' ' && delim != '\t') {
                return -1;
            }
            *cp++ = '\0';
        }
        cp += strspn(cp, " \t");
    }
    //  The parameter ends at ";" or the end of the value
    if (delim != ';' && *cp != '\0' && *cp != ';') {
        return -1;
    }
    *cpp = cp;
    return 1;
}


static void processUploadHeader(Webs *wp, char *line)
{
    WebsUpload *file;
    char       *key, *headerTok, *rest, *value;
    ssize      nameLen;
    int        rc;

    if (line[0] == '\0') {
        if (wp->uploadVar == 0) {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing Content-Disposition");
            return;
        }
        wp->uploadState = UPLOAD_CONTENT_DATA;
        return;
    }
    trace(7, "Header line: %s", line);

    /*
        Require field-name ":" OWS field-value; reject folding and whitespace before the colon (RFC 7230 3.2.4).
     */
    nameLen = (ssize) strcspn(line, ":");
    if (nameLen == 0 || line[nameLen] != ':' || (ssize) strcspn(line, " \t") < nameLen) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Bad part header");
        return;
    }
    headerTok = line;
    line[nameLen] = '\0';
    rest = &line[nameLen + 1];
    rest += strspn(rest, " \t");

    if (scaselesscmp(headerTok, "Content-Disposition") == 0) {
        if (wp->uploadVar) {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Repeated Content-Disposition");
            return;
        }
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
        while ((rc = nextParam(&rest, &key, &value)) > 0) {
            if (strchr(key, '*')) {
                //  RFC 2231 and RFC 5987 extended parameters are prohibited in form-data (RFC 7578 4.2)
                websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Extended Content-Disposition parameter");
                return;
            }
            if (scaselesscmp(key, "form-data") == 0) {
                /* Nothing to do */

            } else if (scaselesscmp(key, "name") == 0) {
                if (*value == '\0') {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Empty name field");
                    return;
                }
                /*
                    uploadVar is cleared at each boundary; a set value here means the name parameter is repeated.
                 */
                if (wp->uploadVar) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Repeated name parameter");
                    return;
                }
                wp->uploadVar = sclone(value);

            } else if (scaselesscmp(key, "filename") == 0) {
                if (wp->uploadVar == 0) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing name field");
                    return;
                }
                /*
                    A part may open only one file. wp->upfd is the per-part descriptor and is reset
                    when the part ends, so a descriptor open here means a repeated filename= pair.
                    Reject rather than leak the descriptor.
                 */
                if (wp->upfd >= 0) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Duplicate filename field");
                    return;
                }
                if (++wp->uploadFiles > ME_GOAHEAD_LIMIT_UPLOAD_FILES) {
                    websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Too many upload files");
                    return;
                }
                if (!validClientFilename(value)) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload client filename");
                    return;
                }
                wfreeset(wp->clientFilename);
                wp->clientFilename = sclone(value);

                /*
                    Create the file to hold the uploaded data
                 */
                wfreeset(wp->uploadTmp);
                if ((wp->uploadTmp = websTempFileOpen(uploadDir, "tmp", &wp->upfd)) == 0) {
                    /*
                        Do not name the temp file here: the assignment above is what failed, so
                        wp->uploadTmp is unconditionally NULL on this path. gcc proves it at -O2
                        and reports it as a null %s argument.
                     */
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR,
                              "Cannot create upload temp file. Check upload temp dir %s", uploadDir);
                    return;
                }
                trace(5, "File upload of: %s stored as %s", wp->clientFilename, wp->uploadTmp);

                /*
                    Create the files[id]
                 */
                freeUploadFile(wp->currentFile);
                file = wp->currentFile = walloc(sizeof(WebsUpload));
                memset(file, 0, sizeof(WebsUpload));
                file->clientFilename = sclone(wp->clientFilename);
                file->filename = sclone(wp->uploadTmp);
            }
        }
        if (rc < 0) {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Bad Content-Disposition parameter");
        }

    } else if (scaselesscmp(headerTok, "Content-Type") == 0) {
        if (wp->clientFilename) {
            trace(5, "Set files[%s][CONTENT_TYPE] = %s", wp->uploadVar, rest);
            //  A repeated line replaces the earlier value
            wfreeset(wp->currentFile->contentType);
            wp->currentFile->contentType = sclone(rest);
        }
    }
}


static void setUploadVar(Webs *wp, int index, cchar *value)
{
    char *key;

    key = sfmt("%s%s", uploadVars[index], wp->uploadVar);
    websSetVar(wp, key, value);
    wfree(key);
}


static void defineUploadVars(Webs *wp)
{
    WebsUpload *file;
    char       size[32];

    file = wp->currentFile;
    setUploadVar(wp, UPLOAD_VAR_CLIENT_FILENAME, file->clientFilename);
    setUploadVar(wp, UPLOAD_VAR_CONTENT_TYPE, file->contentType);
    setUploadVar(wp, UPLOAD_VAR_FILENAME, file->filename);
    fmt(size, sizeof(size), "%zd", file->size);
    setUploadVar(wp, UPLOAD_VAR_SIZE, size);
}


PUBLIC bool websIsUploadVar(cchar *name)
{
    cchar **prefix;

    if (smatch(name, "UPLOAD_DIR")) {
        return 1;
    }
    for (prefix = uploadVars; *prefix; prefix++) {
        if (sstarts(name, *prefix)) {
            return 1;
        }
    }
    return 0;
}


static int writeToFile(Webs *wp, char *data, ssize len)
{
    WebsUpload *file;
    ssize      rc;

    file = wp->currentFile;

    //  Tested against the room left, as the running total plus a buffer can overflow
    if (len > ME_GOAHEAD_LIMIT_UPLOAD - file->size) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Uploaded file exceeds maximum %zd",
                  (ssize) ME_GOAHEAD_LIMIT_UPLOAD);
        return -1;
    }
    if (len > 0) {
        /*
            File upload. Write the file data.
         */
        if ((rc = write(wp->upfd, data, (int) len)) != len) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot write to upload temp file %s, rc %zd", wp->uploadTmp,
                      rc);
            return -1;
        }
        file->size += len;
        trace(7, "uploadFilter: Wrote %zd bytes to %s", len, wp->uploadTmp);
    }
    return 0;
}


static bool processContentData(Webs *wp)
{
    WebsUpload *file;
    WebsBuf    *content;
    WebsKey    *sp;
    ssize      size, nbytes, len;
    char       *data, *bp;

    content = &wp->input;
    file = wp->currentFile;

    size = bufLen(content);
    if (size < wp->boundaryLen) {
        /*  Incomplete boundary. Return and get more data */
        return 0;
    }
    //  The boundary scan and the writes below read the content as one flat block
    bufStart(content);
    if ((bp = getBoundary(wp, content->servp, size)) == 0) {
        if (wp->clientFilename) {
            /* Keep a possible split delimiter; bytes before boundaryScan are safe to write. */
            nbytes = wp->boundaryScan;
            if (nbytes > 0) {
                if (writeToFile(wp, content->servp, nbytes) < 0) {
                    /* Proceed to handle error */
                    return 1;
                }
                websConsumeInput(wp, nbytes);
                wp->boundaryScan = 0;
            }
            //  Get more data
            return 0;
        }
        /*
            Retain form fields until their delimiter arrives. Resume at boundaryScan to keep scanning linear; the
            body limit bounds memory.
         */
        return 0;
    }
    data = content->servp;
    len = bp - data;
    //  Consume the CRLF that begins the delimiter too, leaving the delimiter line for processContentBoundary
    nbytes = len + 2;
    wp->boundaryScan = 0;
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
        /* Free a repeated field's previous upload and staging file; hashEnter frees only string values. */
        if ((sp = hashLookup(wp->files, wp->uploadVar)) != 0) {
            freeUploadFile(sp->content.value.symbol);
        }
        hashEnter(wp->files, wp->uploadVar, valueSymbol(file), 0);
        defineUploadVars(wp);
        wp->currentFile = 0;

    } else if (wp->uploadVar) {
        /*
            Normal string form data variables, held until the body completes. ME_GOAHEAD_LIMIT_PARAMS bounds the
            count through uploadParts.
         */
        data[len] = '\0';
        //  Multipart names and values are literal (RFC 7578), not URL encoded
#if ME_DEBUG
        trace(5, "uploadFilter: form[%s] = %s", wp->uploadVar, data);
#else
        trace(5, "uploadFilter: form[%s]", wp->uploadVar);
#endif
        wp->uploadFields = wrealloc(wp->uploadFields, (wp->uploadFieldCount + 1) * 2 * (ssize) sizeof(char*));
        wp->uploadFields[wp->uploadFieldCount * 2] = sclone(wp->uploadVar);
        wp->uploadFields[wp->uploadFieldCount * 2 + 1] = snclone(data, len);
        wp->uploadFieldCount++;
    }
    websConsumeInput(wp, nbytes);
    if (wp->clientFilename) {
        /*
            Now have all the data (we've seen the boundary)
         */
        close(wp->upfd);
        wp->upfd = -1;
        wfreeset(wp->clientFilename);
        wfreeset(wp->uploadTmp);
    }
    wp->uploadState = UPLOAD_BOUNDARY;
    return 1;
}


/*
    Return 1 for a complete delimiter suffix, 0 for data, or BOUNDARY_MORE for incomplete input.
 */
static int delimiterSuffix(cchar *cp, cchar *end)
{
    cchar *start;

    if (cp < end && *cp == '-') {
        if (cp + 1 >= end) {
            return BOUNDARY_MORE;
        }
        return cp[1] == '-';
    }
    for (start = cp; cp < end && (*cp == ' ' || *cp == '\t'); cp++) {
        if (cp - start >= MAX_PADDING) {
            return 0;
        }
    }
    if (cp + 1 >= end) {
        return cp < end && *cp != '\r' ? 0 : BOUNDARY_MORE;
    }
    return cp[0] == '\r' && cp[1] == '\n';
}


/*
    Resume delimiter scanning at boundaryScan. Return the delimiter CRLF, or 0 with boundaryScan at the first
    possible partial delimiter. Work stays linear in part size.
 */
static char *getBoundary(Webs *wp, char *buf, ssize bufLen)
{
    char  *cp, *end, *suffix;
    ssize avail;
    int   rc;

    assert(buf);

    end = buf + bufLen;
    for (cp = buf + wp->boundaryScan; cp < end; cp++) {
        if ((cp = memchr(cp, '\r', (size_t) (end - cp))) == 0) {
            break;
        }
        avail = end - cp;
        if (avail < wp->boundaryLen) {
            if (memcmp(cp, wp->boundary, (size_t) avail) == 0) {
                wp->boundaryScan = cp - buf;
                return 0;
            }
            continue;
        }
        if (memcmp(cp, wp->boundary, (size_t) wp->boundaryLen) == 0) {
            suffix = cp + wp->boundaryLen;
            if ((rc = delimiterSuffix(suffix, end)) == 1) {
                return cp;
            } else if (rc == BOUNDARY_MORE) {
                wp->boundaryScan = cp - buf;
                return 0;
            }
        }
    }
    wp->boundaryScan = bufLen;
    return 0;
}


WebsUpload *websLookupUpload(Webs *wp, cchar *key)
{
    WebsKey *sp;

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
        uploadDir = websGetTempDir();
    }
    trace(4, "Upload directory is %s", uploadDir);
    websDefineHandler("upload", 0, uploadHandler, 0, 0);
}

#endif /* ME_GOAHEAD_UPLOAD */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
