/*
    uripath-control-chars.tst.c - Regression: a decoded control character must not reach wp->path.

    websValidUriChars runs on the raw request-line URI and must permit '%', so "%0A" passes it.
    websDecodeUrl then turns that into a real newline, and validCanonicalPath's per-character
    fall-through accepted every byte below 0x80 - control bytes included. The accepted string
    becomes wp->path, which the access log writes as one line of a Common Log Format record and
    websRedirect passes unescaped into a Location header value. A newline there forges a log record
    or splits the response.

    The choke point both sinks share is websValidateUriPath, so that is what this asserts: no
    accepted path may contain a byte below 0x20 or DEL, whatever encoding was used to smuggle it.
 */

#include "testme.h"
#include "goahead.h"

/*
    Every input that must be refused outright. Each smuggles a control byte through percent-encoding
    in a different position - alone, paired as CRLF, mid-segment, and at the top of the control range.
 */
static cchar *rejected[] = {
    "/%0A",                             /* LF */
    "/%0D",                             /* CR */
    "/%0d%0a",                          /* CRLF, lowercase hex */
    "/%09",                             /* TAB */
    "/%1F",                             /* last C0 control */
    "/%01",                             /* SOH */
    "/%7F",                             /* DEL */
    "/%7Fx",                            /* DEL followed by ordinary text */
    "/a%0Ab",                           /* mid-segment LF */
    "/dir%0A/index.html",               /* LF at a segment boundary */
    "/%0Aadmin",                        /* the forged-log-record shape from the report */
    "/%00",                             /* NUL: left encoded by the decoder, so refused as an escape */
    "/%2500",                           /* double-encoded NUL, which decodes to the same escape */
    NULL
};

/*
    Paths that must keep working. The fix must reject control characters and nothing else: ordinary
    text, a percent-encoded printable, an encoded space, and multi-byte UTF-8 all stay accepted.
 */
static cchar *accepted[] = {
    "/",
    "/index.html",
    "/%69ndex.html",                    /* percent-encoded 'i' - decoding still happens */
    "/a%20b/c",                         /* an encoded space is legal away from a '/' */
    "/caf%C3%A9/",                      /* UTF-8 above 0x80 is not a control byte */
    "/a.b/c-d_e~f",
    NULL
};

/*
    True when the string holds no ASCII control character. This is the property the two sinks
    depend on, asserted directly rather than inferred from the accept/reject verdict.
 */
static int isControlFree(cchar *path)
{
    cchar *cp;

    for (cp = path; *cp; cp++) {
        if ((uchar) *cp < 0x20 || (uchar) *cp == 0x7F) {
            return 0;
        }
    }
    return 1;
}


int main(int argc, char **argv)
{
    char *path;
    int  i;

    tinfo("uripath-control-chars: a decoded control character must never be accepted\n");

    for (i = 0; rejected[i]; i++) {
        path = websValidateUriPath(rejected[i]);
        ttrue(path == 0, "[%s] must be rejected", rejected[i]);
        if (path) {
            /*
                Report the property that was violated, not only the verdict - an accepted path
                carrying a control byte is the defect itself.
             */
            ttrue(isControlFree(path), "[%s] was accepted carrying a control character", rejected[i]);
            wfree(path);
        }
    }

    for (i = 0; accepted[i]; i++) {
        path = websValidateUriPath(accepted[i]);
        ttrue(path != 0, "[%s] must still be accepted", accepted[i]);
        if (path) {
            ttrue(isControlFree(path), "[%s] must decode to a control-free path", accepted[i]);
            wfree(path);
        }
    }

    /*
        Decoding must still happen for the accepted cases - a validator that rejected nothing by
        refusing to decode at all would otherwise pass every assertion above.
     */
    path = websValidateUriPath("/%69ndex.html");
    ttrue(path != 0 && smatch(path, "/index.html"), "percent-decoding must still be applied");
    wfree(path);

    return 0;
}
