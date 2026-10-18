/*
    jst.c -- JavaScript templates

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"
#include    "js.h"

#if ME_GOAHEAD_JAVASCRIPT
/********************************** Locals ************************************/

static WebsHash websJstFunctions = -1;  /* Symbol table of functions */

/***************************** Forward Declarations ***************************/

static char *strtokcmp(char *s1, char *s2);
static char *skipWhite(char *s);
static int jstEscape(int jid, Webs *wp, int argc, char **argv);
static int jstEscapeUri(int jid, Webs *wp, int argc, char **argv);
static int jstEscapeJs(int jid, Webs *wp, int argc, char **argv);
static int jstWriteHtml(int jid, Webs *wp, int argc, char **argv);

/************************************* Code ***********************************/
/*
    Keep template source and interpreter details out of client responses. A native callback
    may already have finalized an error response; never append another response in that case.
 */
static void jstError(Webs *wp)
{
    if (!wp->finalized) {
        if (wp->flags & WEBS_HEADERS_CREATED) {
            websWrite(wp, "<h2>Javascript Error</h2>\n");
        } else {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Javascript Error");
        }
    }
}


/*
    Process requests and expand all scripting commands. We read the entire web page into memory and then process. If
    you have really big documents, it is better to make them plain HTML files rather than Javascript web pages.
    Return true to indicate the request was handled, even for errors.
 */
static bool jstHandler(Webs *wp)
{
    WebsFileInfo sbuf;
    char         *lang, *token, *result, *ep, *cp, *buf, *nextp, *last;
    ssize        len;
    int          rc, jid, vid;

    assert(websValid(wp));
    assert(wp->filename && *wp->filename);
    assert(wp->ext && *wp->ext);

    buf = 0;
    vid = -1;
    if ((jid = jsOpenEngine(wp->vars, websJstFunctions)) < 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create JavaScript engine");
        goto done;
    }
    /*
        The engine's global variable frame is the request variable table (wp->vars) which owns its values.
        Run the page script in a variable frame of its own so that script declarations shadow request query
        and form variables instead of replacing and freeing them. Reads still fall back to the global frame
        and assignments to undeclared names still define request variables, so templates are unaffected.
        The frame spans all the <% %> blocks in the page as they share the one engine.
     */
    if ((vid = jsOpenBlock(jid)) < 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create JavaScript variable scope");
        goto done;
    }
    jsSetUserHandle(jid, wp);

    if (websPageStat(wp, &sbuf) < 0) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot stat %s", wp->filename);
        goto done;
    }
    if (websPageOpen(wp, O_RDONLY | O_BINARY, 0666) < 0) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot open URL: %s", wp->filename);
        goto done;
    }
    /*
        Create a buffer to hold the web page in-memory
     */
    if (sbuf.size >= MAXSSIZE) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Template too large: %s", wp->filename);
        goto done;
    }
    len = (ssize) sbuf.size;
    if ((buf = walloc(len + 1)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get memory");
        goto done;
    }
    buf[len] = '\0';

    if (websPageReadData(wp, buf, len) != len) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot read %s", wp->filename);
        goto done;
    }
    websPageClose(wp);
    wp->flags |= WEBS_JST_HEADERS;

    /*
        Scan for the next "<%"
     */
    last = buf;
    for (rc = 0; rc == 0 && *last && ((nextp = strstr(last, "<%")) != NULL); ) {
        websWriteBlock(wp, last, (nextp - last));
        nextp = skipWhite(nextp + 2);
        /*
            Decode the language
         */
        token = "language";
        if ((lang = strtokcmp(nextp, token)) != NULL) {
            if ((cp = strtokcmp(lang, "=javascript")) != NULL) {
                /* Ignore */;
            } else {
                cp = nextp;
            }
            nextp = cp;
        }

        /*
            Find tailing bracket and then evaluate the script
         */
        if ((ep = strstr(nextp, "%>")) != NULL) {

            *ep = '\0';
            last = ep + 2;
            nextp = skipWhite(nextp);
            /*
                Handle backquoted newlines
             */
            for (cp = nextp; *cp; ) {
                if (*cp == '\\' && (cp[1] == '\r' || cp[1] == '\n')) {
                    *cp++ = ' ';
                    while (*cp == '\r' || *cp == '\n') {
                        *cp++ = ' ';
                    }
                } else {
                    cp++;
                }
            }
            if (*nextp) {
                result = NULL;
                /*
                    SECURITY Acceptable: executing embedded <% %> JavaScript from .jst templates is the
                    purpose of the JST engine. Templates are trusted operator-supplied content served from
                    the document root; there is no default remote write primitive (routes enforce methods and
                    PUT/DELETE are not enabled by default), and filesystem integrity is out of scope.
                 */
                if (jsEval(jid, nextp, &result) == 0) {
                    if (result) {
                        trace(4, "JST evaluation failed: %s", result);
                        wfree(result);
                    }
                    jstError(wp);
                    goto done;
                }
                if (wp->finalized) {
                    goto done;
                }
            }

        } else {
            trace(4, "Unterminated JST script");
            jstError(wp);
            goto done;
        }
    }
    /*
        Output any trailing HTML page text
     */
    if (last && *last && rc == 0) {
        websWriteBlock(wp, last, strlen(last));
    }

/*
    Common exit and cleanup
 */
done:
    /*
        Close the page scope before the engine. The caller retains ownership of the global request variables;
        the engine releases any remaining block frames.
     */
    if (jid >= 0) {
        if (vid >= 0) {
            jsCloseBlock(jid, vid);
        }
        jsCloseEngine(jid);
    }
    if (websValid(wp)) {
        websPageClose(wp);
    }
    websDone(wp);
    wfree(buf);
    return 1;
}


static void closeJst(void)
{
    if (websJstFunctions != -1) {
        hashFree(websJstFunctions);
        websJstFunctions = -1;
    }
}


PUBLIC int websJstOpen(void)
{
    websJstFunctions = hashCreate(WEBS_HASH_INIT * 2);
    websDefineJst("write", websJstWrite);
    websDefineJst("writeHtml", jstWriteHtml);
    websDefineJst("escape", jstEscape);
    websDefineJst("escapeUri", jstEscapeUri);
    websDefineJst("escapeJs", jstEscapeJs);
    websDefineHandler("jst", 0, jstHandler, closeJst, 0);
    return 0;
}


/*
    Define a Javascript function. Bind an Javascript name to a C procedure.
 */
PUBLIC int websDefineJst(cchar *name, WebsJstProc fn)
{
    return jsSetGlobalFunctionDirect(websJstFunctions, name, (JsProc) fn);
}


/*
    Javascript write command. This implements <% write("text"); %> command
 */
PUBLIC int websJstWrite(int jid, Webs *wp, int argc, char **argv)
{
    int i;

    assert(websValid(wp));

    for (i = 0; i < argc; ) {
        assert(argv);
        if (websWriteBlock(wp, argv[i], strlen(argv[i])) < 0) {
            return -1;
        }
        if (++i < argc) {
            if (websWriteBlock(wp, " ", 1) < 0) {
                return -1;
            }
        }
    }
    return 0;
}


/*
    Write HTML-escaped arguments separated by spaces.
 */
static int jstWriteHtml(int jid, Webs *wp, int argc, char **argv)
{
    char *escaped;
    int  i;

    for (i = 0; i < argc; i++) {
        escaped = websEscapeHtml(argv[i]);
        if (websWriteBlock(wp, escaped, slen(escaped)) < 0) {
            wfree(escaped);
            return -1;
        }
        wfree(escaped);
        if (i + 1 < argc && websWriteBlock(wp, " ", 1) < 0) {
            return -1;
        }
    }
    return 0;
}


/*
    Return HTML-escaped text.
 */
static int jstEscape(int jid, Webs *wp, int argc, char **argv)
{
    char *escaped;

    if (argc != 1) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "escape() takes one argument");
        return -1;
    }
    escaped = websEscapeHtml(argv[0]);
    jsSetResult(jid, escaped);
    wfree(escaped);
    return 0;
}


/*
    Encode a whole URI while preserving delimiters. HTML-escape the result when writing attributes.
 */
static int jstEscapeUri(int jid, Webs *wp, int argc, char **argv)
{
    char *escaped;

    if (argc != 1) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "escapeUri() takes one argument");
        return -1;
    }
    escaped = websEscapeUri(argv[0]);
    jsSetResult(jid, escaped);
    wfree(escaped);
    return 0;
}



static int jstEscapeJs(int jid, Webs *wp, int argc, char **argv)
{
    char *escaped;

    if (argc != 1) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "escapeJs() takes one argument");
        return -1;
    }
    escaped = websEscapeJs(argv[0]);
    jsSetResult(jid, escaped);
    wfree(escaped);
    return 0;
}


/*
    Find s2 in s1. We skip leading white space in s1.  Return a pointer to the location in s1 after s2 ends.
 */
static char *strtokcmp(char *s1, char *s2)
{
    ssize len;

    s1 = skipWhite(s1);
    len = strlen(s2);
    for (len = strlen(s2); len > 0 && (tolower((uchar) *s1) == tolower((uchar) *s2)); len--) {
        if (*s2 == '\0') {
            return s1;
        }
        s1++;
        s2++;
    }
    if (len == 0) {
        return s1;
    }
    return NULL;
}


static char *skipWhite(char *s)
{
    assert(s);

    if (s == NULL) {
        return s;
    }
    while (*s && isspace((uchar) *s)) {
        s++;
    }
    return s;
}

#endif /* ME_GOAHEAD_JAVASCRIPT */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
